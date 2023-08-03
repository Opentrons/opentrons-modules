
from dataclasses import dataclass
import serial
import time
import re
import argparse
import io
import os 
import datetime

from serial.tools.list_ports import grep
from typing import  Dict, Tuple, List, Optional

def parse_args():
    parser = argparse.ArgumentParser(description="Run a simple PCR cycle")
    parser.add_argument('-p', '--port', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    parser.add_argument('-f', '--file', type=argparse.FileType('w'), required=False, default=None, help='file to log temperature to')
    parser.add_argument('setpoints', metavar='setpoints', type=float, nargs='+',
                        help="Define a series of setpoints to move to. Each setpoint is two "
                             "numbers, the temperature (in C) and the hold time (in seconds). "
                             "For example, to hold 4 for 10 minutes and then 98 for 1 minute, "
                             "you would pass in:  4 600 98 60")

    return parser.parse_args()

def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*hermocycler*'))
        if not avail:
            raise RuntimeError("could not find thermocycler")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)

class Thermocycler():
    def __init__(self, file: Optional[io.TextIOWrapper] = None, port: str = None):
        '''
        Constructs a new Thermocycler. Leave `port` empty to connect to
        the first Thermocycler connected over USB.
        '''
        self.ser = build_serial(port)
        if file is not None:
            self.file = file
        else:
            serial = self.get_serial()
            timestamp = datetime.datetime.now()
            filepath = os.path.join(os.getcwd(), f'{serial}-hold-temperatures-{timestamp}.csv')
            self.file = open(filepath, 'w', newline='\n')
        self.file.write('time,HST,FRT,FLT,FCT,BRT,BLT,BCT,left,center,right,fans\n')
        self.start = time.time()
    
    _POLL_FREQ = 1
    '''Poll frequency in Hz'''

    @dataclass
    class Thermistors:
        """Class to encapsulate thermistor temperatures"""
        hs: float # heat sink
        fr: float # front right
        fc: float # front center
        fl: float # front left
        br: float # back right
        bc: float # back center
        bl: float # back left
        
        def __str__(self):
            return f'{self.hs},{self.fr},{self.fl},{self.fc},{self.br},{self.bl},{self.bc}'
        
    @dataclass
    class Power:
        """Class to encapsulate peltier power values"""
        left:   float
        center: float
        right:  float
        fans: float
        
        def __str__(self):
            return f'{self.left},{self.center},{self.right},{self.fans}'

    def _send_and_recv(self, msg: str, guard_ret: str = None) -> str:
        '''Internal utility to send a command and receive the response'''
        self.ser.write(msg.encode())
        ret = self.ser.readline()
        if guard_ret:
            if not ret.startswith(guard_ret.encode()):
                raise RuntimeError(f'Incorrect Response: {ret}')
        if ret.startswith('ERR'.encode()):
            raise RuntimeError(ret)
        return ret.decode()

    _VERSION_RE = re.compile('M115 FW:.* HW:.* SerialNo:(?P<SERIAL>.+) OK\n')
    def get_serial(self) -> str:
        """Gets the serial number."""
        response = self._send_and_recv('M115\n', 'M115 FW:')
        match = re.match(self._VERSION_RE, response)
        return str(match.group('SERIAL'))


    _TEMP_DEBUG_RE = re.compile('^M105.D HST:(?P<HST>.+) FRT:(?P<FRT>.+) FLT:(?P<FLT>.+) FCT:(?P<FCT>.+) BRT:(?P<BRT>.+) BLT:(?P<BLT>.+) BCT:(?P<BCT>.+) HSA.* OK\n')
    def get_plate_thermistors(self) -> Thermistors:
        """
        Gets each thermistor on the plate.
        """
        res = self._send_and_recv('M105.D\n', 'M105.D HST:')
        match = re.match(self._TEMP_DEBUG_RE, res)
        fr = float(match.group('FRT'))
        br = float(match.group('BRT'))
        fc = float(match.group('FCT')) 
        bc = float(match.group('BCT'))
        fl = float(match.group('FLT')) 
        bl = float(match.group('BLT'))
        hs = float(match.group('HST'))
        return Thermocycler.Thermistors(hs, fr, fc, fl, br, bc, bl)
    
    _POWER_RE = re.compile('^M103.D L:(?P<left>.+) C:(?P<center>.+) R:(?P<right>.+) H:(?P<heater>.+) F:(?P<fans>.+) T1:(?P<t1>.+) T2:(?P<t2>.+) OK\n')
    def get_peltier_power(self) -> Power:
        """
        Get the power of all peltiers.
        """
        res = self._send_and_recv('M103.D\n', 'M103.D L:')
        match = re.match(self._POWER_RE, res)
        left = float(match.group('left'))
        center = float(match.group('center'))
        right = float(match.group('right'))
        fans = float(match.group('fans'))
        return Thermocycler.Power(left, center, right, fans)

    _PLATE_TEMP_RE = re.compile('^M105 T:(?P<target>.+) C:(?P<temp>.+) H:(?P<hold>.+) Total_H:(?P<total_hold>.+) At_target\?:(?P<at_target>.+) OK\n')
    
    def get_plate_temperature(self) -> Tuple[float, float, bool]:
        '''
        Get the temperature of the plate.

        Returns Tuple[temperature, remaining hold time, at_target]
        '''
        res = self._send_and_recv('M105\n', 'M105 T:')
        match = re.match(self._PLATE_TEMP_RE, res)
        at_target = int(match.group('at_target')) == 1
        return float(match.group('temp')), float(match.group('hold')), at_target
    
    _LID_TEMP_RE = re.compile('^M141 T:(?P<target>.+) C:(?P<temp>.+) OK\n')
    def get_lid_temperature(self) -> Tuple[float, float]:
        '''
        Gets the current temperature of the lid heater.
        
        Returns Tuple[temperature, target_temperature]
        '''
        res = self._send_and_recv('M141\n', 'M141 T:')
        match = re.match(self._LID_TEMP_RE, res)
        temp = float(match.group('temp'))
        try:
            target = float(match.group('target'))
        except ValueError:
            target = None
        return temp, target

    def add_log_entry(self) -> None:
        """Write a log entry to the file."""
        temps = self.get_plate_thermistors()
        power = self.get_peltier_power()
        now = time.time() - self.start 
        self.file.write(f'{now},{str(temps)},{str(power)}\n')
        self.file.flush()
    
    def set_plate_target(self, temperature: float, hold_time: float = 0, volume: float = None):
        '''
        Sets the target temperature of the thermal plate. The temperature is required,
        but the hold_time and volume parameters may be left as defaults.
        '''
        send = f'M104 S{temperature} H{hold_time}'
        if(volume):
            send = send + f' V{volume}'
        send = send + '\n'
        self._send_and_recv(send, 'M104')
    
    def set_lid_target(self, temperature: float = None):
        '''
        Sets the lid heater temperature target. Leave `temperature` empty to
        set the target to the default of 105ºC.
        '''
        send = 'M140'
        if(temperature):
            send = send + f' S{temperature}'
        send = send + '\n'
        self._send_and_recv(send, 'M140 OK')

    def set_block_temperature(self, temperature: float, hold_time: float = 0, volume: float = None):
        '''
        Commands the thermocycler to move the peltier block to temperature, and then
        waits for the hold time to expire.
        '''
        self.set_plate_target(temperature, hold_time, volume)
        done = False
        # Poll for remaining time
        while not done:
            time.sleep(1 / self._POLL_FREQ)
            temp, remaining_time, at_target = self.get_plate_temperature()
            self.add_log_entry()
            if (remaining_time < 0.1) and at_target:
                done = True
    
    def set_lid_temperature(self, temperature: float = None):
        '''
        Commands the thermocycler to move the lid to a temperature, and then
        waits for the temperature to be reached
        '''
        self.set_lid_target(temperature)
        done = False 
        # Poll for remaining time
        while not done:
            time.sleep(1/ self._POLL_FREQ)
            temp, target = self.get_lid_temperature()
            if abs(temp - target) < 2.0:
                done = True

    def execute_profile(self, steps: List[Dict], cycles: int = 1, volume: float = None):
        '''
        Executes a thermal profile multiple times in a row.

        `Steps` should be a list of dictionary items where each step
        contains members 'temperature' and 'hold_time_seconds'

        `cycles` is the number of times to repeat the cycle
        '''
        if(cycles < 1):
            raise RuntimeError('cycles must be a positive integer')
        for i in range(cycles):
            print(f'Beginning cycle {i+1}')
            for step in steps:
                temp = step['temperature']
                hold = step['hold_time_seconds']
                print(f'  Moving to {temp}ºC for {hold} seconds')
                self.set_block_temperature(temp, hold, volume)

    def deactivate_lid(self):
        '''Turn off the lid heater'''
        self._send_and_recv('M108\n', 'M108 OK')

    def deactivate_plate(self):
        '''Turn off the peltiers'''
        self._send_and_recv('M14\n', 'M14 OK')

    def deactivate_all(self):
        '''Turn off the lid heater and the peltiers'''
        self._send_and_recv('M18\n', 'M18 OK')
    
    def open_lid(self):
        '''Opens the lid and blocks'''
        self._send_and_recv('M126\n', 'M126 OK')

    def close_lid(self):
        '''Closes the lid and blocks'''
        self._send_and_recv('M127\n', 'M127 OK')

@dataclass 
class Setpoint:
    temperature: float 
    hold_seconds: float

def get_setpoints(args: List[float]) -> List[Setpoint]:
    ret = []
    while len(args) > 0:
        temp = args.pop(0)
        hold = args.pop(0)
        ret.append(Setpoint(temperature=temp, hold_seconds=hold))
    return ret

if __name__ == '__main__':
    args = parse_args()

    if len(args.setpoints) == 0:
        print('There needs to be at least one setpoint.')
        exit(-1)
    if len(args.setpoints) % 2 != 0:
        print(f'Setpoints must have a temperature and time for each setpoint.')
        exit(-1)

    setpoints = get_setpoints(args.setpoints)
    print(f'There are {len(setpoints)} steps:')
    for i, setpoint in enumerate(setpoints):
        print(f'  Step {i+1}:\thold {setpoint.temperature}C for {setpoint.hold_seconds} seconds')
    print()

    thermocycler = Thermocycler(port=args.port, file=args.file)
    print(f'Logging all data to: {thermocycler.file.name}')
    print()
    
    try:
        for i, setpoint in enumerate(setpoints):
            print(f'Starting step {i+1}:\tholding {setpoint.temperature}C for {setpoint.hold_seconds} seconds')
            thermocycler.set_block_temperature(temperature=setpoint.temperature, hold_time=setpoint.hold_seconds)

        print('Done!')
        thermocycler.deactivate_all()
    except KeyboardInterrupt:
        # If the script is ended early, turn the thermocycler off
        # for safety. When the script ends normally, it is okay for
        # the plate to be kept at 4ºC indefinitely.
        print(f'Ending early')
        print('Turning off Thermocycler')
        thermocycler.deactivate_all()
    except RuntimeError:
        # Same error handling if the thermocycler throws an error
        print(f'Ending early due to error')
        print('Turning off Thermocycler')
        thermocycler.deactivate_all()


import serial
import time
import re
import argparse

from serial.tools.list_ports import grep
from typing import  Dict, Tuple, List

def parse_args():
    parser = argparse.ArgumentParser(description="Run a simple PCR cycle")
    parser.add_argument('-v', '--volume', type=float, required=False, 
                        default=25.00, help='Volume of liquid (in uL)')
    parser.add_argument('-c', '--cycles', type=int, required=False,
                        default=35, help='Number of cycles to run')
    parser.add_argument('-p', '--port', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    parser.add_argument('-d', '--debug', required=False, action='store_true',
                        help='Enable debugging print outputs with the temperature')
    return parser.parse_args()

def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*hermocycler*'))
        if not avail:
            raise RuntimeError("could not find thermocycler")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)

class Thermocycler():
    def __init__(self, port: str = None, debug: bool = False):
        '''
        Constructs a new Thermocycler. Leave `port` empty to connect to
        the first Thermocycler connected over USB.
        '''
        self.ser = build_serial(port)
        self.debug = debug
    
    
    _POLL_FREQ = 2.0 
    '''Poll frequency in Hz'''

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
            if(self.debug):
                print(f'Temp: {temp} Remaining: {remaining_time}')
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
            if(self.debug):
                print(f'Lid: {temp} Target: {target}')
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

if __name__ == '__main__':
    args = parse_args()

    thermocycler = Thermocycler(port=args.port, debug=args.debug)
    
    try:
        thermocycler.open_lid()
        input('Load the wellplate into the module and then press Enter to continue...')
        print('Setting block to 4ºC')
        thermocycler.set_block_temperature(4)
        print('Closing lid')
        thermocycler.close_lid()
        print('Preheating lid to 105ºC')
        thermocycler.set_lid_temperature(105)
        print('Preheating block to 95ºC for 3 minutes')
        thermocycler.set_block_temperature(95, hold_time=60*3)

        steps = [
            {'temperature': 70, 'hold_time_seconds': 30},
            {'temperature': 72, 'hold_time_seconds': 30},
            {'temperature': 95, 'hold_time_seconds': 10}
        ]
        thermocycler.execute_profile(steps, args.cycles, args.volume)

        print('Setting block to 72º for 5 minutes')
        thermocycler.set_block_temperature(72, hold_time=5*60)
        print('Cooling block to 4º')
        thermocycler.set_block_temperature(4)

        thermocycler.open_lid()
        print('Done!')
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

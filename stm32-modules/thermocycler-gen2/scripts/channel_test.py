#! python3

import serial
import time
import re
import argparse
import datetime

from serial.tools.list_ports import grep
from typing import  Dict, Tuple, List

def parse_args():
    parser = argparse.ArgumentParser(description="Run a simple PCR cycle")
    parser.add_argument('-p', '--port', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    parser.add_argument('-f', '--file', type=argparse.FileType('w'), required=False, default=None,
                        help='File to write data to. If not specified, a new file will be generated.')
    parser.add_argument('-t', '--time', type=float, required=False, default=180,
                        help='Number of seconds to measure data for each temperature')
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

    class Thermistors():
        def __init__(self, hs, fr, fc, fl, br, bc, bl):
            self.hs = hs # heat sink
            self.fr = fr # front right
            self.fc = fc # front center
            self.fl = fl # front left
            self.br = br # back right
            self.bc = bc # back center
            self.bl = bl # back left
        
        def __str__(self):
            return f'{self.hs},{self.fr},{self.fc},{self.fl},{self.br},{self.bc},{self.bl}'
        
    class Power():
        def __init__(self, left, center, right):
            self.left = left 
            self.center = center 
            self.right = right 
        
        def __str__(self):
            return f'{self.left},{self.center},{self.right}'

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

    _TEMP_DEBUG_RE = re.compile('^M105.D HST:(?P<HST>.+) FRT:(?P<FRT>.+) FLT:(?P<FLT>.+) FCT:(?P<FCT>.+) BRT:(?P<BRT>.+) BLT:(?P<BLT>.+) BCT:(?P<BCT>.+) HSA.* OK\n')
    def get_plate_thermistors(self) -> Thermistors:
        '''
        Gets each thermistor on the plate.
        '''
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
    
    _POWER_RE = re.compile('^M103.D L:(?P<left>.+) C:(?P<center>.+) R:(?P<right>.+) H:(?P<heater>.+) F:(?P<fans>.+) OK\n')
    def get_peltier_power(self) -> Power:
        '''
        Get the power of all peltiers.
        '''
        res = self._send_and_recv('M103.D\n', 'M103.D L:')
        match = re.match(self._POWER_RE, res)
        left = float(match.group('left'))
        center = float(match.group('center'))
        right = float(match.group('right'))
        return Thermocycler.Power(left, center, right)
    
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

    thermocycler = Thermocycler(port=args.port)

    file = args.file
    if not file:
        timestamp = datetime.datetime.now()
        file = open(f'./channel-test-{timestamp}.csv', 'w', newline='\n')

    print('Writing to file ' + file.name)
    
    try:
        temperatures = [94.0, 70.0, 50.0, 4.0]
        start_time = time.time()
        file.write('time,target,heatsink,front right,front center,front left,back right,back center,back left,left power,center power,right power')
        file.write('\n')
        for temp in temperatures:
            thermocycler.set_plate_target(temp)
            temp_start_time = time.time()
            while time.time() < (temp_start_time + args.time):
                thermistors = thermocycler.get_plate_thermistors()
                power = thermocycler.get_peltier_power()
                measurement_time = time.time()
                seconds = measurement_time - start_time
                toprint = f'{seconds},{temp},' + str(thermistors) + ',' + str(power)
                file.write(toprint)
                file.write('\n')
                time.sleep(0.5)
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

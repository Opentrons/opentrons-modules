#! python3

'''
This script sets the lid to 105C and the block to 70C, and then prints
temperature readings every 3 seconds to confirm the Thermocycler is on.
'''

import serial
import time
import re
import argparse

from serial.tools.list_ports import grep
from typing import Tuple

def parse_args():
    parser = argparse.ArgumentParser(description="Basic ESD testing script")
    parser.add_argument('-p', '--port', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    return parser.parse_args()

def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*hermocycler*'))
        if not avail:
            raise RuntimeError("could not find thermocycler")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)


class Thermocycler():
    '''Represents a connection to a physical Thermocycler unit'''
    def __init__(self, port: str = None):
        '''
        Constructs a new Thermocycler. Leave `port` empty to connect to
        the first Thermocycler connected over USB.
        '''
        self.ser = build_serial(port)
    
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

    def set_plate_target(self, temperature: float):
        '''
        Sets the target temperature of the thermal plate. The temperature is required,
        but the hold_time and volume parameters may be left as defaults.
        '''
        send = f'M104 S{temperature}\n'
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

    def deactivate_all(self):
        '''Turn off the lid heater and the peltiers'''
        self._send_and_recv('M18\n', 'M18 OK')


def main():
    args = parse_args()

    tc = Thermocycler(port=args.port)

    try:
        tc.set_lid_target(105)
        print('Set lid to 105C')
        tc.set_plate_target(70)
        print('Set plate to 70C')
        start = time.time()
        while True:
            time.sleep(3)
            lid = tc.get_lid_temperature()[0]
            plate = tc.get_plate_temperature()[0]
            now = time.time()
            print(f'{now - start}\tLid: {lid}\tPlate: {plate}')
    except KeyboardInterrupt:
        print('Ending script')
        print('')
    finally:
        print('Deactivating Thermocycler')
        tc.deactivate_all()

if __name__ == '__main__':
    main()

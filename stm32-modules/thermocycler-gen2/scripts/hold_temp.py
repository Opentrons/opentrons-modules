#! python3

'''
Script to cycle an arbitrary set of temperatures
'''
from dataclasses import dataclass
import serial
import argparse
import re
import time
import datetime

from serial.tools.list_ports import grep
from typing import  Dict, Tuple, List

def parse_args():
    parser = argparse.ArgumentParser(description="Hold temperature")
    parser.add_argument('-t', '--temp', type=float, required=True, help='Target temp')
    parser.add_argument('--hold', type=float, required=False, default=None, help='Hold time')
    parser.add_argument('--port', type=str, required=False, default=None, help='Port to use (comport or tty path)')
    parser.add_argument('-f', '--file', type=argparse.FileType('w'), required=False, default=None, help='file to log temperature to')
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
        """
        Constructs a new Thermocycler. Leave `port` empty to connect to
        the first Thermocycler connected over USB.
        """
        self.ser = build_serial(port)
        self.debug = debug
    
    
    _POLL_FREQ = 2.0 
    """Poll frequency in Hz"""

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
            return f'{self.hs},{self.fr},{self.fc},{self.fl},{self.br},{self.bc},{self.bl}'
        
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
        """Internal utility to send a command and receive the response"""
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
    
    def set_plate_target(self, temperature: float, hold_time: float = 0, volume: float = None):
        """
        Sets the target temperature of the thermal plate. The temperature is required,
        but the hold_time and volume parameters may be left as defaults.
        """
        send = f'M104 S{temperature} H{hold_time}'
        if(volume):
            send = send + f' V{volume}'
        send = send + '\n'
        self._send_and_recv(send, 'M104')
    
    def deactivate_all(self):
        """Turn off the lid heater and the peltiers"""
        self._send_and_recv('M18\n', 'M18 OK')
    
    def open_lid(self):
        """Opens the lid and blocks"""
        self._send_and_recv('M126\n', 'M126 OK')

    def close_lid(self):
        """Closes the lid and blocks"""
        self._send_and_recv('M127\n', 'M127 OK')

def main():
    args = parse_args()

    target = args.temp
    
    tc = Thermocycler(port=args.port)

    file = args.file
    if not file:
        timestamp = datetime.datetime.now()
        file = open(f'./tc-hold-{timestamp}.csv', 'w', newline='\n')
    
    if args.hold:
        print(f'Holding for {args.hold} seconds')
    else:
        print('Holding indefinitely')

    try:
        start = time.time()
        file.write('time,HST,FRT,FLT,FCT,BRT,BLT,BCT,left,center,right,fans\n')
        tc.set_plate_target(target)
        while(True):
            temps = tc.get_plate_thermistors()
            power = tc.get_peltier_power()
            now = time.time() - start 
            msg = f'{now},{str(temps)},{str(power)}' + '\n'
            file.write(msg)
            file.flush()
            if args.hold and now > args.hold:
                print('Hold time reached')
                break
            time.sleep(1)

    finally:
        print('Ending script')
        tc.deactivate_all()

if __name__ == '__main__':
    main()

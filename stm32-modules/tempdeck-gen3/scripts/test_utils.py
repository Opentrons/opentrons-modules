import re
from serial.tools.list_ports import grep
import serial

from typing import  Dict, Tuple, List


def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*empdeck*'))
        if not avail:
            raise RuntimeError("could not find tempdeck")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)

class Tempdeck():
    def __init__(self, port: str = None, debug: bool = False):
        '''
        Constructs a new Tempdeck. Leave `port` empty to connect to the
        first Tempdeck connected over USB.
        '''
        self._ser = build_serial(port)
        self._debug = debug

    def _send_and_recv(self, msg: str, guard_ret: str = None) -> str:
        '''Internal utility to send a command and receive the response'''
        self._ser.write(msg.encode())
        if self._debug:
            print(f'Sending: {msg}')
        ret = self._ser.readline()
        if guard_ret:
            if not ret.startswith(guard_ret.encode()):
                raise RuntimeError(f'Incorrect Response: {ret}')
        if ret.startswith('ERR'.encode()):
            raise RuntimeError(ret)
        if self._debug:
            print(f'Received: {ret.decode()}')
        return ret.decode()

    _TEMP_RE = re.compile('^M105.D PT:(?P<plate>.+) HST:(?P<heatsink>.+) PA:(.+) HSA:(.+) OK\n')
    
    def get_temperatures(self) -> Tuple[float, float]:
        '''
        Get the temperatures of the system. Returns a tuple
        of [plate, heatsink]
        '''
        res = self._send_and_recv('M105.D\n', 'M105.D PT:')
        match = re.match(self._TEMP_RE, res)
        plate = float(match.group('plate'))
        heatsink = float(match.group('heatsink'))
        return plate, heatsink

    def set_peltier_power(self, power: float = 0):
        '''
        Set the output PWM of the peltiers. Positive values heat and
        negative values cool. The range is [-1,1]
        '''
        send = f'M104.D S{power}\n'
        self._send_and_recv(send, 'M104.D OK')

    def set_fan_power(self, power: float = 0):
        '''
        Set the fan output PWM
        '''
        send = f'M106 S{power}\n'
        self._send_and_recv(send, 'M106 OK')
    
    def set_fan_automatic(self):
        '''
        Set the fan to automatic mode
        '''
        self._send_and_recv('M107\n', 'M107 OK')

    def dfu(self):
        '''
        Enter the bootloader. This will cause future comms to fail.
        '''
        self._ser.write(b'dfu\n')

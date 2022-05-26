#! python 3

from dataclasses import dataclass
from numpy import average
import serial
import time
import re
import argparse
import numpy as np
import datetime

from serial.tools.list_ports import grep
from typing import  Dict, Tuple, List

def parse_args():
    parser = argparse.ArgumentParser(description="Calculate and program thermocycler offset coefficients")
    parser.add_argument('-t', '--thermocycler', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    parser.add_argument('-p', '--probe', type=str, required=True, default=None,
                        help='The USB port that the multiprobe is connected to')
    parser.add_argument('-c', '--check', required=False, action='store_true',
                        help='Run through the temperature settings again after the test to check data')
    parser.add_argument('-f', '--file', type=argparse.FileType('w'), required=False, default=None,
                        help='File to write data to. If not specified, a new file will be generated.')
    return parser.parse_args()

class Multiprobe():
    """A class to communicate with the Eutechnics 4690 thermometer"""
    def __init__(self, port: str = None):
        self.ser = serial.Serial(port, 
                                 baudrate = 115200,
                                 timeout=2,
                                 parity = serial.PARITY_NONE,
                                 stopbits = serial.STOPBITS_ONE,
                                 bytesize = serial.EIGHTBITS)
        # Make sure the probe is connected right
        self.get_reading(retries=3)
    
    @dataclass 
    class Readings:
        """Class to encapsulate readings from the multiprobe wells"""
        h1: float  # Well H1
        a1: float  # Well A1
        f4: float  # Well F4
        c4: float  # Well C4 
        f9: float  # Well F9 
        c9: float  # Well C9 
        h12: float # Well H12
        a12: float # Well A12

        def left_temp(self) -> float:
            return average([self.h1, self.a1])

        def center_temp(self) -> float:
            return average([self.f4, self.c4, self.f9, self.c9])

        def right_temp(self) -> float:
            return average([self.h12, self.a12])

        def as_list(self) -> List[float]:
            return [self.h1, self.a1, self.f4, self.c4, self.f9, self.c9, self.h12, self.a12]

        def uniformity(self) -> float:
            temps = self.as_list()
            return (max(temps) - min(temps)) / 2

        def average(self) -> float:
            return average(self.as_list())

        def to_csv(self) -> str:
            ret = ''
            for well in self.as_list():
                ret += f'{well},'
            return ret
    
    def get_reading(self, retries=3) -> Readings:
        """Get a temperature reading from the probe"""
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        line_ready = False 
        data = []
        # Clear out the possible last reading
        self.ser.read_until(expected=b'\r')
        while not line_ready:
            reading = self.ser.read_until(expected=b'\r')
            if reading != '':
                data = reading.decode().strip().split()
                if len(data) == 9:
                    line_ready = True
            retries = retries - 1
            if retries < 1:
                msg = 'Multiprobe is not sending data correctly.\n'
                msg += 'Check that the probe is connected and that the mode is set to Average.'
                raise serial.SerialException(msg)
        # Multiprobe is upside down from what is expected
        return Multiprobe.Readings(
            h12=float(data[0]), a12=float(data[1]), 
            c9=float(data[2]), f9=float(data[3]), 
            c4=float(data[4]), f4=float(data[5]), 
            a1=float(data[6]), h1=float(data[7]) )

class Thermocycler():
    def __init__(self, port: str = None, debug: bool = False):
        """
        Constructs a new Thermocycler. Leave `port` empty to connect to
        the first Thermocycler connected over USB.
        """
        self.ser = Thermocycler.build_serial(port)
        self.debug = debug
    

    def build_serial(cls, port: str = None) -> serial.Serial:
        if not port:
            avail = list(grep('.*hermocycler*'))
            if not avail:
                raise RuntimeError("could not find thermocycler")
            return serial.Serial(avail[0].device, 115200)
        return serial.Serial(port, 115200)

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

    def set_coefficients(self, channel: str = None, A: float = None, B: float = None, C: float = None):
        """Sets the offset coefficients for either all channels or a single channel"""
        message = 'M116'
        if channel:
            message = message + '.' + channel
        if A != None:
            message = message + f' A{A}'
        if B != None:
            message = message + f' B{B}'
        if C != None:
            message = message + f' C{C}'
        message = message + '\n'
        self._send_and_recv(message, 'M116 OK')

def calculate_coefficients(targets, offsets) -> Tuple[float, float]:
    """Returns a tuple of (slope,offset)"""
    b, c = np.polynomial.Polynomial.fit(targets, offsets, 1).convert().coef
    return c, b

if __name__ == '__main__':
    args = parse_args()

    thermocycler = Thermocycler(port=args.thermocycler)
    probe = Multiprobe(port=args.probe)

    file = args.file
    if not file:
        timestamp = datetime.datetime.now()
        file = open(f'./thermocycler-gen2-cal-{timestamp}.csv', 'w', newline='\n')

    print('Thermocycler coefficients: ' + thermocycler._send_and_recv('M117\n'))
    print('Clearing B and C offset coefficients...')
    thermocycler.set_coefficients(B = 0, C = 0)

    target_temps = [4.0, 50.0, 70.0, 94.0]
    left_offsets =[]
    center_offsets = []
    right_offsets = []
    MINUTES = 3
    csv_data = []

    csv_data.append('target,calibrated,h1,a1,f4,c4,f9,c9,h12,a12,average,uniformity')

    for temperature in target_temps:
        print('')
        print(f'Moving to {temperature}ºC and waiting {MINUTES} minutes...')
        thermocycler.set_plate_target(temperature)
        start = time.time()
        while time.time() < start + (MINUTES*60):
            # Keep reading the probe to keep its data current
            probe.get_reading()

        readings = probe.get_reading()

        print(f'Probe readings: Left={readings.left_temp()} Center={readings.center_temp()} Right={readings.right_temp()}')
        print(f'Average:    {readings.average()}')
        print(f'Uniformity: {readings.uniformity()}')

        left_offsets.append(readings.left_temp() - temperature)
        center_offsets.append(readings.center_temp() - temperature)
        right_offsets.append(readings.right_temp() - temperature)

        csv_data.append(f'{temperature},NO,{readings.to_csv()}{readings.average()},{readings.uniformity()}')

    thermocycler.deactivate_all()

    # Calulate coefficients and DONE
    bl, cl = calculate_coefficients(target_temps, left_offsets)
    bc, cc = calculate_coefficients(target_temps, center_offsets)
    br, cr = calculate_coefficients(target_temps, right_offsets)

    print('')
    print(f'Done calculating offsets!')
    print(f'   bl={bl}, cl={cl}')
    print(f'   bc={bc}, cc={cc}')
    print(f'   br={br}, cr={cr}')

    thermocycler.set_coefficients('L', B=bl, C=cl)
    thermocycler.set_coefficients('C', B=bc, C=cc)
    thermocycler.set_coefficients('R', B=br, C=cr)

    print('')
    print('Coefficients have been updated!')

    if args.check:
        print('')
        print('Re-running temperatures to check constants...')
        for temperature in target_temps:
            print('')
            print(f'Moving to {temperature}ºC and waiting {MINUTES} minutes...')
            thermocycler.set_plate_target(temperature)
            start = time.time()
            while time.time() < start + (MINUTES*60):
                # Keep reading the probe to keep its data current
                probe.get_reading()

            readings = probe.get_reading()
            print(f'Probe readings: Left={readings.left_temp()} Center={readings.center_temp()} Right={readings.right_temp()}')
            print(f'Average:    {readings.average()}')
            print(f'Uniformity: {readings.uniformity()}')
            csv_data.append(f'{temperature},YES,{readings.to_csv()}{readings.average()},{readings.uniformity()}')
        
    
    thermocycler.deactivate_all()
    
    print('')
    print(f'Writing data to {file.name}')
    for line in csv_data:
        file.write(line)
        file.write('\n')

    print('Done!')

#!/usr/bin/env python3

# Script to get the step responses from the system
import test_utils 
import serial
import datetime
import time
import csv
from typing import Callable, Tuple, List

class StepResponse:
    def __init__(
        self,
        # Input serial port and power to set (power scaled 0.0 to 1.0)
        set_power: Callable[[serial.Serial, float], any],
        # Input serial port, output current reading
        update: Callable[[serial.Serial], float],
        # power level as a float
        power_level: float,
        # Target temp as a float
        target: float,
        timeout: float,
        period: float):

        self.set_power = set_power
        self.update = update
        self.power_level = power_level
        self.timeout = timeout
        self.period = period
        self.target = target
        self.start_timestamp = time.time()
        self.last_reading = 0.0
        self.last_timestamp = time.time()
        self.current_power = 0
        self.data = []
    
    def run(self, ser: serial.Serial):
        # Start with power off
        self.set_power(ser, 0)
        self.current_power = 0
        self.iteration = 0
        test_utils.sample_until_condition(
            ser, self.period, self.sampler, self.done)
    
    def sampler(self, ser: serial.Serial):
        self.iteration = self.iteration + 1
        if self.iteration == 10:
            self.set_power(ser, self.power_level)
            self.current_power = self.power_level

        self.last_reading = self.update(ser)
        self.last_timestamp = time.time()
        self.data.append([
            self.last_timestamp - self.start_timestamp,
            self.current_power,
            self.last_reading])
    
    def done(self, ser: serial.Serial) -> bool:
        elapsed = self.last_timestamp - self.start_timestamp
        if elapsed > self.timeout:
            print('Timeout has elapsed')
            return True 
        if self.last_reading > self.target:
            print('Target has been reached')
            return True

# Data format:
#    time (seconds), power (percent), reading (temperature)
def write_csv(filename: str, data: List[Tuple[float, float, float]]):
    timestamp = datetime.datetime.now()
    output_file = open(f'./thermocycler-gen2-{filename}-{timestamp}.csv', 'w')
    writer = csv.writer(output_file)
    writer.writerows(
        [['title', f'thermocycler gen2 {filename}'],
        ['taken at', str(timestamp)]])
    writer.writerow(
        ['time', 'power', 'reading'])
    writer.writerows(data)

def set_whole_power(ser: serial.Serial, power: float):
    direction = 'C'
    if power > 0:
        direction = 'H'
    test_utils.set_peltier_debug(abs(power), direction, 'A', ser)

def get_whole_temperature(ser: serial.Serial):
    ret = test_utils.get_plate_temperature(ser)
    print(f'Temp reading: {ret}')
    return ret

def set_lid_power(ser: serial.Serial, power: float):
    test_utils.set_heater_debug(power, ser)

def get_lid_temp(ser: serial.Serial):
    ret = test_utils.get_lid_temperature(ser)
    print(f'Lid reading: {ret}')
    return ret

if __name__ == '__main__':
    print('Testing peltier system')
    ser = test_utils.build_serial()
    main_tester = StepResponse(set_whole_power, get_whole_temperature, 1, 95, 100, 0.1)
    test_utils.set_fans_manual(0, ser)
    main_tester.run(ser)
    main_tester.set_power(ser, 0)
    test_utils.set_fans_automatic(ser)
    print('Test done, exporting data')
    write_csv('main', main_tester.data)

    print('Testing lid heater system')
    lid_tester = StepResponse(set_lid_power, get_lid_temp, 1, 80, 100, 0.1)
    lid_tester.run(ser)
    lid_tester.set_power(ser, 0)
    print('Lid done, exporting')
    write_csv('lid', lid_tester.data)

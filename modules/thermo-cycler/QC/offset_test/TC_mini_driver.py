import sys, os
import csv
import time
import threading
import serial
from argparse import ArgumentParser

#---------------------------------------#
# Users can change these values as required #

TOTAL_RUNS = 1

HI_TEMP_OFFSET = 1.2
LO_TEMP_OFFSET = 0.4

COVER_TEMP = 105
PLATE_TEMP_PRE = 10
PLATE_TEMP_HOLD_1 = (94 - HI_TEMP_OFFSET, 180)
PLATE_TEMP_REPEAT = [(94 - HI_TEMP_OFFSET, 10), (70 - LO_TEMP_OFFSET, 30), (72 - LO_TEMP_OFFSET, 30)]
PLATE_TEMP_HOLD_2 = (72 - LO_TEMP_OFFSET, 300)
PLATE_TEMP_POST = 4
CYCLES = 29

#---------------------------------------#
# DO NOT change any of these
GCODES = True
TC_GCODE_ROUNDING_PRECISION = 2
# Use this global var with care. Status will have Hold_time of 0 when there was
# no hold time set as well as when the hold timer runs out


BAUDRATE = 115200
GCODES = {
    'OPEN_LID': 'M126',
    'CLOSE_LID': 'M127',
    'GET_LID_STATUS': 'M119',
    'SET_LID_TEMP': 'M140',
    'GET_LID_TEMP': 'M141',
    'DEACTIVATE_LID_HEATING': 'M108',
    'EDIT_PID_PARAMS': 'M301',
    'SET_PLATE_TEMP': 'M104',
    'GET_PLATE_TEMP': 'M105',
    'SET_RAMP_RATE': 'M566',
    'DEACTIVATE': 'M18',
    'DEVICE_INFO': 'M115'
}

SERIAL_ACK = '\r\n'


GET_STAT = 'stat{}'.format(SERIAL_ACK)
DEACTIVATE = '{}{}'.format(GCODES['DEACTIVATE'], SERIAL_ACK)

class thermocycler(serial.Serial):
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200, timeout = 0.1):
        self.error_count = 0
        self.max_errors = 100
        self.unlimited_errors = False
        self.raise_exceptions = True
        self.reading_raw = ''
        self._port = serial.Serial(
            	port = port,
            	baudrate = baudrate,
                stopbits = serial.STOPBITS_ONE,
                bytesize = 8,
                timeout=timeout)

    def _send(self, cmd):
        print("Sending: {}".format(cmd))
        self._port.write(cmd.encode())

    def parse_number_from_substring(self, substring, rounding_val):
        '''
        Returns the number in the expected string "N:12.3", where "N" is the
        key, and "12.3" is a floating point value

        For the temp-deck or thermocycler's temperature response, one expected
        input is something like "T:none", where "none" should return a None value
        '''
        try:
        	value = substring.split(':')[1]
        	if value.strip().lower() == 'none':
        		return None
        	return round(float(value), rounding_val)
        except (ValueError, IndexError, TypeError, AttributeError):
        	print('Unexpected argument to parse_number_from_substring:')
        	raise Exception(
        		'Unexpected argument to parse_number_from_substring: {}'.format(substring))

    def parse_key_from_substring(self, substring) -> str:
        '''
        Returns the axis in the expected string "N:12.3", where "N" is the
        key, and "12.3" is a floating point value
        '''
        try:
        	return substring.split(':')[0]
        except (ValueError, IndexError, TypeError, AttributeError):
        	print('Unexpected argument to parse_key_from_substring:')
        	raise Exception(
        		'Unexpected argument to parse_key_from_substring: {}'.format(substring))

    def read_temp(self):
        #CURRENT_HOLD_TIME = 99
        #GCODE_DEBUG_PRINT_MODE = 'M111 cont{}'.format(SERIAL_ACK)
        #self._send(GCODE_DEBUG_PRINT_MODE)
        while True:
            serial_line = self._port.readline()
            if serial_line:
                serial_list = serial_line.decode().split("\t")
                serial_list = list(map(lambda x: x.strip(), serial_list))
                data_res = {}
                for substr in serial_list:
                    if substr == '':
                        continue
                    key = self.parse_key_from_substring(substr)
                    value = self.parse_number_from_substring(substr, TC_GCODE_ROUNDING_PRECISION)
                    data_res[key] = value
                return data_res #return particular temperature
            time.sleep(0.1)

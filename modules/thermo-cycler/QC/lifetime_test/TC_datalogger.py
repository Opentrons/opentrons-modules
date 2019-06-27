import serial
import csv
import time
import threading
from argparse import ArgumentParser

#---------------------------------------#
TOTAL_RUNS = 250
GCODES = True
TC_GCODE_ROUNDING_PRECISION = 2

COVER_TEMP = 105
PLATE_TEMP_PRE = 10
PLATE_TEMP_HOLD_1 = (94, 180)
# Following temp durations have been increased by 10 sec to accommodate for ramp time
PLATE_TEMP_REPEAT = [(94, 20), (70, 40), (72, 40)]
PLATE_TEMP_HOLD_2 = (72, 300)
PLATE_TEMP_POST = 4
CYCLES = 29
#---------------------------------------#

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

PROTOCOL_STEPS = [
	'{} S{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_PRE, SERIAL_ACK),	# 0
	'{} S{}{}'.format(GCODES['SET_LID_TEMP'], COVER_TEMP, SERIAL_ACK),		# 1
	'{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_HOLD_1[0], PLATE_TEMP_HOLD_1[1], SERIAL_ACK),		# 2
	'{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_REPEAT[0][0], PLATE_TEMP_REPEAT[0][1], SERIAL_ACK),# 3
	'{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_REPEAT[1][0], PLATE_TEMP_REPEAT[1][1], SERIAL_ACK),# 4
	'{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_REPEAT[2][0], PLATE_TEMP_REPEAT[2][1], SERIAL_ACK),# 5
	'{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_HOLD_2[0], PLATE_TEMP_HOLD_2[1], SERIAL_ACK),		# 6
	'{}{}'.format(GCODES['DEACTIVATE_LID_HEATING'], SERIAL_ACK),				# 7
	'{} S{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_POST, SERIAL_ACK)	# 8
	]
GCODE_DEBUG_PRINT_MODE = 'M111 cont{}'.format(SERIAL_ACK)
GET_STAT = 'stat{}'.format(SERIAL_ACK)
DEACTIVATE = '{}{}'.format(GCODES['DEACTIVATE'], SERIAL_ACK)

def build_arg_parser():
	arg_parser = ArgumentParser(
		description="Thermocycler temperature data logger")
	arg_parser.add_argument("-P", "--module_port", required=True)
	arg_parser.add_argument("-F", "--csv_file_name", required=True)
	return arg_parser

def _send(ser, lock, cmd):
	with lock:
		print("Sending: {}".format(cmd))
		ser.write(cmd.encode())

def parse_number_from_substring(substring, rounding_val):
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


def parse_key_from_substring(substring) -> str:
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


def run_protocol(ser, lock):
	for run_x in range(TOTAL_RUNS):
		print("****** Run #{} *******".format(run_x))
		_send(ser, lock, PROTOCOL_STEPS[0])	# Plate PRE
		_send(ser, lock, PROTOCOL_STEPS[1])	# Lid temp
		time.sleep(150)	 # Takes approx 5 minutes for lid to heat
		_send(ser, lock, PROTOCOL_STEPS[2])	# First point
		time.sleep(PLATE_TEMP_HOLD_1[1])
		for i in range(CYCLES):
			_send(ser, lock, PROTOCOL_STEPS[3])	# First repeat
			time.sleep(PLATE_TEMP_REPEAT[0][1])
			_send(ser, lock, PROTOCOL_STEPS[4])	# Second repeat
			time.sleep(PLATE_TEMP_REPEAT[1][1])
			_send(ser, lock, PROTOCOL_STEPS[5])	# Last repeat
			time.sleep(PLATE_TEMP_REPEAT[2][1])
		_send(ser, lock, PROTOCOL_STEPS[6])	# Rest step
		time.sleep(PLATE_TEMP_HOLD_2[1])
		_send(ser, lock, PROTOCOL_STEPS[7])	# Lid stop
		_send(ser, lock, PROTOCOL_STEPS[8])	# incubate
		time.sleep(200)
		print("******* Run #{} Completed *******".format(run_x))
	_send(ser, lock, DEACTIVATE)


def record_status(filename, ser, lock):
	_send(ser, lock, GCODE_DEBUG_PRINT_MODE)
	time.sleep(0.5)
	ser.readline()
	ser.readline()
	while True:
		with lock:
			serial_line = ser.readline()
		if serial_line:
			serial_list = serial_line.decode().split("\t")
			serial_list = list(map(lambda x: x.strip(), serial_list))
			data_res = {}
			for substr in serial_list:
				if substr == '':
					continue
				print(substr,end ="\t"),
				key = parse_key_from_substring(substr)
				value = parse_number_from_substring(substr, TC_GCODE_ROUNDING_PRECISION)
				data_res[key] = value
			print()
			print("---------------------")
			status_list = []
			for key, val in data_res.items():
				status_list.append(val)
			with open('{}.csv'.format(filename), mode='a') as data_file:
				data_writer = csv.writer(data_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
				data_writer.writerow(status_list)
		time.sleep(0.1)

if __name__ == '__main__':
	arg_parser = build_arg_parser()
	args = arg_parser.parse_args()
	ser = serial.Serial(args.module_port, baudrate=BAUDRATE, timeout=1)
	print("Serial: {}".format(ser))
	time.sleep(1)
	filename = args.csv_file_name
	with open('{}.csv'.format(filename), mode='w') as data_file:
	    data_writer = csv.writer(data_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
	    data_writer.writerow(["Millis", "Plate target", "Cover target", "hold time", "therm1", "therm2", "therm3", "therm4", "therm5", "therm6", "heatsink", "loop duration", "fan", "auto fan?", "lid temp", "motor fault"])
	lock = threading.Lock()
	recorder = threading.Thread(target=record_status, args=(filename, ser, lock), daemon=True)
	protocol_writer = threading.Thread(target=run_protocol, args=(ser, lock))
	recorder.start()
	time.sleep(5)	# Just to record pre-protocol data
	print("Starting protocol writer")
	protocol_writer.start()
	protocol_writer.join()

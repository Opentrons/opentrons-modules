import serial
import csv
import time
import threading
from argparse import ArgumentParser

SLEEP_TIME = 17 #seconds
TEST_ROUNDS = 1000
BAUDRATE = 115200
SERIAL_ACK = '\r\n'
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

OPEN_CMD = '{}{}'.format(GCODES['OPEN_LID'], SERIAL_ACK)
CLOSE_CMD = '{}{}'.format(GCODES['CLOSE_LID'], SERIAL_ACK)

def build_arg_parser():
	arg_parser = ArgumentParser(
		description="Thermocycler temperature data logger")
	arg_parser.add_argument("-P", "--module_port", required=True)
	arg_parser.add_argument("-F", "--csv_file_name", required=True)
	return arg_parser

def record_status(filename, ser, lock):
	while True:
		with lock:
			serial_line = ser.readline()
		if serial_line:
			print("Received a serial line")
			status_list = serial_line.decode().split(",")
			status_list = list(map(lambda x: x.strip(), status_list))
			for i, item in enumerate(status_list):
				if item.isdigit():
					status_list[i] = int(item)
			print("{}".format(status_list))
			with open('{}.csv'.format(filename), mode='a') as data_file:
				data_writer = csv.writer(data_file, delimiter=',', quoting=csv.QUOTE_NONNUMERIC)
				data_writer.writerow(status_list)
		time.sleep(1);

def _send(ser, lock, cmd):
	print("Sending: {}".format(cmd))
	ser.write(cmd.encode())

def open_close_lid(ser, lock):
	for i in range(TEST_ROUNDS):
		with lock:
			_send(ser, lock, OPEN_CMD);
			time.sleep(SLEEP_TIME);
			_send(ser,lock, CLOSE_CMD);
			time.sleep(SLEEP_TIME);
		time.sleep(2)

if __name__ == '__main__':
	arg_parser = build_arg_parser()
	args = arg_parser.parse_args()
	ser = serial.Serial(args.module_port, baudrate=BAUDRATE, timeout=1)
	print("Serial: {}".format(ser))
	time.sleep(1)
	filename = args.csv_file_name
	with open('{}.csv'.format(filename), mode='w') as data_file:
	    data_writer = csv.writer(data_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_NONNUMERIC)
	    data_writer.writerow(["Action", "End status", "Duration (msec)", "Action", "End status", "Duration (msec)"])
	lock = threading.Lock()
	recorder = threading.Thread(target=record_status, args=(filename, ser, lock), daemon=True)
	lid_tester = threading.Thread(target=open_close_lid, args=(ser, lock))
	recorder.start()
	lid_tester.start()
	lid_tester.join()

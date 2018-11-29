# Use this script to upload firmware to modules in case 
# uploading through update endpoints/app isn't working.
# The script makes use of the 1200baud_touch feature of
# arduino to set the module into bootloader mode and then
# searches for the bootloader port and uploads firmware.
# To use this script, run with the following args:
# -P: module portname 
# -F: firmware file path
# -C (optional): conf file path
# -B (optional): bootloader port, if not 'ttyn_bootloader' 
# -A (optional): avrdude location

import asyncio
import serial
import time
import os
import subprocess
from argparse import ArgumentParser


PART_NO='atmega32u4'
PROGRAMMER_ID='avr109'
BAUDRATE=57600
UPDATE_TIMEOUT = 15 # seconds


def build_arg_parser():
	arg_parser = ArgumentParser(
		description="Firmware uploader for Opentrons modules")
	arg_parser.add_argument("-P", "--module_port", required=True)
	arg_parser.add_argument("-F", "--firmware_file", required=True)
	arg_parser.add_argument("-C", "--config_file")
	arg_parser.add_argument("-B", "--bootloader_port")
	arg_parser.add_argument("-A", "--avrdude_loc")
	return arg_parser


async def search_port_name():
	if os.path.isdir('/dev/modules'):
		for attempt in range(2):
			# Measure for race condition where port is being switched in
			# between calls to isdir() and listdir()
			try:
				return os.listdir('/dev/modules')
			except (FileNotFoundError, OSError):
				pass
			await asyncio.sleep(2)
		raise Exception("No /dev/modules found. Try again")
	else:
		print('/dev/modules: no such directory')


async def get_bootloader_port():
	print('Searching bootloader port')
	new_port=''
	while not new_port:
		ports = await search_port_name()
		if ports:
			discovered_ports = list(filter(
				lambda x: x.endswith('bootloader'), ports))
			if len(discovered_ports) == 1:
				new_port = '/dev/modules/{}'.format(discovered_ports[0])
				print('Found bootloader port')
		await asyncio.sleep(0.05)
	return new_port


async def upload(loop, args):
	firmware_file = args.firmware_file
	if args.config_file:
		config_file = args.config_file
	else:
		config_file = '/usr/local/lib/python3.6/'\
		'site-packages/opentrons/config/modules/avrdude.conf'
	if args.bootloader_port:
		bootloader_port = args.bootloader_port
		asyncio.sleep(1)		# wait for the port to change
	else:
		try:
			bootloader_port = await asyncio.wait_for(get_bootloader_port(), 6)
		except asyncio.TimeoutError:
			return 'No bootloader port found'
	if args.avrdude_loc:
		avrdude = args.avrdude_loc
	else:
		avrdude = 'avrdude'
	print("Executing AVRdude cmd:")
	print('{} -C{} -v -p{} -c{} -P{} -b{} -D '\
	'-Uflash:w:{}:i'.format(avrdude, config_file, PART_NO, 
		PROGRAMMER_ID, bootloader_port, BAUDRATE, firmware_file))
	proc = await asyncio.create_subprocess_exec(
	        '{}'.format(avrdude), '-C{}'.format(config_file), '-v',
	        '-p{}'.format(PART_NO),
	        '-c{}'.format(PROGRAMMER_ID),
	        '-P{}'.format(bootloader_port),
	        '-b{}'.format(BAUDRATE), '-D',
	        '-Uflash:w:{}:i'.format(firmware_file),
	        stdout=asyncio.subprocess.PIPE,
	        stderr=asyncio.subprocess.PIPE, loop=loop)
	await proc.wait()

	_result = await proc.communicate()
	result = _result[1].decode()
	return result


async def main(loop, args):
	try:
		res = await asyncio.wait_for(upload(loop, args),UPDATE_TIMEOUT)
		print('response: {}'.format(res))
	except asyncio.TimeoutError:
		print('message: Process not responding')


if __name__ == '__main__':
	arg_parser = build_arg_parser()
	args = arg_parser.parse_args()
	# Open close port at 1200bps to enter bootloader
	ser = serial.Serial(args.module_port, baudrate=1200)
	time.sleep(1)
	ser.close()
	time.sleep(1.5)
	loop = asyncio.get_event_loop()
	loop.run_until_complete(main(loop, args))

	
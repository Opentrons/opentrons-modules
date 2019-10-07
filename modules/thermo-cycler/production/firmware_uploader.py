# This script searches for a thermocycler port and uploads the pre-built .bin
# from `/production/firmware`, or, a .bin/.uf2 file provided, to the module
#
# Usage: `python firmware_uploader.py` or
#        `python firmware_uploader.py -F my_firmware_file.bin`
# Requires:
#   - Thermocycler Sam-ba or uf2 bootloader for thermocycler
#   - Firmware binary in `production/firmware/`
#     or path to binary/uf2 file added as argument `-F`
#   - BOSSA cmdline tool- https://github.com/shumatech/BOSSA/releases (Tested with 1.9.1)

import os
import sys
import uf2conv
import serial
import time
import subprocess
import asyncio
from serial.tools.list_ports import comports
from argparse import ArgumentParser

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
DEFAULT_FW_FILE_PATH = os.path.join(THIS_DIR, "firmware", "thermo-cycler-arduino.ino.bin")
OPENTRONS_VID = 1240
TC_BOOTLOADER_PID = 0xED12
MAX_SERIAL_LEN = 16

def build_arg_parser():
    arg_parser = ArgumentParser(
        description="Thermocycler firmware uploader")
    arg_parser.add_argument("-F", "--fw_file", required=False,
                            default=DEFAULT_FW_FILE_PATH,
                            help='Firmware file (default: ..production/firmware/thermo-cycler-arduino.ino.bin)')
    return arg_parser

def find_opentrons_port(bootloader=False):
    retries = 5
    while retries:
        for p in comports():
            if p.vid == OPENTRONS_VID:
                if bootloader:
                    if p.pid != TC_BOOTLOADER_PID:
                        continue
                print("Port found:{}".format(p.device))
                time.sleep(1)
                return p.device
        retries -= 1
        time.sleep(0.5)
    raise RuntimeError('Could not find Opentrons Thermocycler connected over USB')

def connect_to_module(port_name):
    print('Connecting to module...')
    port = serial.Serial(port_name, 115200, timeout=1)
    print('Connected.')
    return port

def trigger_bootloader(port_name):
    port = serial.Serial(port_name, 1200, timeout=1)
    time.sleep(0.005)
    port.close()
    return port

def find_bootloader_drive():
    # takes around 4 seconds for TCBOOT to register in /Volumes
    retries = 7
    while retries:
        for d in uf2conv.get_drives():
            if "TCBOOT" in d:
                print("Bootloader Volume found")
                return d
        print("Searching bootloader...")
        retries -= 1
        time.sleep(1)
    raise Exception ('Bootloader volume not found')


def upload_uf2(uf2_file, drive):
    print("Converting to uf2, output size: %d, start address: 0x%x" %
          (len(outbuf), uf2conv.appstartaddr))
    print("Flashing %s (%s)" % (drive, uf2conv.board_id(drive)))
    uf2conv.write_file(drive + "/NEW.UF2", outbuf)
    return True

def upload_using_bossa(bin_file, port):
    # bossac -p/dev/cu.usbmodem14101 -e -w -v -R --offset=0x2000 modules/thermo-cycler/production/firmware/thermo-cycler-arduino.ino.bin
    if sys.platform == "linux" or sys.platform == "linux2":
        bossa_cmd = './bossac'
    else:
        bossa_cmd = 'bossac'
    bossa_args = [bossa_cmd, '-p{}'.format(port), '-e', '-w', '-v', '-R', '--offset=0x2000', '{}'.format(bin_file)]
    proc = subprocess.run(bossa_args, timeout=60, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    res = proc.stdout.decode()
    print(res)
    if "Verify successful" in res:
        return True
    elif proc.stderr:
        print(proc.stderr)
        return False

def main():
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    firmware_file = args.fw_file
    print('\n')
    connected_port = None
    try:
        print('\nTrigerring Bootloader..')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading application..')
        fw_uploaded = False
        with open(firmware_file, mode='rb') as f:
            if uf2conv.is_uf2(f.read()):
                print("Uf2 file. Copying to drive..")
                fw_uploaded = upload_sketch(firmware_file, find_bootloader_drive())
            else:
                print("Binary file. Flashing using bossac")
                fw_uploaded = upload_using_bossa(firmware_file, find_opentrons_port(bootloader=True))
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
        if fw_uploaded:
            print('\n\n PASS! Uploaded Sketch: {}'.format(firmware_file))
        else:
            print('\n\n FAILED to upload sketch: {}'.format(firmware_file))
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
    except KeyboardInterrupt:
        exit()
    except Exception as e:
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
        print('\n\nFAIL: {}'.format(e))
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
    finally:
        if connected_port:
            connected_port.close()

if __name__ == "__main__":
    main()

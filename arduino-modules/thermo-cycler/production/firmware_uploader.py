# This script searches for a thermocycler port and uploads the pre-built .bin
# from `/production/bin`, or, a .bin/.uf2 file provided, to the module
#
# Usage: `python firmware_uploader.py` or
#        `python firmware_uploader.py -F my_firmware_file.bin`
# Requires:
#   - Thermocycler Sam-ba or uf2 bootloader for thermocycler
#   - Firmware binary in `production/bin/`
#     or path to binary/uf2 file added as argument `-F`
#   - BOSSA cmdline tool- https://github.com/shumatech/BOSSA/releases (Tested with 1.9.1)

from pathlib import PurePath
import sys
import serial
import time
import subprocess
from serial.tools.list_ports import comports
from argparse import ArgumentParser

THIS_DIR = PurePath(__file__).parent
DEFAULT_FW_FILE_PATH = THIS_DIR.joinpath('thermo-cycler-arduino.ino.bin')
OPENTRONS_VID       = 0x04d8
ADAFRUIT_VID        = 0x239a
TC_BOOTLOADER_PID   = 0xed12
ADAFRUIT_BOOTLD_PID = 0x000b
MAX_SERIAL_LEN = 16

def build_arg_parser():
    arg_parser = ArgumentParser(
        description="Thermocycler firmware uploader")
    arg_parser.add_argument("-F", "--fw_file", required=False,
                            default=DEFAULT_FW_FILE_PATH,
                            help='Firmware file (default: thermo-cycler-arduino.ino.bin '
                                 'located in same dir as uploader)')
    return arg_parser

def find_opentrons_port(bootloader=False):
    retries = 5
    while retries:
        for p in comports():
            if p.vid in (OPENTRONS_VID, ADAFRUIT_VID):
                print("Available: {0}->\t(pid:{1:#x})\t(vid:{2:#x})".format(p.device, p.pid, p.vid))
                if bootloader:
                    if p.pid not in (TC_BOOTLOADER_PID, ADAFRUIT_BOOTLD_PID):
                        continue
                print("Port found:{}".format(p.device))
                if p.pid == ADAFRUIT_BOOTLD_PID:
                    print("--- WARNING!! : Uploading with Adafruit bootloader."
                          " Please update to TC bootloader ---")
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

def upload_using_bossa(bin_file, port):
    # bossac -p/dev/cu.usbmodem14101 -e -w -v -R --offset=0x2000 modules/thermo-cycler/production/bin/thermo-cycler-arduino.ino.bin
    if sys.platform == "linux" or sys.platform == "linux2":
        bossa_cmd = 'bossac'
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
        raise Exception("BOSSA error:{}".format(proc.stderr.decode()))

def main():
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    firmware_file = args.fw_file
    print('\n')
    print("FW file: {}".format(firmware_file))
    connected_port = None
    try:
        print('\nTrigerring Bootloader..')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading application..')
        print("Flashing using bossac")
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

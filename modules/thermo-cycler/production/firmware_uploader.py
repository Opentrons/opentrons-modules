# This script searches for a thermocycler port and uploads the pre-built binary
# from /build/tmp to the module
# Usage: `python firmware_uploader.py` (No args)
# Requires:
#   - New uf2 bootloader for thermocycler
#   - Firmware binary in /build/tmp (This is where `make build` or Arduino->compile
#     would store the build binary)

import uf2conv
import serial
import time
from serial.tools.list_ports import comports

FIRMWARE_FILE_PATH = "../../../build/tmp/thermo-cycler-arduino.ino.bin"
OPENTRONS_VID = 1240
MAX_SERIAL_LEN = 16

def find_opentrons_port():
    retries = 5
    while retries:
        for p in comports():
            if p.vid == OPENTRONS_VID:
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


def upload_sketch(sketch_file, drive):
    with open(sketch_file, mode='rb') as f:
        inpbuf = f.read()
    outbuf = uf2conv.convert_to_uf2(inpbuf)
    print("Converting to uf2, output size: %d, start address: 0x%x" %
          (len(outbuf), uf2conv.appstartaddr))
    print("Flashing %s (%s)" % (drive, uf2conv.board_id(drive)))
    uf2conv.write_file(drive + "/NEW.UF2", outbuf)

def main():
    print('\n')
    connected_port = None
    try:
        print('\nTrigerring Bootloader..')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading application..')
        upload_sketch(FIRMWARE_FILE_PATH, find_bootloader_drive())
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
        print('\n\n Uploaded Sketch: {}'.format(FIRMWARE_FILE_PATH))
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

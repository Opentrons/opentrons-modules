# Script that will:
# - Upload eeprom writer code onto TC
# - Write Serial number from scanner onto TC
# - Verify written Serial number
# - Upload production TC firmware
# NOT Compatible with UF2
#
# BOSSA: bossac -p/dev/cu.usbmodem14101 -e -w -v -R --offset=0x2000 thermo-cycler-arduino.ino.bin
from pathlib import Path
import sys
import subprocess
import serial
import time
from serial.tools.list_ports import comports
from argparse import ArgumentParser

THIS_DIR = Path.cwd()
DEFAULT_FW_FILE_PATH = THIS_DIR.resolve().parent.joinpath('thermo-cycler', 'thermo-cycler-arduino.ino.bin')
EEPROM_WRITER_PATH = THIS_DIR.joinpath('eepromWriter.ino.bin')
OPENTRONS_VID = 1240
TC_BOOTLOADER_PID = 0xED12
MAX_SERIAL_LEN = 16
BAD_BARCODE_MESSAGE = 'Serial longer than expected -> {}'
WRITE_FAIL_MESSAGE = 'Data not saved'

def build_arg_parser():
    arg_parser = ArgumentParser(
        description="Thermocycler serial & firmware uploader")
    arg_parser.add_argument("-F", "--fw_file", required=False,
                            default=DEFAULT_FW_FILE_PATH,
                            help='Firmware file (default: ..production/bin/thermo-cycler-arduino.ino.bin)')
    return arg_parser

def find_opentrons_port(bootloader=False):
    retries = 5
    while retries:
        for p in comports():
            if p.vid == OPENTRONS_VID:
                print("Available: {}->\t(pid:{})\t(vid:{})".format(p.device, p.pid, p.vid))
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

def upload_using_bossa(bin_file, port):
    # bossac -p/dev/cu.usbmodem14101 -e -w -v -R --offset=0x2000 modules/thermo-cycler/production/bin/thermo-cycler-arduino.ino.bin
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
        raise Exception("BOSSA error:{}".format(proc.stderr.decode()))

def _user_submitted_barcode(max_length):
    print('\n\n-----------------')
    print('-----------------')
    print('-----------------')
    print('\n\nScan the barcode\n\n')
    barcode = input('\tSCAN: ').strip() # remove all whitespace
    print('\n\n-----------------')
    print('-----------------')
    print('-----------------')
    if len(barcode) > max_length:
        raise Exception(BAD_BARCODE_MESSAGE.format(barcode))
    # remove all characters before the letter T
    # for example, remove ASCII selector code "\x1b(B" on chinese keyboards
    if 'TC' in barcode:
        barcode = barcode[barcode.index('TC'):]
        return barcode

def check_previous_data(module):
    print("Checking existing data..")
    serial, model = _get_info(module)
    if serial == '~' or model == '~':
        print('No/ invalid old data on this device')
        return
    print(
        'Overwriting old data: id={0}, model={1}'.format(serial, model))


def _get_info(module):
    info = (None, None)
    # Empty out existing input buffer
    module.reset_input_buffer()
    module.write(b'&')  # read stored data
    # ignoring errors caused by spurious 0xff
    serial = module.read_until(b'\r\n').decode(errors="ignore").strip().split(':')
    model = module.read_until(b'\r\n').decode(errors="ignore").strip().split(':')
    return (serial[1], model[1])


def write_identifiers(module, new_serial, model):
    to_write = '{}:{}'.format(new_serial, model)
    module.write(to_write.encode())
    time.sleep(2)
    serial, model = _get_info(module)
    _assert_the_same(new_serial, serial)
    _assert_the_same(_parse_model_from_barcode(new_serial), model)
    print("EEPROM written to & verified")

def _parse_model_from_barcode(barcode):
    model = 'v' + barcode[barcode.index('V')+1: barcode.index('V')+3]
    return model

def _assert_the_same(a, b):
    if a != b:
        raise Exception(WRITE_FAIL_MESSAGE + ' ({0} != {1})'.format(a, b))

def assert_id_and_model(port, serial, model):
    print("asserting id & model..")
    msg = 'M115\r\n'.encode()
    port.reset_input_buffer()
    port.write(msg)
    res = port.read_until('ok\r\nok\r\n'.encode())
    res = res.decode().split('\r')[0]
    device_info = {
        r.split(':')[0]: r.split(':')[1]
        for r in res.split(' ')
    }
    assert device_info.get('serial') == serial, "Device serial does not match"
    assert device_info.get('model') == model, "Device model does not match"

def main():
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    firmware_file = args.fw_file
    print('\n')
    connected_port = None
    try:
        print('\nTrigerring Bootloader..')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading EEPROM sketch..')
        upload_using_bossa(EEPROM_WRITER_PATH, find_opentrons_port(bootloader=True))
        print('\nAsking for barcode..')
        barcode = _user_submitted_barcode(MAX_SERIAL_LEN)
        model = _parse_model_from_barcode(barcode)
        print('\nConnecting to device and writing..')
        connected_port = connect_to_module(find_opentrons_port())
        check_previous_data(connected_port)
        write_identifiers(connected_port, barcode, model)
        connected_port.close()
        print('\nTriggering Bootloader..')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading application..')
        upload_using_bossa(firmware_file, find_opentrons_port(bootloader=True))
        print('\nConnecting to device and testing..')
        time.sleep(5)  # wait for it to boot up
        connected_port = connect_to_module(find_opentrons_port())
        assert_id_and_model(connected_port, barcode, model)
        print('\n\n-----------------')
        print('-----------------')
        print('-----------------')
        print('\n\nPASS: Saved -> {0} (model {1})'.format(barcode, model))
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

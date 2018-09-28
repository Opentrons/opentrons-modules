'''
    1) discover port name
    2) open/close port at 1200bps to reset into bootloader
    3) Run AVR commnad to upload eeprom writing sketch
    4) Save ID and model from barcode
    5) open/close port at 1200bps to reset into bootloader
    6) Run AVR commnad to upload application firmware
    7) Check to make sure it return the correct information (M115)
'''

import os
import subprocess
import sys
import time

from serial import Serial
from serial.tools.list_ports import comports


OPENTRONS_VID = 1240
BOOTLOADER_PID = 61018

BAD_BARCODE_MESSAGE = 'Unexpected Serial -> {}'
WRITE_FAIL_MESSAGE = 'Data not saved'

MODELS = {
    'TDV01': 'temp_deck_v1.1',  # make sure to skip v2 if model updates
    'TDV03': 'temp_deck_v3.0',  # this model has the new fans with pcb fan_v4
    'TDV04': 'temp_deck_v4.0',  # this model has the new fans with pcb fan_v4.1
    'MDV01': 'mag_deck_v1.1'
}

DIR_NAME = os.path.dirname(os.path.realpath(__file__))
AVR_CONFIG_FILE = os.path.join(DIR_NAME, 'avrdude.conf')
FIRMWARE_DIR = os.path.join(DIR_NAME, 'firmware')
EEPROM_FIRMARE_PATH = os.path.join(FIRMWARE_DIR, 'eepromWriter.hex')
TEMP_DECK_FIRMARE_PATH = os.path.join(FIRMWARE_DIR, 'temp-deck-arduino.ino.tempdeck32u4.hex')
MAG_DECK_FIRMARE_PATH = os.path.join(FIRMWARE_DIR, 'mag-deck-arduino.ino.magdeck32u4.hex')

AVR_COMMAND = 'avrdude -C {config} -v -patmega32u4 -cavr109 -P {port} -b 57600 -D -U flash:w:{firmware}:i'  # NOQA


def find_opentrons_port():
    for i in range(5 * 2):
        print([
            'PID:{0}, VID:{1}'.format(p.pid, p.vid)
            for p in comports() if p.vid
        ])
        for p in comports():
            if p.vid == OPENTRONS_VID:
                time.sleep(1)
                return p.device
        time.sleep(0.5)
    raise RuntimeError('Could not find Opentrons Model connected over USB')

def find_bootloader_port():
    for i in range(5 * 2):
        print([
            'PID:{0}, VID:{1}'.format(p.pid, p.vid)
            for p in comports() if p.vid
        ])
        for p in comports():
            if p.vid == OPENTRONS_VID and p.pid == BOOTLOADER_PID:
                time.sleep(1)
                return p.device
        time.sleep(0.5)
    raise RuntimeError('Could not find bootloader connected over USB')


def trigger_bootloader(port_name):
    port = Serial(port_name, 1200, timeout=1)
    # time.sleep(0.005)
    port.close()
    time.sleep(0.3)
    return port


def upload_eeprom_sketch(port_name):
    cmd = AVR_COMMAND.format(
        config=AVR_CONFIG_FILE, port=port_name, firmware=EEPROM_FIRMARE_PATH)
    res = subprocess.check_output('{}'.format(cmd), shell=True)
    print('AVR gave response:')
    print(res)


def upload_application_firmware(port_name, model):
    hex_file = ''
    if 'temp' in model:
        hex_file = TEMP_DECK_FIRMARE_PATH
    elif 'mag' in model:
        hex_file = MAG_DECK_FIRMARE_PATH
    else:
        raise RuntimeError(
            'Unknown model for writing firmware: {}'.format(model))
    cmd = AVR_COMMAND.format(
        config=AVR_CONFIG_FILE, port=port_name, firmware=hex_file)
    res = subprocess.check_output('{}'.format(cmd), shell=True)
    print('AVR gave response:')
    print(res)


def assert_id_and_model(port, serial, model):
    msg = 'M115\r\n'.encode()
    port.reset_input_buffer()
    port.write(msg)
    res = port.read_until('ok\r\nok\r\n'.encode())
    res = res.decode().split('\r')[0]
    device_info = {
        r.split(':')[0]: r.split(':')[1]
        for r in res.split(' ')
    }
    assert device_info.get('serial') == serial
    assert device_info.get('model') == model


def connect_to_module(port_name):
    print('Connecting to module...')
    port = Serial(port_name, 115200, timeout=1)
    return port


def write_identifiers(module, new_id, new_model):
    msg = '{0}:{1}'.format(new_id, new_model)
    module.write(msg.encode())
    module.read_until(b'\r\n')
    serial, model = _get_info(module)
    _assert_the_same(new_id, serial)
    _assert_the_same(new_model, model)


def check_previous_data(module):
    serial, model = _get_info(module)
    if not serial or not model:
        print('No old data on this device')
        return
    print(
        'Overwriting old data: id={0}, model={1}'.format(serial, model))


def _get_info(module):
    info = (None, None)
    module.write(b'&')  # special character to retrive old data
    res = module.read_until(b'\r\n').decode().strip()
    return tuple(res.split(':'))


def _assert_the_same(a, b):
    if a != b:
        raise Exception(WRITE_FAIL_MESSAGE + ' ({0} != {1})'.format(a, b))


def _user_submitted_barcode(max_length):
    print('\n\n-----------------')
    print('-----------------')
    print('-----------------')
    print('\n\nScan the barcode\n\n')
    barcode = input('\tSCAN: ').strip()
    print('\n\n-----------------')
    print('-----------------')
    print('-----------------')
    if len(barcode) > max_length:
        raise Exception(BAD_BARCODE_MESSAGE.format(barcode))
    # remove all characters before the letter T
    # for example, remove ASCII selector code "\x1b(B" on chinese keyboards
    for m in MODELS.keys():
        if m in barcode:
            barcode = barcode[barcode.index(m):]
    barcode = barcode.split('\n')[0].split('\r')[0]
    barcode = ''.join([b for b in barcode if b.isalpha() or b.isdigit()])
    return barcode


def _parse_model_from_barcode(barcode):
    for barcode_substring, model in MODELS.items():
        if barcode.startswith(barcode_substring):
            return model
    raise Exception(BAD_BARCODE_MESSAGE.format(barcode))


def main():
    print('\n')
    connected_port = None
    try:
        print('\nTriggering Bootloader')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading EEPROM sketch')
        upload_eeprom_sketch(find_bootloader_port())
        print('\nAsking for barcode')
        barcode = _user_submitted_barcode(32)
        model = _parse_model_from_barcode(barcode)
        print('\nConnecting to device and writing')
        connected_port = connect_to_module(find_opentrons_port())
        check_previous_data(connected_port)
        write_identifiers(connected_port, barcode, model)
        connected_port.close()
        print('\nTriggering Bootloader')
        trigger_bootloader(find_opentrons_port())
        print('\nUploading application')
        upload_application_firmware(find_bootloader_port(), model)
        print('\nConnecting to device and testing')
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

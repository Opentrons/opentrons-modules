'''
    1) discover port name
    2) open/close port at 1200bps to reset into bootloader
    3) Run AVR commnad to upload eeprom writing sketch
    4) Save ID and model from barcode
    5) open/close port at 1200bps to reset into bootloader
    6) Run AVR commnad to upload application firmware
    7) Check to make sure it return the correct information (M115)
'''

import subprocess
import sys

from serial import Serial
from serial.tools.list_ports import comports


OPENTRONS_VID = 1240

BAD_BARCODE_MESSAGE = 'Unexpected Serial -> {}'
WRITE_FAIL_MESSAGE = 'Data not saved'

MODELS = {
    'TDV01': 'temp_deck_v1.1',  # make sure to skip v2 if model updates
    'MDV01': 'mag_deck_v1.1'
}

EEPROM_FIRMARE_PATH = './eepromWriter.hex'
TEMP_DECK_FIRMARE_PATH = './temp-deck-arduino.ino.tempdeck32u4.hex'
MAG_DECK_FIRMARE_PATH = './mag-deck-arduino.ino.magdeck32u4.hex'

AVR_COMMAND = 'avrdude -C ./avrdude.conf -v -patmega32u4 -cavr109 -P {port} -b 57600 -D -U flash:w:{firmware}:i'  # NOQA


def find_device_port():
    port = None
    for p in comports():
        if p.vid == OPENTRONS_VID:
            port = p.device
            break
    if port is None:
        raise RuntimeError('Could not find Opentrons Model connected over USB')
    return port


def trigger_bootloader(port_name):
    port = Serial(port_name, 1200, timeout=1)
    time.sleep(0.02)
    port.close()
    return td


def upload_eeprom_sketch(port_name):
    cmd = AVR_COMMAND.format(
        port=port_name, firmware=EEPROM_FIRMARE_PATH)
    res = subprocess.check_output('{} &> /dev/null'.format(cmd), shell=True)


def upload_application_firmware(port_name, model):
    hex_file = ''
    if 'temp' in model:
        hex_file = TEMP_DECK_FIRMARE_PATH
    elif 'mag' in model:
        hex_file = MAG_DECK_FIRMARE_PATH
    else raise RuntimeError(
        'Unknown model for writing firmware: {}'.format(model))
    cmd = AVR_COMMAND.format(
        port=port_name, firmware=hex_file)


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
    assert device_info.get('serial') == barcode
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
    barcode = input('SCAN: ').strip()
    if len(barcode) > max_length:
        raise Exception(BAD_BARCODE_MESSAGE.format(barcode))
    # remove all characters before the letter T
    # for example, remove ASCII selector code "\x1b(B" on chinese keyboards
    for m in MODELS.keys():
        if m in barcode:
            barcode = barcode[barcode.index(m):]
    barcode = barcode.split('\n')[0].split('\r')[0]
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
        port_name = find_device_port()
        trigger_bootloader(port_name)
        upload_eeprom_sketch(port_name)
        barcode = _user_submitted_barcode(32)
        model = _parse_model_from_barcode(barcode)
        connected_port = connect_to_module(port_name)
        check_previous_data(connected_port)
        write_identifiers(connected_port, barcode, model)
        connected_port.close()
        trigger_bootloader(port_name)
        upload_application_firmware(port_name, model)
        connected_port = connect_to_module(port_name)
        assert_id_and_model(connected_port, barcode, model)
        print('PASS: Saved -> {0} (model {1})'.format(barcode, model))
    except KeyboardInterrupt:
        exit()
    except Exception as e:
        print('FAIL: {}'.format(e))
    finally:
        if connected_port:
            connected_port.close()
        main()


if __name__ == "__main__":
    main()

import os
from datetime import datetime
import sys
import time

import serial
from serial.tools.list_ports import comports

from opentrons import robot


PID_MAGDECK = 61072

TEST_CYCLES = 50
MAX_ALLOWED_FAILURES = 3

FIRMWARE_HOMING_RETRACT = 2  # must match firmware!!
FIRMWARE_MAX_TRAVEL_DISTANCE = 40  # must match firmware!!
TEST_BOTTOM_POS = 1
SKIPPING_TOLERANCE = 0.5

FIRMWARE_DEFAULT_MOVE_CURRENT = 0.4  # must match firmware!!
CURRENT_MOVE_PERCENTAGE = 0.75

FIRMWARE_DEFAULT_PROBE_CURRENT = 0.04  # must match firmware!!
CURRENT_PROBE_PERCENTAGE = 0.5

MOVE_DELAY_SECONDS = 0.5

GCODE_HOME = 'G28.2'
GCODE_MOVE = 'G0Z{position}C{current}'
GCODE_GET_POSITION = 'M114.2'
GCODE_PROBE = 'G38.2'
GCODE_GET_PROBE_POS = 'M836'
GCODE_GET_INFO = 'M115'

original_results_file_path = '{id}_{date}.txt'
results_file_path = ''


def connect_to_mag_deck():
    ports = []
    for p in comports():
        if p.pid == PID_MAGDECK:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a MagDeck connected over USB')
    for p in ports:
        try:
            return serial.Serial(p, 115200, timeout=10)
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to MagDeck')



def send_command(port, data):
    ack = 'ok\r\nok\r\n'
    port.reset_input_buffer()
    port.write(data.encode() + b'\r\n')
    res = port.read_until(ack.encode()).decode()
    res = res.replace(ack, '')
    return res


def wait_for_button_click():
    if not os.environ.get('RUNNING_ON_PI'):
        return
    while robot._driver.read_button():
        pass
    while not robot._driver.read_button():
        pass
    while robot._driver.read_button():
        pass


def check_if_exit_button():
    if os.environ.get('RUNNING_ON_PI') and robot._driver.read_button():
        raise Exception('Exit button pressed')


def assert_magdeck_has_serial(port):
    res = send_command(port, GCODE_GET_INFO)
    res = res.split(' ')[:3]
    info = {
        r.split(':')[0]: r.split(':')[1]
        for r in res
    }
    assert 'MD' in info['serial']
    assert 'mag' in info['model']
    return info['serial']


def home(port):
    send_command(port, GCODE_HOME)


def move(port, pos):
    current = round(FIRMWARE_DEFAULT_MOVE_CURRENT * CURRENT_MOVE_PERCENTAGE, 2)
    cmd = GCODE_MOVE.format(position=pos, current=current)
    send_command(port, cmd)


def position(port):
    res = send_command(port, GCODE_GET_POSITION)
    return float(res.strip().split(':')[-1])


def test_for_skipping(port):
    position_near_endstop = -FIRMWARE_HOMING_RETRACT + SKIPPING_TOLERANCE
    move(port, position_near_endstop)
    assert position(port) == position_near_endstop
    move(port, -FIRMWARE_HOMING_RETRACT - SKIPPING_TOLERANCE)
    assert position(port) == 0.0


def create_data_file(serial_number):
    global results_file_path, original_results_file_path

    # create the folder to save the test results
    dir_path = os.path.dirname(os.path.realpath(__file__))
    usb_path = '/mnt/usbdrive'
    if os.path.isdir(usb_path):
        dir_path = usb_path
    data_path = os.path.join(dir_path, 'mag-deck-QC')
    if not os.path.isdir(data_path):
        os.mkdir(data_path)

    date_string = datetime.utcfromtimestamp(time.time()).strftime(
        '%Y-%m-%d_%H-%M-%S')
    results_file_path = original_results_file_path.format(id=serial_number, date=date_string)
    results_file_path = os.path.join(data_path, results_file_path)
    write_line_to_file('Mag-Deck: {}'.format(serial_number))


def write_line_to_file(data_line, end='\r\n'):
    global results_file_path
    print(data_line, end=end, flush=True)
    with open(results_file_path, 'a+') as f:
        f.write(data_line + end)


def main(magdeck, cycles):

    write_line_to_file('\n\nStarting test, homing...\n')
    home(magdeck)

    fail_count = 0
    cycles_ran = 0

    for i in range(cycles):
        cycles_ran += 1
        write_line_to_file('  {0}/{1}: '.format(i + 1, TEST_CYCLES), end='')
        time.sleep(MOVE_DELAY_SECONDS)
        check_if_exit_button()
        move(magdeck, FIRMWARE_MAX_TRAVEL_DISTANCE)
        assert position(magdeck) == FIRMWARE_MAX_TRAVEL_DISTANCE
        time.sleep(MOVE_DELAY_SECONDS)
        check_if_exit_button()
        move(magdeck, TEST_BOTTOM_POS)
        assert position(magdeck) == TEST_BOTTOM_POS
        check_if_exit_button()
        test_for_skipping(magdeck)
        write_line_to_file('PASS')

    write_line_to_file('\n\nPASSED\n\n')


if __name__ == '__main__':
    robot._driver.turn_off_button_light()
    while True:
        if os.environ.get('RUNNING_ON_PI'):
            print('Press the BUTTON to start...')
            wait_for_button_click()
        else:
            input('Press ENTER when ready to run...')
        magdeck = None
        data_file_created = False
        try:
            robot._driver._set_button_light(blue=True)
            magdeck = connect_to_mag_deck()
            magdeck.reset_input_buffer()
            serial_number = assert_magdeck_has_serial(magdeck)
            create_data_file(serial_number)
            data_file_created = True
            main(magdeck, TEST_CYCLES)
            robot._driver._set_button_light(green=True)
        except Exception as e:
            print(str(e))
            if magdeck:
                magdeck.close()
            if data_file_created:
                write_line_to_file(str(e))
            robot._driver._set_button_light(red=True)

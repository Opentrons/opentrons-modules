import sys
import time

import serial
from serial.tools.list_ports import comports


TEST_CYCLES = 100
MAX_ALLOWED_FAILURES = 3

TEST_TOP_POS = 40
TEST_BOTTOM_POS = 1

SKIPPING_TOLERANCE = 0.25
CURRENT_PERCENTAGE = 0.75

MOVE_DELAY_SECONDS = 0.5

GCODE_HOME = 'G28.2'
GCODE_MOVE = 'G0Z{position}C{current}'
GCODE_GET_POSITION = 'M114.2'

FIRMWARE_DEFAULT_CURRENT = 0.5
FIRMWARE_HOMING_RETRACT = 2


def connect_to_mag_deck(default_port=None):
    ports = []
    for p in comports():
        if 'magdeck' in p.description.lower():
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a MagDeck connected over USB')
    if default_port and default_port in ports:
        ports = [default_port] + ports
    for p in ports:
        try:
            return serial.Serial(p, 115200, timeout=5)
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
    return res.replace(ack, '')


def home(port):
    send_command(port, GCODE_HOME)


def move(port, pos):
    current = round(FIRMWARE_DEFAULT_CURRENT * CURRENT_PERCENTAGE, 2)
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


def main(cycles, default_port=None):
    magdeck = connect_to_mag_deck(default_port)

    print('\n\nStarting test, homing...\n')
    home(magdeck)

    fail_count = 0
    cycles_ran = 0

    for i in range(cycles):
        cycles_ran += 1
        print('  {0}/{1}: '.format(i + 1, TEST_CYCLES), end='', flush=True)
        try:
            time.sleep(MOVE_DELAY_SECONDS)
            move(magdeck, TEST_TOP_POS)
            assert position(magdeck) == TEST_TOP_POS
            time.sleep(MOVE_DELAY_SECONDS)
            move(magdeck, TEST_BOTTOM_POS)
            assert position(magdeck) == TEST_BOTTOM_POS
            test_for_skipping(magdeck)
            print('PASS')
        except AssertionError:
            fail_count += 1
            print(' *** FAIL ***')
            if fail_count >= MAX_ALLOWED_FAILURES:
                break
        except KeyboardInterrupt:
            exit()

    if fail_count:
        print('\n\nFAILED {0} times after {1} cycles\n\n'.format(
            fail_count, cycles_ran))
    else:
        print('\n\nPASSED\n\n')


if __name__ == '__main__':
    main(TEST_CYCLES, default_port=sys.argv[-1])

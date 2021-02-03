import os
from datetime import datetime
import serial
import time

from serial.tools.list_ports import comports

from opentrons import robot
from opentrons.drivers import temp_deck


PID_TEMPDECK = 61075
PID_FTDI = 24577
PID_UNO = 67
PID_UNO_CHINESE = 29987

TARGET_TEMPERATURES = [3, 4, 95]

TOLERANCE_TO_PASS_TEST_LOW = 1
TOLERANCE_TO_PASS_TEST_HIGH = 2

SEC_WAIT_BEFORE_MEASURING = 5 * 60
SEC_TO_RECORD = 1 * 60

test_start_time = 0
original_results_file_path = '{id}_{date}.txt'
results_file_path = ''

length_samples_test_stabilize = 100
samples_test_stabilize = []

def connect_to_temp_deck():
    tempdeck = temp_deck.TempDeck()
    ports = []
    for p in comports():
        if p.pid == PID_TEMPDECK:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a TempDeck connected over USB')
    for p in ports:
        try:
            tempdeck.connect(p)
            return tempdeck
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to TempDeck')


def connect_to_external_sensor():
    ports = []
    for p in comports():
        if p.pid == PID_FTDI:
            ports.append(p.device)
        elif p.pid == PID_UNO:
            ports.append(p.device)
        elif p.pid == PID_UNO_CHINESE:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a External Sensor connected over USB')
    for p in ports:
        try:
            external_sensor = serial.Serial(p, 115200, timeout=1)
            time.sleep(3)
            return external_sensor
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to External Sensor')


def test_time_seconds():
    global test_start_time
    return round(time.time() - test_start_time, 2)


def read_temperature_sensors(external_sensor):
    external_sensor.reset_input_buffer()
    external_sensor.write(b'\r\n')
    time.sleep(0.025)
    try:
        res = external_sensor.readline().decode().strip()
        return float(res)
    except Exception:
        write_line_to_file('Error parsing "{}"'.format(res))
        raise Exception('Error parsing "{}"'.format(res))


def is_temp_arrived(tempdeck, target_temperature):
    global samples_test_stabilize, length_samples_test_stabilize
    td_temp, _ = read_temperatures(tempdeck)
    while len(samples_test_stabilize) >= length_samples_test_stabilize:
        samples_test_stabilize.pop(0)
    samples_test_stabilize.append(td_temp)
    if len(samples_test_stabilize) < length_samples_test_stabilize:
        return False
    if abs(min(samples_test_stabilize) - target_temperature) > 0.5:
        return False
    if abs(max(samples_test_stabilize) - target_temperature) > 0.5:
        return False
    return True


def is_finished_stabilizing(tempdeck, target_temperature, timestamp, time_to_stabilize):
    if not is_temp_arrived(tempdeck, target_temperature):
        return False
    return bool(timestamp + time_to_stabilize < time.time())


def read_temperatures(tempdeck, external_sensor=None):
    time.sleep(0.05)
    tempdeck.update_temperature()
    if not external_sensor:
        return (tempdeck.temperature, None)
    external_temp = read_temperature_sensors(external_sensor);
    return (tempdeck.temperature, external_temp)


def write_line_to_file(data_line):
    global results_file_path
    print(data_line)
    with open(results_file_path, 'a+') as f:
        f.write(data_line + '\r\n')


def get_sensors_delta(tempdeck, external_sensor):
    tempdeck_temp, external_temp = read_temperatures(tempdeck, external_sensor)
    external_delta = external_temp - tempdeck_temp
    return external_delta


def analyze_results(results):
    write_line_to_file('\n\n')
    for r in results:
        write_line_to_file(
            '{0}C:\tAverage Delta: {1}\tAverage Abs Delta{2}:'.format(
            r['target'], r['average'], r['absolute']))
    write_line_to_file('\n\n')
    did_pass = True
    for r in results:
        pass_thresh = TOLERANCE_TO_PASS_TEST_HIGH
        if r['target'] < 25:
            pass_thresh = TOLERANCE_TO_PASS_TEST_LOW
        if abs(r['average']) > pass_thresh:
            write_line_to_file('*** FAIL ***')
            did_pass = False
            break
        if r['absolute'] > pass_thresh:
            write_line_to_file('*** FAIL ***')
            did_pass = False
            break
    if did_pass:
        write_line_to_file('PASS')
    write_line_to_file('\n\n')
    return did_pass


def assert_tempdeck_has_serial(tempdeck):
    info = tempdeck.get_device_info()
    assert info.get('serial') != 'none'
    assert info.get('model') != 'none'


def create_data_file(tempdeck):
    global results_file_path, original_results_file_path, dir_path

    # create the folder to save the test results
    dir_path = os.path.dirname(os.path.realpath(__file__))
    usb_path = '/mnt/usbdrive'
    if os.path.isdir(usb_path):
        dir_path = usb_path
    data_path = os.path.join(dir_path, 'temp-deck-QC')
    if not os.path.isdir(data_path):
        os.mkdir(data_path)

    serial_number = tempdeck.get_device_info().get('serial')
    if not serial_number:
        raise RuntimeError('Module has no ID, please write ID before testing')
    date_string = datetime.utcfromtimestamp(time.time()).strftime(
        '%Y-%m-%d_%H-%M-%S')
    results_file_path = original_results_file_path.format(id=serial_number, date=date_string)
    results_file_path = os.path.join(data_path, results_file_path)
    write_line_to_file('Temp-Deck: {}'.format(serial_number))


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


def run_test(tempdeck, sensor, targets):
    global test_start_time
    try:
        assert_tempdeck_has_serial(tempdeck)
    except AssertionError as e:
        write_line_to_file('\n\tFAIL: Please write ID to module\n')
        raise Exception('FAIL: Please write ID to module')
    test_start_time = time.time()
    results = []
    write_line_to_file('\nStarting Test')
    for i in range(len(targets)):
        write_line_to_file('Setting temperature to {} Celsius'.format(targets[i]))
        tempdeck.set_temperature(targets[i])
        time.sleep(1)
        timestamp = time.time()
        seconds_passed = 0
        while not is_temp_arrived(tempdeck, targets[i]):
            check_if_exit_button()
            if time.time() - timestamp > 30:
                seconds_passed += 30
                write_line_to_file('Waiting {0} seconds to reach {1}'.format(
                    int(seconds_passed), targets[i]))
                timestamp = time.time()
        tstamp = time.time()
        if i == 0:
            write_line_to_file('Successfully reached {} degrees'.format(targets[i]))
            continue
        else:
            write_line_to_file('Waiting for {0} seconds before measuring...'.format(
                SEC_WAIT_BEFORE_MEASURING))
            while time.time() - tstamp < SEC_WAIT_BEFORE_MEASURING:
                check_if_exit_button()
                pass
            delta_temperatures = []
            tstamp = time.time()
            write_line_to_file('Measuring temperature for {0} seconds...'.format(
                SEC_TO_RECORD))
            while not is_finished_stabilizing(tempdeck, targets[i], tstamp, SEC_TO_RECORD):
                check_if_exit_button()
                delta_temp_thermistor = get_sensors_delta(tempdeck, sensor)
                delta_temperatures.append(delta_temp_thermistor)
                write_line_to_file('Plate is {0}C, Sensor is {1}C'.format(
                    round(tempdeck.temperature, 2),
                    round(tempdeck.temperature + delta_temp_thermistor, 2)
                ))
            absolute_deltas = [abs(d) for d in delta_temperatures]
            average_delta = round(
                sum(delta_temperatures) / len(delta_temperatures), 2)
            average_absolute_delta = round(
                sum(absolute_deltas) / len(delta_temperatures), 2)
            results.append({
                'target': targets[i],
                'average': average_delta,
                'absolute': average_absolute_delta
            })
    result = analyze_results(results)
    tempdeck.set_temperature(0)
    time.sleep(1)
    while tempdeck.temperature > 50:
        check_if_exit_button()
        time.sleep(1)
        tempdeck.update_temperature()
    time.sleep(1)
    tempdeck.disengage()
    time.sleep(1)
    return result


def main():
    data_file_created = False
    try:
        robot._driver._set_button_light(blue=True)
        sensor = connect_to_external_sensor()
        tempdeck = connect_to_temp_deck()
        create_data_file(tempdeck)
        data_file_created = True
        if run_test(tempdeck, sensor, TARGET_TEMPERATURES):
            robot._driver._set_button_light(green=True)
        else:
            robot._driver._set_button_light(red=True)
    except Exception as e:
        if data_file_created:
            write_line_to_file(str(e))
        robot._driver._set_button_light(red=True)
    finally:
        try:
            sensor.close()
            tempdeck.disengage()
            tempdeck.disconnect()
        except:
            pass


if __name__ == '__main__':
    robot._driver.turn_off_button_light()
    while True:
        if os.environ.get('RUNNING_ON_PI'):
            print('Press the BUTTON to start...')
            wait_for_button_click()
        else:
            input('Press ENTER when ready to run...')
        main()

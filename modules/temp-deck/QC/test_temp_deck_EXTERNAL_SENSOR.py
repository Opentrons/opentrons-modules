# this must happen before attempting to import opentrons
import os
os.environ['OVERRIDE_SETTINGS_DIR'] = './data'

from datetime import datetime
import serial
import time

from serial.tools.list_ports import comports

from opentrons.drivers import temp_deck


PID_TEMPDECK = 61075
PID_FTDI = 24577

TARGET_TEMPERATURES = [4, 95]

SEC_WAIT_BEFORE_MEASURING = 3 * 60
SEC_TO_RECORD = 1 * 60

test_start_time = 0
csv_file_path = './data/{id}_{date}.csv'

length_samples_test_stabilize = 100
samples_test_stabilize = []

def connect_to_temp_deck(default_port=None):
    tempdeck = temp_deck.TempDeck()
    ports = []
    for p in comports():
        if p.pid == PID_TEMPDECK:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a TempDeck connected over USB')
    if default_port and default_port in ports:
        ports = [default_port] + ports
    for p in ports:
        try:
            tempdeck.connect(p)
            return tempdeck
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to TempDeck')


def connect_to_external_sensor(default_port=None):
    ports = []
    for p in comports():
        if p.pid == PID_FTDI:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a External Sensor connected over USB')
    if default_port and default_port in ports:
        ports = [default_port] + ports
    for p in ports:
        try:
            external_sensor = serial.Serial(p, 9600, timeout=1)
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
        print('Error parsing "{}"'.format(res))
        exit()


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


def is_finished_stabilizing(target_temperature, timestamp, time_to_stabilize):
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


def write_to_file(data_line):
    global csv_file_path
    with open(csv_file_path, 'a+') as f:
        f.write(data_line + '\r\n')


def log_temperatures(tempdeck, external_sensor):
    tempdeck_temp, external_temp = read_temperatures(tempdeck, external_sensor)
    csv_line = '{0}, {1}, {2}'.format(
        test_time_seconds(),
        round(tempdeck_temp, 2),
        round(external_temp, 2))
    print(csv_line)
    write_to_file(csv_line)
    external_delta = abs(external_temp - tempdeck_temp)
    return external_delta  # return the absolute DELTA


def analyze_results(results):
    print('\n\n\n\n')
    for r in results:
        print(
            '{0}C:\tAvg: {1},\tMin: {2},\tMax: {3}'.format(
            r['target'], r['average'], r['min'], r['max']))
    print('\n\n\n\n')
    did_fail = False
    for r in results:
        if r['average'] > 1:
            print('*** FAIL ***')
            did_fail = True
            break
    if not did_fail:
        print('PASS')
    print('\n\n\n\n')


def main(tempdeck, sensor, targets):
    global test_start_time, csv_file_path
    test_start_time = time.time()
    serial_number = tempdeck.get_device_info().get('serial')
    if not serial_number:
        raise RuntimeError('Module has no ID, please write ID before testing')
    date_string = datetime.utcfromtimestamp(time.time()).strftime(
        '%Y-%m-%d_%H-%M-%S')
    csv_file_path = csv_file_path.format(id=serial_number, date=date_string)
    write_to_file('seconds, internal, external')
    results = []
    for i in range(len(targets)):
        tempdeck.set_temperature(targets[i])
        while not is_temp_arrived(tempdeck, targets[i]):
            log_temperatures(tempdeck, sensor)
        tstamp = time.time()
        while time.time() - tstamp < SEC_WAIT_BEFORE_MEASURING:
            log_temperatures(tempdeck, sensor)
        delta_temperatures = []
        tstamp = time.time()
        while not is_finished_stabilizing(targets[i], tstamp, SEC_TO_RECORD):
            delta_temp_thermistor = log_temperatures(tempdeck, sensor)
            delta_temperatures.append(delta_temp_thermistor)
        average_delta = round(
            sum(delta_temperatures) / len(delta_temperatures), 2)
        min_delta = round(min(delta_temperatures), 2)
        max_delta = round(max(delta_temperatures), 2)
        results.append({
            'target': targets[i],
            'average': average_delta,
            'min': min_delta,
            'max': max_delta
        })
    analyze_results(results)



if __name__ == '__main__':
    try:
        sensor = connect_to_external_sensor()
        tempdeck = connect_to_temp_deck()
        main(tempdeck, sensor, TARGET_TEMPERATURES)
    finally:
        del os.environ['OVERRIDE_SETTINGS_DIR']
        tempdeck.disengage()

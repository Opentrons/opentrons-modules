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

SECONDS_WAIT_BEFORE_MEASURING = 3 * 60
SECONDS_TO_RECORD = 1 * 60

test_start_time = 0
csv_file_path = './data/{id}_{date}'

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


def connect_to_ir_sensor(default_port=None):
    ports = []
    for p in comports():
        if p.pid == PID_FTDI:
            ports.append(p.device)
    if not ports:
        raise RuntimeError('Can not find a IR Sensor connected over USB')
    if default_port and default_port in ports:
        ports = [default_port] + ports
    for p in ports:
        try:
            ir_sensor = serial.Serial(p, 9600, timeout=1)
            time.sleep(3)
            return ir_sensor
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to IR Sensor')


def test_time_seconds():
    global test_start_time
    return round(time.time() - test_start_time, 2)


def read_temperature_sensors(ir_sensor):
    ir_sensor.reset_input_buffer()
    ir_sensor.write(b'\r\n')
    time.sleep(0.025)
    try:
        res = ir_sensor.readline().decode().strip()
        both_temps = res.split(',')[:2]
        return (float(both_temps[0]), float(both_temps[1]))
    except Exception:
        print('Error parsing "{}"'.format(res))
        exit()


def is_temp_arrived(tempdeck, target_temperature):
    td_temp, _, _ = read_temperatures(tempdeck)
    return bool(abs(td_temp - target_temperature) <= 0.5)


def is_finished_stabilizing(target_temperature, timestamp, time_to_stabilize):
    if not is_temp_arrived(tempdeck, target_temperature):
        return False
    return bool(timestamp + time_to_stabilize < time.time())


def read_temperatures(tempdeck, ir_sensor=None):
    time.sleep(0.05)
    tempdeck.update_temperature()
    if not ir_sensor:
        return (tempdeck.temperature, None, None)
    ir_temp, thermistor_temp = read_temperature_sensors(ir_sensor);
    return (tempdeck.temperature, ir_temp, thermistor_temp)


def write_to_file(data_line):
    global csv_file_path
    with open(csv_file_path, 'a+') as f:
        f.write(data_line + '\r\n')


def log_temperatures(tempdeck, ir_sensor):
    tempdeck_temp, ir_temp, thermistor_temp = read_temperatures(tempdeck, ir_sensor)
    csv_line = '{0}, {1}, {2}, {3}'.format(
        test_time_seconds(),
        round(tempdeck_temp, 2),
        round(ir_temp, 2),
        round(thermistor_temp, 2))
    print(csv_line)
    write_to_file(csv_line)
    ir_delta = abs(ir_temp - tempdeck_temp)
    thermistor_delta = abs(thermistor_temp - tempdeck_temp)
    return (ir_delta, thermistor_delta)  # return the absolute DELTA


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
    write_to_file('seconds, tempdeck, ir, thermistor')
    results = []
    for i in range(len(targets)):
        tempdeck.set_temperature(targets[i])
        while not is_temp_arrived(tempdeck, targets[i]):
            log_temperatures(tempdeck, sensor)
        timestamp = time.time()
        while time.time() - timestamp < SECONDS_WAIT_BEFORE_MEASURING:
            log_temperatures(tempdeck, sensor)
        delta_temperatures = []
        timestamp = time.time()
        while not is_finished_stabilizing(targets[i], timestamp, SECONDS_TO_RECORD):
            delta_temp_ir, delta_temp_thermistor = log_temperatures(tempdeck, sensor)
            if targets[i] < 25:
                delta_temperatures.append(delta_temp_thermistor)
            else:
                delta_temperatures.append(delta_temp_ir)
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
        sensor = connect_to_ir_sensor()
        tempdeck = connect_to_temp_deck()
        main(tempdeck, sensor, TARGET_TEMPERATURES)
    finally:
        del os.environ['OVERRIDE_SETTINGS_DIR']
        tempdeck.disengage()

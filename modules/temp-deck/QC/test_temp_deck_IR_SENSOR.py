# this must happen before attempting to import opentrons
import os
os.environ['OVERRIDE_SETTINGS_DIR'] = './data'

import serial
import time

from serial.tools.list_ports import comports

from opentrons.drivers import temp_deck


PID_TEMPDECK = 61075
PID_FTDI = 24577

TARGET_TEMPERATURES = [4, 95]

SECONDS_WAIT_AT_TEMP = 60


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
            ir_sensor = serial.Serial(p, 115200, timeout=1)
            time.sleep(2)
            return ir_sensor
        except KeyboardInterrupt:
            exit()
        except:
            pass
    raise Exception('Could not connect to IR Sensor')


def read_ir_temp(ir_sensor):
    ir_sensor.reset_input_buffer()
    ir_sensor.write(b'\r\n')
    time.sleep(0.1)
    try:
        res = ir_sensor.readline().decode().strip()
        return float(res)
    except Exception:
        print('Error parsing "{}"'.format(res))
        exit()


def is_temp_arrived(tempdeck, target_temperature):
    td_temp, _ = read_average_temperatures(tempdeck)
    return bool(abs(td_temp - target_temperature) <= 0.5)


def is_finished_stabilizing(target_temperature, timestamp, time_to_stabilize):
    if not is_temp_arrived(tempdeck, target_temperature):
        return False
    return bool(timestamp + time_to_stabilize < time.time())


def read_average_temperatures(tempdeck, ir_sensor=None, samples=10):
    average_td_temp = 0
    average_ir_temp = 0
    for i in range(samples):
        tempdeck.update_temperature()
        average_td_temp += tempdeck.temperature
        if ir_sensor:
            average_ir_temp += read_ir_temp(ir_sensor);
        time.sleep(0.025)
    average_td_temp = average_td_temp / samples
    average_ir_temp = average_ir_temp / samples
    return (average_td_temp, average_ir_temp)


def log_temperatures(tempdeck, ir_sensor):
    tempdeck_temp, ir_temp = read_average_temperatures(tempdeck, ir_sensor)
    print(
        'Module: {0},\tIR Sensor: {1},\tDelta: {2}'.format(
            round(tempdeck_temp, 2),
            round(ir_temp, 2),
            round(ir_temp - tempdeck_temp, 2)
        )
    )
    return abs(ir_temp - tempdeck_temp)  # return the absolute DELTA


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
    results = []
    for i in range(len(targets)):
        tempdeck.set_temperature(targets[i])
        while not is_temp_arrived(tempdeck, targets[i]):
            log_temperatures(tempdeck, sensor)
        timestamp = time.time()
        delta_temperatures = []
        while not is_finished_stabilizing(targets[i], timestamp, SECONDS_WAIT_AT_TEMP):
            delta_temp = log_temperatures(tempdeck, sensor)
            delta_temperatures.append(delta_temp)
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

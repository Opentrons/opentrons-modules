#!/usr/bin/env python3
import argparse
import itertools
import io
import datetime
import time
import json
import re
from typing import Tuple, List

import serial
from serial.tools.list_ports import grep
from matplotlib import pyplot as pp


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description='Test a heater-shaker for stability')
    p.add_argument('-o', '--output', choices=['plot','json'],
                   action='append',
                   help='Output type; plot draws a matplotlib graph, json dumps a data blob. Pass twice to do both.')
    p.add_argument('-f', '--file', type=argparse.FileType('w'), help='Path to write the json blob (ignored if not -ojson)',
                   default=None, dest='output_file')
    p.add_argument('-p','--port', type=str, help='Path to serial port to open. If not specified, will try to find by usb details', default=None)
    p.add_argument('speed', metavar='SPEED', type=float, help='Target speed to regulate at')
    p.add_argument('-w', '--warm-up', type=float, dest='warmup_speed',
                    help='Speed to hold before going to the speed argument (unplotted)',
                    default=None)
    p.add_argument('-t', '--time', type=float, help='Time to sample after setting the target speed. If not specified, tries to await stability',
                   default=None)
    p.add_argument('-s', '--sampling-time', dest='sample_time', type=float, help='How frequently to sample speed', default=0.1)
    p.add_argument('-a', '--acceleration', help='acceleration  (rpm/s)', type=int, default=1000)
    p.add_argument('--stability-criterion', type=float, default=2, help='square dev from target threshold for stability (rpm^2)')
    p.add_argument('--stability-window', type=float, default=1, help='trailing window duration for stability check (s)')
    return p


def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*eater.*haker.*'))
        if not avail:
            raise RuntimeError("could not find heater-shaker")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)

def guard_error(res: bytes, prefix: bytes=  None):
    if res.startswith(b'ERR'):
        raise RuntimeError(res)
    if prefix and not res.startswith(prefix):
        raise RuntimeError(f'incorrect response: {res} (expected prefix {prefix})')


_SPEED_RE = re.compile('^M123 C(?P<current>.+) T(?P<target>.+) OK\n')
def get_speed(ser: serial.Serial) -> Tuple[float, float]:
    ser.write(b'M123\n')
    res = ser.readline()
    guard_error(res, b'M123')
    res_s = res.decode()
    match = re.match(_SPEED_RE, res_s)
    return float(match.group('current')), float(match.group('target'))


def stable(data: List[Tuple[float, float]],
           target: float,
           window: datetime.timedelta,
           criterion: float):
    last_timestamp = data[-1][0]
    if last_timestamp - data[0][0] < window.total_seconds():
        return False
    tocheck = list(itertools.takewhile(lambda sample: sample[0] > (last_timestamp - window.total_seconds()), reversed(data)))
    dev = [(sample[1]-target)**2 for sample in tocheck]
    if any([d > criterion for d in dev]):
        return False
    return True


def sample_speed(ser: serial.Serial, target: float,
                 stab_window: datetime.timedelta,
                 stab_criterion: float,
                 sample_time: datetime.timedelta,
                 timeout: datetime.timedelta = None,
                 fixed_time: datetime.timedelta = None,
                 min_time: datetime.timedelta =  None):
    sample_time = sample_time or datetime.timedelta(milliseconds=100)
    min_time = min_time or datetime.timedelta(seconds=2)
    if fixed_time:
        min_time = min(min_time, fixed_time)
    if timeout:
        min_time = min(min_time, timeout)
    print("Beginning data sampling (ctrl-c ONCE stops early)")
    running_since = datetime.datetime.now()
    data = []
    try:
        while True:
            speed, target = get_speed(ser)
            now = datetime.datetime.now()
            data.append(((now - running_since).total_seconds(), speed))
            time.sleep(sample_time.total_seconds())
            if now - running_since < min_time:
                continue
            if timeout and now - running_since > timeout:
                raise RuntimeError('timeout exceeded')
            if fixed_time and now - running_since > fixed_time:
                print(
                    "Data sampling complete after (fixed time elapsed) {(now-running_since).total_seconds()}s")
                break
            if not fixed_time and (abs(speed-target) < 2) and stable(data, target, stab_window, stab_criterion):
                print(
                    "Data sampling complete after (speed stable) {(now-running_since).total_seconds()}")
                break

    except KeyboardInterrupt:
        now = datetime.datetime.now()
        print(f"Data sampling complete after (keyboard interrupt) {(now-running_since).total_seconds()}")
    print(f"Got {len(data)} samples")
    return data


def set_speed(ser: serial.Serial, speed: float):
    print(f"Setting speed to {speed}RPM")
    ser.write(f'M3 S{int(speed)}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M3')
    print(res)


def set_acceleration(ser: serial.Serial, acceleration: float):
    print(f'Setting acceleration to {acceleration}RPM/s')
    ser.write(f'M204 S{acceleration}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M204')

def do_warmup(ser: serial.Serial, speed: float):
    print(f"Doing warmup phase at {speed}RPM")
    then = datetime.datetime.now()
    set_speed(ser, speed)
    sample_speed(ser, speed, sample_time=datetime.timedelta(seconds=1), stab_window=datetime.timedelta(seconds=0.5), stab_criterion=5)
    now = datetime.datetime.now()
    print(f"Warmup phase complete after {(now-then).total_seconds()}")


def do_run(ser: serial.Serial,
           speed: float,
           stab_window: datetime.timedelta,
           stab_criterion: float,
           sampletime: float = None,
           fixedtime: float = None):
    print(f"Doing main sample run")
    then = datetime.datetime.now()
    set_speed(ser, speed)
    data = sample_speed(
        ser,
        speed,
        stab_window,
        stab_criterion,
        sampletime and datetime.timedelta(seconds=sampletime),
        None,
        fixedtime and datetime.timedelta(seconds=fixedtime))
    now = datetime.datetime.now()
    set_speed(ser, 0)
    print(f"Data acquisition complete after {(now-then).total_seconds()}")
    return data


def plot_data(data: List[Tuple[float, float]], speed: float, stab_criterion: float = None):
    pp.plot([sample[0] for sample in data], [sample[1] for sample in data], label='speed')
    pp.axhline(speed, color='red', linestyle='-.', label='target speed')
    if stab_criterion:
        pp.axhspan(speed - stab_criterion**2, speed + stab_criterion**2,
                   alpha=0.1, color='r')
    pp.ylabel('speed (rpm)')
    pp.xlabel('time (sec)')
    pp.title(f'Speed/Time (target={args.speed}rpm)')
    pp.show()

def write_data(data: List[Tuple[float, float]], speed: float, destfile: io.StringIO):
    timestamp = datetime.datetime.now()
    output_file = destfile or open(f'./heater-shaker-speed-{timestamp}.json', 'w')
    json.dump({'meta': {'name': 'heater-shaker motor consistency run',
                        'format': ['time', 'speed'], 'units': ['s', 'rpm'],
                        'takenAt': str(timestamp)},
                   'data': data}, output_file)



if __name__ == '__main__':
    parser = build_parser()
    args = parser.parse_args()
    port = build_serial(args.port)
    set_acceleration(port, args.acceleration)
    if args.warmup_speed:
        do_warmup(port, args.warmup_speed)
    data = do_run(port, args.speed,
                  datetime.timedelta(seconds=args.stability_window),
                  args.stability_criterion,
                  args.sample_time, args.time)
    port.close()
    if 'json' in args.output:
        write_data(data, args.speed, args.output_file)
    if not args.output or 'plot' in args.output:
        plot_data(data, args.speed, args.stability_criterion)

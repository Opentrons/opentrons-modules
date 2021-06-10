#!/usr/bin/env python3
import argparse
import math
from copy import copy
import csv
from dataclasses import dataclass, asdict
import itertools
import io
import datetime
import time
import json
import re
from typing import Any, Dict, Tuple, List, Optional

import serial
from serial.tools.list_ports import grep
from matplotlib import pyplot as pp, gridspec

DEFAULT_PID_CONSTANTS = {'kp': 2372, 'ki': 2624, 'kd': 0}

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description='Test a heater-shaker for stability')
    p.add_argument('-o', '--output', choices=['plot','json', 'csv'],
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
    p.add_argument('-s', '--sampling-time', dest='sampling_time', type=float, help='How frequently to sample speed', default=0.1)
    p.add_argument('-a', '--acceleration', help='acceleration  (rpm/s)', type=int, default=1000)
    p.add_argument('--stability-criterion', type=float, default=2, help='square dev from target threshold for stability (rpm^2)')
    p.add_argument('--stability-window', type=float, default=1, help='trailing window duration for stability check (s)')
    p.add_argument('-kp', dest='kp_override', type=float, help='Kp override')
    p.add_argument('-ki', dest='ki_override', type=float, help='Ki override')
    p.add_argument('-kd', dest='kd_override', type=float, help='Kd override')
    return p


@dataclass
class RunConfig:
    speed: float
    warmup_speed: Optional[float]
    fixed_time_sample: Optional[datetime.timedelta]
    sampling_time: datetime.timedelta
    acceleration: float
    stability_criterion: float
    stability_window: datetime.timedelta
    pid_constants: Dict[str, float]
    pid_constants_need_setting: bool

    @classmethod
    def from_args(cls, args: argparse.Namespace):
        pid_constants = copy(DEFAULT_PID_CONSTANTS)
        needs_setting = False
        if args.kp_override:
            pid_constants['kp'] = args.kp_override
            needs_setting = True
        if args.ki_override:
            pid_constants['ki'] = args.ki_override
            needs_setting = True
        if args.kd_override:
            pid_constants['kd'] = args.kd_override
            needs_setting = True
        return cls(speed=args.speed,
                   warmup_speed=args.warmup_speed,
                   fixed_time_sample=args.time and datetime.timedelta(seconds=args.time),
                   sampling_time=datetime.timedelta(seconds=args.sampling_time),
                   acceleration=args.acceleration,
                   stability_criterion=args.stability_criterion,
                   stability_window=datetime.timedelta(seconds=args.stability_window),
                   pid_constants=pid_constants,
                   pid_constants_need_setting=needs_setting)

    def dict_for_save(self) -> Dict[str, Any]:
        mydict = asdict(self)
        for k, v in mydict.items():
            if isinstance(v, datetime.timedelta):
                mydict[k] = v.total_seconds()
        mydict.pop('pid_constants_need_setting')
        return mydict

    def rowiterable_for_save(self) -> List[List[Any]]:
        return [
            ['speed (rpm)', self.speed],
            ['warmup speed (rpm)',  self.warmup_speed or 0],
            ['fixed sample time  (s)',
             (self.fixed_time_sample and self.fixed_time_sample.total_seconds())
             or 'not set'],
            ['acceleration (rpm/s)', self.acceleration],
            ['stability criterion (rpm^2)', self.stability_criterion],
            ['stability window (s)',  self.stability_window.total_seconds()],
            ['kp', self.pid_constants['kp']],
            ['ki', self.pid_constants['ki']],
            ['kd', self.pid_constants['kd']]
        ]


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


def set_speed_pid(ser: serial.Serial,
                  kp: float, ki: float, kd: float):
    print(f'Overriding PID constants to kp={kp}, ki={ki}, kd={kd}')
    ser.write(f'M301 TM P{kp} I{ki} D{kd}\n'.encode())
    guard_error(ser.readline(), b'M301')


def sample_speed(ser: serial.Serial,
                 config: RunConfig,
                 min_time: datetime.timedelta =  None):
    min_time = min_time or datetime.timedelta(seconds=2)
    if config.fixed_time_sample:
        min_time = min(min_time, config.fixed_time_sample)
    print("Beginning data sampling (ctrl-c ONCE stops early)")
    running_since = datetime.datetime.now()
    data = []
    try:
        while True:
            speed, target = get_speed(ser)
            now = datetime.datetime.now()
            data.append(((now - running_since).total_seconds(), speed))
            time.sleep(config.sampling_time.total_seconds())
            if now - running_since < min_time:
                continue
            if config.fixed_time_sample and now - running_since > config.fixed_time_sample:
                print(
                    "Data sampling complete after (fixed time elapsed) {(now-running_since).total_seconds()}s")
                break
            if (not config.fixed_time_sample)\
               and (abs(speed-target) < 2) \
               and stable(data, target, config.stability_window, config.stability_criterion):
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
    warmup_config = RunConfig(
        speed=speed,
        stability_window=datetime.timedelta(seconds=2),
        stability_criterion=20,
        sampling_time=datetime.timedelta(milliseconds=500))
    sample_speed(ser, warmup_config)
    now = datetime.datetime.now()
    print(f"Warmup phase complete after {(now-then).total_seconds()}")


def do_run(ser: serial.Serial, config: RunConfig):
    print(f"Doing main sample run")
    then = datetime.datetime.now()
    set_speed(ser, config.speed)
    data = sample_speed(ser, config)
    now = datetime.datetime.now()
    set_speed(ser, 0)
    print(f"Data acquisition complete after {(now-then).total_seconds()}")
    return data


def plot_data(data: List[Tuple[float, float]], config: RunConfig):
    fig = pp.figure()
    gs = gridspec.GridSpec(2, 1, height_ratios=[4, 1], hspace=0.5)
    axes = pp.subplot(gs[0])
    bwaxes = pp.subplot(gs[1])
    axes.plot([sample[0] for sample in data], [sample[1] for sample in data], label='speed')
    axes.axhline(config.speed, color='red', linestyle='-.', label='target speed')
    axes.axhspan(config.speed - config.stability_criterion**2, config.speed + config.stability_criterion**2,
                 alpha=0.1, color='r')
    # options string: we'd like to put the options on the plot mostly so that if
    # it's saved as an image, we keep them around. it'll be a big chunk of text,
    # so we autolayout it to either the bottom right or top right depending on
    # whether most of the data is on the bottom or the top (because we capture
    # a ramp and then regulation period, it'll usually have a blank space)
    options_string = '\n'.join(f'{k}: {val}' for k, val in config.dict_for_save().items())
    yavg = sum([sample[1] for sample in data]) / len(data)
    ylims = axes.get_ylim()
    if yavg < sum(ylims)/2:
        pos = (axes.get_xlim()[1], ylims[1])
        valign = 'top'
    else:
        pos = (axes.get_xlim()[1], ylims[0])
        valign = 'bottom'
    # once we render the text we  know what size it is, so we can shift it in
    # to the visible area
    text = axes.text(
        *pos,
        options_string,
        verticalalignment=valign,
        horizontalalignment='left')
    box = text.get_window_extent(
        fig.canvas.get_renderer()).transformed(axes.transData.inverted())
    def matched(sample):
        if data[0][1] < config.speed:
            return sample[1] > config.speed
        else:
            return sample[1] < config.speed
    crossings = [samp for samp in data if matched(samp)]
    if crossings:
        crossing = crossings[0]
        axes.annotate(f'target crossing:\nt={crossing[0]}',
                      crossing, (0.01, 0.8), 'axes fraction')

    # shift right to visible area
    minx, maxx = sorted(box.intervalx)
    miny, maxy = sorted(box.intervaly)
    newx = minx - (maxx-minx)
    if yavg < sum(ylims)/2:
        # line is low, text is high
        newy = miny - (maxy-miny)
    else:
        newy = miny
    text.set_position((newx, newy))

    #filtered_samples  =  list(itertools.dropwhile(
    #lambda samp: (samp[0] < 0.1 * data[-1][0]) or abs(samp[1]-config.speed) < 0.1*config.speed, data))
    filtered_samples = data[len(data)//2:-1]
    box_data = [filtered_samp[1] for filtered_samp in filtered_samples]
    bwaxes.boxplot(box_data, vert=False)
    bp = bwaxes.set_xlabel('speed (rpm)')
    bwaxes.set_title(f'Speed sample data after first target intercept')
    axes.indicate_inset([filtered_samples[0][0], min(box_data),
                         filtered_samples[-1][0]-filtered_samples[0][0],
                         max(box_data)-min(box_data)],
                        bwaxes)

    axes.set_ylabel('speed (rpm)')
    axes.set_xlabel('time (sec)')
    axes.set_title(f'Speed/Time (target={config.speed}rpm)')
    pp.show()

def write_json(data: List[Tuple[float, float]], config: RunConfig, destfile: io.StringIO):
    timestamp = datetime.datetime.now()
    output_file = destfile or open(f'./heater-shaker-speed-{config.speed}rpm-{timestamp}.json', 'w')
    json.dump({'meta': {'name': 'heater-shaker motor consistency run',
                        'config': config.dict_for_save(),
                        'format': ['time', 'speed'], 'units': ['s', 'rpm'],
                        'takenAt': str(timestamp)},
                   'data': data}, output_file)

def write_csv(data: List[Tuple[float, float]], config: RunConfig, destfile: io.StringIO):
    timestamp = datetime.datetime.now()
    output_file = destfile or open(f'./heater-shaker-speed-{config.speed}rpm-{timestamp}.csv', 'w')
    writer = csv.writer(output_file)
    writer.writerows(
        [['title', 'heater shaker speed'],
         ['taken at', str(timestamp)]])
    writer.writerows(config.rowiterable_for_save())
    writer.writerows([['time', 'speed'],
                      ['seconds', 'rpm']])
    writer.writerows(data)

if __name__ == '__main__':
    parser = build_parser()
    args = parser.parse_args()
    if args.output and 'json' in args.output and 'csv' in args.output:
        raise RuntimeError("please pick just one of json or csv")
    port = build_serial(args.port)
    config = RunConfig.from_args(args)
    set_acceleration(port, config.acceleration)
    if config.pid_constants_need_setting:
        set_speed_pid(port, **config.pid_constants)
    if config.warmup_speed:
        do_warmup(port, config.warmup_speed)
    data = do_run(port, config)
    port.close()
    output_type = args.output or ['plot']
    if 'json' in output_type:
        write_json(data, config, args.output_file)
    if 'csv' in output_type:
        write_csv(data, config, args.output_file)
    if 'plot' in output_type:
        plot_data(data, config)

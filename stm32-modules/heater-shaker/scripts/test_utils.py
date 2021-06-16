#!/usr/bin/env python3

from matplotlib import pyplot as pp, gridspec
import argparse
import csv
import io
import json
import re
import serial
import datetime
import time

from serial.tools.list_ports import grep
from typing import Any, Callable, Dict, Tuple, List, Optional

from dataclasses import dataclass, asdict

def build_parent_parser(desc: str) -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=desc)
    p.add_argument('-o', '--output', choices=['plot','json', 'csv'],
                   action='append',
                   help='Output type; plot draws a matplotlib graph, json dumps a data blob. Pass twice to do both.')
    p.add_argument('-f', '--file', type=argparse.FileType('w'), help='Path to write the json blob (ignored if not -ojson)',
                   default=None, dest='output_file')
    p.add_argument('-p','--port', type=str, help='Path to serial port to open. If not specified, will try to find by usb details', default=None)
    p.add_argument('-s', '--sampling-time', dest='sampling_time', type=float, help='How frequently to sample in seconds', default=0.1)
    return p

@dataclass
class RunConfig:
    """
    Dataclass for configuration storage that will also help with displays
    """
    process: str
    process_unit: str
    target: float
    stability_criterion: float
    sampling_time: datetime.timedelta

    def dict_for_save(self) -> Dict[str, Any]:
        return asdict(self)

    def rowiterable_for_save(self) -> List[List[Any]]:
        return [
            [f'target {self.process} ({self.process_unit})'],
            ['sample time (s)', self.sampling_time.total_seconds()],
            [f'stability criterion ({self.process_unit}^2)', self.stability_criterion]
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


_TEMP_RE = re.compile('^M105 C(?P<current>.+) T(?P<target>.+) OK\n')
def get_temperature(ser: serial.Serial) -> Tuple[float, float]:
    ser.write(b'M105\n')
    res = ser.readline()
    guard_error(res, b'M105')
    res_s = res.decode()
    match = re.match(_TEMP_RE, res_s)
    return float(match.group('current')), float(match.group('target'))


def set_speed_pid(ser: serial.Serial,
                  kp: float, ki: float, kd: float):
    print(f'Overriding motor PID constants to kp={kp}, ki={ki}, kd={kd}')
    ser.write(f'M301 TM P{kp} I{ki} D{kd}\n'.encode())
    guard_error(ser.readline(), b'M301')


def set_heater_pid(ser: serial.Serial,
                   kp: float, ki: float, kd: float):
    print(f"Overriding heater PID constants to kp={kp}, ki={ki}, kd={kd}")
    ser.write(f'M301 TH P{kp} I{ki} D{kd}\n'.encode())
    guard_error(ser.readline(), b'M301')


def stable(data: List[Tuple[float, float]],
           target: float,
           criterion: float,
           window: datetime.timedelta = datetime.timedelta(seconds=2)):
    last_timestamp = data[-1][0]
    if last_timestamp - data[0][0] < window.total_seconds():
        return False
    tocheck = list(itertools.takewhile(lambda sample: sample[0] > (last_timestamp - window.total_seconds()), reversed(data)))
    dev = [(sample[1]-target)**2 for sample in tocheck]
    if any([d > criterion for d in dev]):
        return False
    return True


def set_temperature(ser: serial.Serial, target: float):
    ser.write(f'M104 S{target}\n'.encode())
    guard_error(ser.readline(), b'M104')


def _do_sample(ser: serial.Serial,
               config: RunConfig,
               sampler: Callable[[serial.Serial], Tuple[float, float]],
               done: Callable[[datetime.timedelta, List[Tuple[float, float]]], bool]) -> List[Tuple[float, float]]:
    data = []
    running_since = datetime.datetime.now()
    try:
        print("Beginning data sampling (ctrl-c ONCE stops early)")
        while True:
            current, target = sampler(ser)
            now = datetime.datetime.now()
            data.append(((now - running_since).total_seconds(), current))
            time.sleep(config.sampling_time.total_seconds())
            if done(now-running_since, data):
                break
    except KeyboardInterrupt:
        print("Sampling complete (keyboard interrupt)")
    except RuntimeError as re:
        print(f"Sampling complete (error from device: {re})")
    finally:
        return data


def sample_fixed_time(ser: serial.Serial,
                      config: RunConfig,
                      duration: datetime.timedelta,
                      sampler: Callable[[serial.Serial], Tuple[float, float]],
                      done: Callable[[datetime.timedelta, List[Tuple[float, float]]], bool] = None):
    def timeonly_done(elapsed: datetime.timedelta, _ignore: Any):
        if elapsed > duration:
            print(f"Sampling complete (fixed time over) after {elapsed.total_seconds()}")
        return elapsed > duration

    return _do_sample(ser, config, sampler, done or timeonly_done)


def sample_until_stable(ser: serial.Serial,
                        config: RunConfig,
                        min_duration: datetime.timedelta,
                        sampler: Callable[[serial.Serial], Tuple[float, float]]):
    def done(elapsed: datetime.timedelta, data: List[Tuple[float, float]]):
        if elapsed < min_duration:
            return False
        if abs(data[-1][1]-config.target) < 2 and stable(data, config.target, config.stability_criterion):
            print("Sampling complete (speed stable) after {elapsed.total_seconds()}")
            return True

    return _do_sample(ser, config, sampler, done)


def set_speed(ser: serial.Serial, speed: float):
    print(f"Setting speed to {speed}RPM")
    ser.write(f'M3 S{int(speed)}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M3')
    print(res)


def set_heater_power(ser: serial.Serial, power: float):
    print(f'Setting heater power to {power}/1.0')
    if power < 0 or power > 1.0:
        raise RuntimeError("bad power setting must be in [0, 1]")
    ser.write(f'M104.D S{power}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M104.D')
    print(res)


def set_acceleration(ser: serial.Serial, acceleration: float):
    print(f'Setting acceleration to {acceleration}RPM/s')
    ser.write(f'M204 S{acceleration}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M204')




def plot_data(data: List[Tuple[float, float]], config: RunConfig):
    fig = pp.figure()
    gs = gridspec.GridSpec(2, 1, height_ratios=[4, 1], hspace=0.5)
    axes = pp.subplot(gs[0])
    bwaxes = pp.subplot(gs[1])
    axes.plot([sample[0] for sample in data], [sample[1] for sample in data], label=f'{config.process} ({config.process_unit})')
    axes.axhline(config.target, color='red', linestyle='-.', label='target')
    axes.axhspan(config.target - config.stability_criterion**2, config.target + config.stability_criterion**2,
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
        if data[0][1] < config.target:
            return sample[1] > config.target
        else:
            return sample[1] < config.target
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

    filtered_samples = data[len(data)//2:-1]
    box_data = [filtered_samp[1] for filtered_samp in filtered_samples]
    bwaxes.boxplot(box_data, vert=False)
    bp = bwaxes.set_xlabel(config.process)
    bwaxes.set_title(f'{config.process} sample data after first target intercept')
    axes.indicate_inset([filtered_samples[0][0], min(box_data),
                         filtered_samples[-1][0]-filtered_samples[0][0],
                         max(box_data)-min(box_data)],
                        bwaxes)

    axes.set_ylabel(f'{config.process} ({config.process_unit})')
    axes.set_xlabel('time (sec)')
    axes.set_title(f'{config.process} ({config.process_unit})/time (sec) (target={config.target})')
    pp.show()

def write_json(data: List[Tuple[float, float]], config: RunConfig, destfile: io.StringIO):
    timestamp = datetime.datetime.now()
    output_file = destfile or open(f'./heater-shaker-{config.process}-{config.target}-{timestamp}.json', 'w')
    json.dump({'meta': {'name': f'heater-shaker {config.process} run',
                        'config': config.dict_for_save(),
                        'format': ['time', config.process], 'units': ['s', 'rpm'],
                        'takenAt': str(timestamp)},
                   'data': data}, output_file)

def write_csv(data: List[Tuple[float, float]], config: RunConfig, destfile: io.StringIO):
    timestamp = datetime.datetime.now()
    output_file = destfile or open(f'./heater-shaker-{config.process}-{config.target}-{timestamp}.csv', 'w')
    writer = csv.writer(output_file)
    writer.writerows(
        [['title', f'heater shaker {config.process}'],
         ['taken at', str(timestamp)]])
    writer.writerows(config.rowiterable_for_save())
    writer.writerows([['time', config.process],
                      ['seconds', config.process_unit]])
    writer.writerows(data)

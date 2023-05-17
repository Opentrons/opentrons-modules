import argparse
import re
from serial.tools.list_ports import grep
import serial
import datetime
import time
from matplotlib import pyplot as pp, gridspec
import csv
import io
import json
from typing import  Dict, Tuple, List, Callable, Any
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

def _do_sample(config: RunConfig,
               sampler: Callable[[serial.Serial], Tuple[float, float]],
               done: Callable[[datetime.timedelta, List[Tuple[float, float]]], bool]) -> List[Tuple[float, float]]:
    data = []
    running_since = datetime.datetime.now()
    try:
        print("Beginning data sampling (ctrl-c ONCE stops early)")
        while True:
            current, target = sampler()
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

def sample_fixed_time(config: RunConfig,
                      duration: datetime.timedelta,
                      sampler: Callable[[serial.Serial], Tuple[float, float]],
                      done: Callable[[datetime.timedelta, List[Tuple[float, float]]], bool] = None):
    def timeonly_done(elapsed: datetime.timedelta, _ignore: Any):
        if elapsed > duration:
            print(f"Sampling complete (fixed time over) after {elapsed.total_seconds()}")
        return elapsed > duration

    return _do_sample(config, sampler, done or timeonly_done)

def sample_until_stable(config: RunConfig,
                        min_duration: datetime.timedelta,
                        sampler: Callable[[serial.Serial], Tuple[float, float]]):
    def done(elapsed: datetime.timedelta, data: List[Tuple[float, float]]):
        if elapsed < min_duration:
            return False
        if abs(data[-1][1]-config.target) < 2 and stable(data, config.target, config.stability_criterion):
            print("Sampling complete (speed stable) after {elapsed.total_seconds()}")
            return True

    return _do_sample(config, sampler, done)

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

def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*empdeck*'))
        if not avail:
            raise RuntimeError("could not find tempdeck")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)


class Tempdeck():
    def __init__(self, port: str = None, debug: bool = False):
        '''
        Constructs a new Tempdeck. Leave `port` empty to connect to the
        first Tempdeck connected over USB.
        '''
        self._ser = build_serial(port)
        self._debug = debug

    def _send_and_recv(self, msg: str, guard_ret: str = None) -> str:
        '''Internal utility to send a command and receive the response'''
        self._ser.write(msg.encode())
        if self._debug:
            print(f'Sending: {msg}')
        ret = self._ser.readline()
        if guard_ret:
            if not ret.startswith(guard_ret.encode()):
                raise RuntimeError(f'Incorrect Response: {ret}')
        if ret.startswith('ERR'.encode()):
            raise RuntimeError(ret)
        if self._debug:
            print(f'Received: {ret.decode()}')
        return ret.decode()

    _TEMP_RE = re.compile('^M105.D PT1:(?P<plate1>.+) PT2:(?P<plate2>.+) HST:(?P<heatsink>.+) PA1:(.+) PA2:(.+) HSA:(.+) OK\n')
    
    def get_temperatures(self) -> Tuple[float, float]:
        '''
        Get the temperatures of the system. Returns a tuple
        of [plate1, plate2, heatsink]
        '''
        res = self._send_and_recv('M105.D\n', 'M105.D PT1:')
        match = re.match(self._TEMP_RE, res)
        plate1 = float(match.group('plate1'))
        plate2 = float(match.group('plate2'))
        heatsink = float(match.group('heatsink'))
        return plate1, plate2, heatsink
    
    def deactivate(self):
        self._send_and_recv('M18\n', 'M18 OK')
    
    def set_temperature(self, temp: float):
        msg = f'M104 S{temp}\n'
        self._send_and_recv(msg, 'M104 OK')

    def set_peltier_power(self, power: float = 0):
        '''
        Set the output PWM of the peltiers. Positive values heat and
        negative values cool. The range is [-1,1]
        '''
        send = f'M104.D S{power}\n'
        self._send_and_recv(send, 'M104.D OK')

    def set_fan_power(self, power: float = 0):
        '''
        Set the fan output PWM
        '''
        send = f'M106 S{power}\n'
        self._send_and_recv(send, 'M106 OK')
    
    def set_fan_automatic(self):
        '''
        Set the fan to automatic mode
        '''
        self._send_and_recv('M107\n', 'M107 OK')

    def set_thermal_pid(self, kp: float, ki: float, kd: float):
        '''
        Set the peltier PID constants
        '''
        send = f'M301 P{kp} I{ki} D{kd}\n'
        self._send_and_recv(send, 'M301 OK')

    def set_offsets(self, a: float = None, b: float = None, c: float = None):
        '''
        Set the offset constants. If any constants are not provided (left as
        NONE), they will not be written.
        '''
        msg = 'M116'
        if a is not None:
            msg += f' A{a}'
        if b is not None:
            msg += f' B{b}'
        if c is not None:
            msg += f' C{c}'
        msg += '\n'
        self._send_and_recv(msg, 'M116 OK')
    
    _OFFSETS_RE = re.compile('^M117 A:(?P<const_a>.+) B:(?P<const_b>.+) C:(?P<const_c>.+) OK\n')

    def get_offsets(self) -> Tuple[float, float, float]:
        '''
        Get the thermal offset constants.

        Returns:
            tuple of [a, b, c] constants
        '''
        res = self._send_and_recv('M117\n', 'M117 A:')
        match = re.match(self._OFFSETS_RE, res)
        a = float(match.group('const_a'))
        b = float(match.group('const_b'))
        c = float(match.group('const_c'))
        return a, b, c

    _POWER_RE = re.compile('^M103.D I:(?P<current>.+) R:(?P<rpm>.+) P:(?P<p_pwm>.+) F:(?P<f_pwm>.+) OK\n')
    def get_thermal_power(self) -> Tuple[float, float, float, float]:
        """
        Get the thermal power of the system.
        
        Returns:
            peltier current in mA
            fan RPM
            peltier PWM [-1,1]
            fan PWM [0,1]
        """
        res = self._send_and_recv('M103.D\n', 'M103.D I:')
        match = re.match(self._POWER_RE, res)
        current = float(match.group('current'))
        rpm = float(match.group('rpm'))
        peltier_pwm = float(match.group('p_pwm'))
        fan_pwm = float(match.group('f_pwm'))
        return current, rpm, peltier_pwm, fan_pwm

    def dfu(self):
        '''
        Enter the bootloader. This will cause future comms to fail.
        '''
        self._ser.write(b'dfu\n')

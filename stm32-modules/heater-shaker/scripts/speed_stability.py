#!/usr/bin/env python3
import argparse
from copy import copy

from dataclasses import dataclass, asdict

import datetime
import time

from typing import Any, Dict, Tuple, List, Optional

import serial

import test_utils


DEFAULT_PID_CONSTANTS = {'kp': 2372, 'ki': 2624, 'kd': 0}

def build_parser() -> argparse.ArgumentParser:
    p = test_utils.build_parent_parser('Run some motor tests on a heater-shaker')
    p.add_argument('speed', metavar='SPEED', type=float, help='Target speed to regulate at')
    p.add_argument('-w', '--warm-up', type=float, dest='warmup_speed',
                    help='Speed to hold before going to the speed argument (unplotted)',
                    default=None)
    p.add_argument('-t', '--time', type=float, help='Time to sample after setting the target speed. If not specified, tries to await stability',
                   default=None)
    p.add_argument('-a', '--acceleration', help='acceleration  (rpm/s)', type=int, default=1000)
    p.add_argument('--stability-criterion', type=float, default=2, help='square dev from target threshold for stability (rpm^2)')
    p.add_argument('--stability-window', type=float, default=1, help='trailing window duration for stability check (s)')
    p.add_argument('-kp', dest='kp_override', type=float, help='Kp override')
    p.add_argument('-ki', dest='ki_override', type=float, help='Ki override')
    p.add_argument('-kd', dest='kd_override', type=float, help='Kd override')
    return p


@dataclass
class SpeedConfig(test_utils.RunConfig):
    warmup_speed: Optional[float]
    fixed_time_sample: Optional[datetime.timedelta]
    acceleration: float
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
        return cls(
            process='speed',
            process_unit='rpm',
            target=args.speed,
            warmup_speed=args.warmup_speed,
            fixed_time_sample=args.time and datetime.timedelta(seconds=args.time),
            sampling_time=datetime.timedelta(seconds=args.sampling_time),
            acceleration=args.acceleration,
            stability_criterion=args.stability_criterion,
            stability_window=datetime.timedelta(seconds=args.stability_window),
            pid_constants=pid_constants,
            pid_constants_need_setting=needs_setting)

    def dict_for_save(self) -> Dict[str, Any]:
        parent = super().dict_for_save()
        mydict = asdict(self)
        for k, v in mydict.items():
            if isinstance(v, datetime.timedelta):
                mydict[k] = v.total_seconds()
        mydict.pop('pid_constants_need_setting')
        parent.update(mydict)
        return parent

    def rowiterable_for_save(self) -> List[List[Any]]:
        return super().rowiterable_for_save() + [
            ['warmup speed (rpm)',  self.warmup_speed or 0],
            ['fixed sample time  (s)',
             (self.fixed_time_sample and self.fixed_time_sample.total_seconds())
             or 'not set'],
            ['acceleration (rpm/s)', self.acceleration],
            ['stability window (s)',  self.stability_window.total_seconds()],
            ['kp', self.pid_constants['kp']],
            ['ki', self.pid_constants['ki']],
            ['kd', self.pid_constants['kd']]
        ]


def sample_speed(ser, config: SpeedConfig, min_time: datetime.timedelta = None):
    min_time = min_time or datetime.timedelta(seconds=2)
    if config.fixed_time_sample:
        return test_utils.sample_fixed_time(ser, config, config.fixed_time_sample, test_utils.get_speed)
    else:
        return test_utils.sample_until_stable(ser, config, min_time, test_utils.get_speed)

def do_warmup(ser: serial.Serial, speed: float):
    print(f"Doing warmup phase at {speed}RPM")
    then = datetime.datetime.now()
    test_utils.set_speed(ser, speed)
    warmup_config = test_utils.RunConfig(
        process='speed',
        process_unit='rpm',
        target=speed,
        stability_criterion=200,
        sampling_time=datetime.timedelta(milliseconds=500))
    sample_speed(ser, warmup_config)
    now = datetime.datetime.now()
    print(f"Warmup phase complete after {(now-then).total_seconds()}")


def do_run(ser: serial.Serial, config: SpeedConfig):
    print(f"Doing main sample run")
    then = datetime.datetime.now()
    test_utils.set_speed(ser, config.target)
    data = sample_speed(ser, config)
    test_utils.set_speed(ser, 0)
    now = datetime.datetime.now()
    print(f"Data acquisition complete after {(now-then).total_seconds()}")
    return data

if __name__ == '__main__':
    parser = build_parser()
    args = parser.parse_args()
    if args.output and 'json' in args.output and 'csv' in args.output:
        raise RuntimeError("please pick just one of json or csv")
    port = test_utils.build_serial(args.port)
    config = SpeedConfig.from_args(args)
    test_utils.set_acceleration(port, config.acceleration)
    if config.pid_constants_need_setting:
        test_utils.set_speed_pid(port, **config.pid_constants)
    if config.warmup_speed:
        do_warmup(port, config.warmup_speed)
    data = do_run(port, config)
    port.close()
    output_type = args.output or ['plot']
    if 'json' in output_type:
        test_utils.write_json(data, config, args.output_file)
    if 'csv' in output_type:
        test_utils.write_csv(data, config, args.output_file)
    if 'plot' in output_type:
        test_utils.plot_data(data, config)

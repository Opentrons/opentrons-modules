#!/usr/bin/env python3
from dataclasses import dataclass, asdict
import datetime
import serial
import argparse
from copy import copy

from typing import Any, Dict, List, Tuple

import test_utils

DEFAULT_PID_CONSTANTS = {'kp': 0.38, 'ki': 0.0275, 'kd': 0}

def build_parser():
    p = test_utils.build_parent_parser(desc='Run a thermal profile (step) on a temp deck gen 3')
    p.add_argument('temperature', metavar='TEMPERATURe', type=float, help='Target temperature')
    p.add_argument('-t', '--time', type=float, help='Fixed time to sample for')
    p.add_argument('--stability-criterion', type=float, default=2, help='square dev from target threshold for stability (rpm^2)')
    p.add_argument('--stability-window', type=float, default=1, help='trailing window duration for stability check (s)')
    p.add_argument('-kp', dest='kp_override', type=float, help='Kp override')
    p.add_argument('-ki', dest='ki_override', type=float, help='Ki override')
    p.add_argument('-kd', dest='kd_override', type=float, help='Kd override')
    return p

@dataclass
class ThermalConfig(test_utils.RunConfig):
    fixed_time_sample: datetime.timedelta
    stability_window: datetime.timedelta
    pid_constants: Dict[str, float]
    pid_constants_need_setting: bool

    @classmethod
    def from_args(cls, args: argparse.Namespace):
        pid_constants = copy(DEFAULT_PID_CONSTANTS)
        needs_setting = False
        if args.kp_override is not None:
            pid_constants['kp'] = args.kp_override
            needs_setting = True
        if args.ki_override is not None:
            pid_constants['ki'] = args.ki_override
            needs_setting = True
        if args.kd_override is not None:
            pid_constants['kd'] = args.kd_override
            needs_setting = True
        return cls(
            process='temperature',
            process_unit='C',
            target=args.temperature,
            fixed_time_sample=args.time and datetime.timedelta(seconds=args.time),
            sampling_time=datetime.timedelta(seconds=args.sampling_time),
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
            ['power', self.power],
            ['fixed sample time (s)',
             (self.fixed_time_sample
              and self.fixed_time_sample.total_seconds()
              or 'not set')],
            ['stability window (s)', self.stability_window.total_seconds()],
            ['kp', self.pid_constants['kp']],
            ['ki', self.pid_constants['ki']],
            ['kd', self.pid_constants['kd']],
            ['duration', self.duration.total_seconds()]
        ]

def sample_heater(config: ThermalConfig,
                  min_time: datetime.timedelta = None) -> List[Tuple[float, float]]:
    min_time = min_time or datetime.timedelta(seconds=2)
    if config.fixed_time_sample:
        return test_utils.sample_fixed_time(
            config, config.fixed_time_sample, tempdeck.get_temperatures)
    else:
        return test_utils.sample_until_stable(
            config, min_time, tempdeck.get_temperatures)

if __name__ == '__main__':
    parser = build_parser()
    args = parser.parse_args()
    if args.output and 'json' in args.output and 'csv' in args.output:
        raise RuntimeError("please pick just one of json or csv")
    tempdeck = test_utils.Tempdeck(port = args.port)
    config = ThermalConfig.from_args(args)
    if config.pid_constants_need_setting:
        tempdeck.set_thermal_pid(**config.pid_constants)
    tempdeck.set_temperature(config.target)
    data = sample_heater(config)
    try:
        tempdeck.set_temperature(0)
    except RuntimeError:
        pass
    #port.close()
    output_type = args.output or ['plot']
    if 'json' in output_type:
        test_utils.write_json(data, config, args.output_file)
    if 'csv' in output_type:
        test_utils.write_csv(data, config, args.output_file)
    if 'plot' in output_type:
        test_utils.plot_data(data, config)

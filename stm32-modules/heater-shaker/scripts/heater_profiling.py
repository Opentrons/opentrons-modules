#!/usr/bin/env python3
from dataclasses import dataclass, asdict
import datetime
import serial
import argparse
import time

from typing import Any, Dict, List, Tuple

import test_utils

def build_parser():
    p = test_utils.build_parent_parser(desc='Run a heater profile (step) on a heater-shaker')
    p.add_argument('power', metavar='POWER', type=float, help='Heater power in [0, 1]')
    p.add_argument('duration', metavar='DURATION', type=float, help='Time to sample for')
    p.add_argument('-e', '--until-exceeds', dest='until_exceeds', type=float, help='Stop sampling early if temp exceeds this valuee', default=95)
    return p

@dataclass
class PowerConfig(test_utils.RunConfig):
    power: float
    duration: datetime.timedelta

    def dict_for_save(self) -> Dict[str, Any]:
        parent = super().dict_for_save()
        parent.update(asdict(self))
        return parent

    def rowiterable_for_save(self) -> List[List[Any]]:
        return super().rowiterable_for_save() + [
            ['power', self.power],
            ['duration', self.duration.total_seconds()]
        ]


if __name__ == '__main__':
    parser = build_parser()
    args = parser.parse_args()
    if args.output and 'json' in args.output and 'csv' in args.output:
        raise RuntimeError("please pick just one of json or csv")
    port = test_utils.build_serial(args.port)
    config = PowerConfig(power=args.power,
                         duration=datetime.timedelta(seconds=args.duration),
                         target=args.until_exceeds,
                         stability_criterion=2,
                         sampling_time=datetime.timedelta(seconds=args.sampling_time),
                         process='temperature',
                         process_unit='C')
    test_utils.set_heater_power(port, config.power)
    def done(duration, data):
        if duration > config.duration:
            print("sampling done (time expired)")
            return True
        if data[-1][1] > config.target:
            print("sampling done (target reaeched)")
            return True
    start = datetime.datetime.now()
    prefix = []
    for i in range(10):
        current, _ = test_utils.get_temperature(port)
        now = datetime.datetime.now()
        prefix.append(((now-start).total_seconds(), current))
        time.sleep(config.sampling_time.total_seconds())
    prefix_end = datetime.datetime.now()
    data = test_utils.sample_fixed_time(port, config, config.duration,
                                        test_utils.get_temperature, done)
    try:
        test_utils.set_heater_power(port, 0)
    except RuntimeError:
        pass
    port.close()

    prefix_duration = (prefix_end-start).total_seconds()
    padded = prefix + [(elem[0] + prefix_duration, elem[1]) for elem in data]
    output_type = args.output or ['plot']
    if 'json' in output_type:
        test_utils.write_json(padded, config, args.output_file)
    if 'csv' in output_type:
        test_utils.write_csv(padded, config, args.output_file)
    if 'plot' in output_type:
        test_utils.plot_data(padded, config)

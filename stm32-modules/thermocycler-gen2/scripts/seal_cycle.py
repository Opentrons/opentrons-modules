#!/usr/bin/env python3

'''Script to test cycling the seal motor'''

import time
from test_utils import *

def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Test the seal motor')
    parser.add_argument('-e', '--extend', type=int, required=False, default=1750000,
                        help='Number of steps to extend the motor')
    parser.add_argument('-r', '--retract', type=int, required=False, default=1900000,
                        help='Number of steps to retract the motor')
    parser.add_argument('-v', '--velocity', type=int, required=False, default=80000,
                        help='Target velocity in pulses/second')
    parser.add_argument('-a', '--accel', type=int, required=False, default=50000,
                        help='Target acceleration in pulses/second^2')
    parser.add_argument('-c', '--current', type=int, required=False, default=28,
                        help='Drive current in milliamps')
    parser.add_argument('-t', '--threshold', type=int, required=False, default=4,
                        help='Stallguard threshold value as an integer')
    parser.add_argument('-s', '--stallvelocity', type=int, required=False, default=70000,
                        help='Stallguard minimum velocity in pulses/second')
    return parser

_SEAL_STEPS_RE = re.compile('^M241.D S:(?P<steps>.+) OK\n')
def move_and_get_sg(steps, ser, delay = 0.1):
    ser.write(f'M241.D {steps}\n'.encode())
    while True:
        ser.write('M242.D\n'.encode())
        res = ser.readline()
        if res.startswith(b'M241'):
            # We are done
            res_s = res.decode()
            match = re.match(_SEAL_STEPS_RE, res_s)
            moved = int(match.group('steps'))
            ser.readline()
            return moved
        elif res.startswith(b'M242'):
            print(res)
            time.sleep(delay)
        else:
            raise RuntimeError(res)
            return 0
    

if __name__ == '__main__':
    parser = make_parser()
    args = parser.parse_args()

    ser = build_serial()

    # Set all the configured parameters
    set_seal_param(SealParam.VELOCITY, args.velocity, ser)
    set_seal_param(SealParam.ACCELERATION, args.accel, ser)
    set_seal_param(SealParam.RUN_CURRENT, args.current, ser)
    set_seal_param(SealParam.STALLGUARD_MIN_VELOCITY, args.stallvelocity, ser)

    steps_to_extend = abs(args.extend) * -1
    steps_to_retract = abs(args.retract)

    set_seal_param(SealParam.STALLGUARD_THRESHOLD, args.velocity, ser)
    steps_extended = move_seal_steps(steps_to_extend, ser)
    get_seal_status(ser)
    print(f'Moved:  {steps_extended} steps')
    time.sleep(10)

    set_seal_param(SealParam.VELOCITY, args.velocity, ser)
    set_seal_param(SealParam.ACCELERATION, args.accel, ser)
    set_seal_param(SealParam.RUN_CURRENT, args.current, ser)
    set_seal_param(SealParam.STALLGUARD_MIN_VELOCITY, args.stallvelocity, ser)
    set_seal_param(SealParam.STALLGUARD_THRESHOLD, args.threshold, ser)
    steps_retracted = move_and_get_sg(steps_to_retract, ser)
    get_seal_status(ser)
    print(f'Retracted: {steps_retracted} steps')

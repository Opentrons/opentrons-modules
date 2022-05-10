#!/usr/bin/env python3

import test_utils
import plot_temp
import serial
import time
import sys
import argparse

def cycle(ser : serial.Serial):
    # Cycle to 95 then 5 and finally back
    # Format is target temp, then hold time
    targets_pcr = [
        (95.0, 40.0),
        (5.0, 50.0),
        (20.0, 20.0) ]
    targets = [
        (35.0, 10.0),
        (20.0, 40.0),
        (65.0, 5.0),
        (80.0, 5.0),
        (95.0, 5.0),
        (50.0, 5.0),
        (4.0, 5.0),
        (20.0, 5.0),
        (10.0, 5.0),
        (1.0, 5.0) ]
    target_idx = 0

    hold_timer = 0.0
    hold_threshold = 0.5

    def update_callback(lid, heatsink, right, left, center):
        nonlocal target_idx
        nonlocal hold_timer

        if target_idx < len(targets):
            if hold_timer == 0.0:
                temp = (right + left + center) / 3.0
                error = abs(temp - targets[target_idx][0])
                if(error < hold_threshold):
                    hold_timer = time.time()
                    print('Reached target. Holding')
            else:
                to_hold = targets[target_idx][1]
                if time.time() - hold_timer >= to_hold:
                    hold_timer = 0.0
                    target_idx = target_idx + 1
                    if target_idx == len(targets):
                        print('Done cycling')
                        test_utils.deactivate_plate(ser)
                        test_utils.deactivate_lid(ser)
                    else:
                        new_temp = targets[target_idx][0]
                        new_hold = targets[target_idx][1]
                        print(f'Moving to {new_temp} for {new_hold} seconds')
                        test_utils.set_plate_temperature(new_temp, ser, hold_time=new_hold)
        sys.stdout.flush()
        return
    
    print(f'Moving to {targets[0][0]} for {targets[0][1]} seconds')
    test_utils.set_plate_temperature(targets[0][0], ser, targets[0][1])
    plot_temp.graphTemperatures(ser, update_callback)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Run a simple PCR cycle")
    parser.add_argument('-s', '--socket', type=int, required=False, 
                        help='Open a socket on localhost with a given port')
    parser.add_argument('-c', '--constants', type=float, required=False, nargs=3, 
                        metavar=('P','I','D'), 
                        help='define P, I, and D constants for control')
    args = parser.parse_args()
    if args.socket:
        print(f"Opening socket at localhost:{args.socket}")
        ser = test_utils.build_tcp(args.socket)
    else:
        print("Connecting to TC via serial")
        ser = test_utils.build_serial()

    if args.constants:
        print(f'Setting P={args.constants[0]}, I={args.constants[1]}, D={args.constants[2]}')
        test_utils.set_peltier_pid(args.constants[0], args.constants[1], args.constants[2], ser)

    test_utils.set_lid_temperature(105, ser)
    cycle(ser)
    test_utils.deactivate_lid(ser)
    test_utils.deactivate_plate(ser)

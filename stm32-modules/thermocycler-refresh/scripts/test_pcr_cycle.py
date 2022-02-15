#!/usr/bin/env python3

import test_utils
import plot_temp
import serial
import time
import sys
from typing import Tuple

def cycle(ser : serial.Serial):
    # Cycle to 95 then 5 and finally back
    # Format is target temp, then hold time
    targets = [
        (95.0, 20.0),
        (5.0, 10.0),
        (20.0, 20.0) ]
    targets_fake = [
        (20.0, 20.0) ]
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
                        print(f'Moving to {new_temp}')
                        test_utils.set_plate_temperature(new_temp, ser)
        sys.stdout.flush()
        return
    
    print(f'Moving to {targets[0][0]}')
    test_utils.set_plate_temperature(targets[0][0], ser)
    plot_temp.graphTemperatures(ser, update_callback)

if __name__ == '__main__':
    ser = test_utils.build_serial()

    #test_utils.set_peltier_pid(0.043429, 0.001656, 0.0164408, ser)
    test_utils.set_peltier_pid(0.26225539341692944, 0.05356372043227761, 0.008128697818415609, ser)
    test_utils.set_heater_pid(0.0922, 0.001552, 0.10358, ser)

    #test_utils.set_lid_temperature(90, ser)
    cycle(ser)
    test_utils.deactivate_lid(ser)
    test_utils.deactivate_plate(ser)

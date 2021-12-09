#!/usr/bin/env python3

import test_utils
import plot_temp

HEAT_TARGET = 50
COOL_TARGET = 10

if __name__ == '__main__':
    ser = test_utils.build_serial()
    direction = int(1)

    def update_cb(lid, heatsink, right, left, center):
        global direction
        global ser
        if(direction > 0):
            if(center > HEAT_TARGET):
                test_utils.set_plate_temperature(temperature=COOL_TARGET, ser=ser)
                direction = -1
        else:
            if(center < COOL_TARGET):
                test_utils.set_plate_temperature(temperature=HEAT_TARGET, ser=ser)
                direction = 1
    

    test_utils.set_peltier_pid(0.97, 0.102, 1.901, ser)
    test_utils.set_fans_manual(0.35, ser)
    test_utils.set_plate_temperature(temperature = HEAT_TARGET, ser=ser)
    plot_temp.graphTemperatures(ser, update_cb)

    test_utils.deactivate_plate(ser)
    test_utils.set_fans_manual(0, ser)

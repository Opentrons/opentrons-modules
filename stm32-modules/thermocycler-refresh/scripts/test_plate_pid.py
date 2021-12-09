#!/usr/bin/env python3

import test_utils
import plot_temp

HEAT_TARGET = 50
COOL_TARGET = 10

if __name__ == '__main__':
    ser = test_utils.build_serial()

    def update_heating(lid, heatsink, right, left, center):
        if(center > HEAT_TARGET):
            test_utils.deactivate_plate(ser)
            plot_temp.closeGraph()
            print('Turned off plate')
        return
    
    def update_cooling(lid, heatsink, right, left, center):
        if(center < COOL_TARGET):
            test_utils.deactivate_plate(ser)
            plot_temp.closeGraph()
            print('Turned off plate')
        return
    
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

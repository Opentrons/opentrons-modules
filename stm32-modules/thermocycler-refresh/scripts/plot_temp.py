#!/usr/bin/env python3

from datetime import datetime
from datetime import timedelta
from matplotlib import pyplot as pp
from matplotlib.animation import FuncAnimation
import test_utils
import serial

ser = serial.Serial

# Callback should accept the current temperature in this order:
#   lid, heatsink, right, left, center
def graphTemperatures(ser_in : serial.Serial = None, _callback = None):
    SAMPLE_LIMIT = 500
    SAMPLE_INTERVAL = 100
    x_time, y_lid , y_left, y_center, y_right, y_sink= [], [], [], [], [], []
    if not ser_in:
        print('Building new serial')
        ser = test_utils.build_serial()
    else:
        ser = ser_in

    fig = pp.figure()
    lid_line, = pp.plot_date(x_time, y_lid, '-b', label='Lid (C)')
    #plate_line, = pp.plot_date(x_time, y_plate, '-r', label='Plate (C)')
    left_line, = pp.plot_date(x_time, y_left, '-r', label='Left (C)')
    right_line, = pp.plot_date(x_time, y_right, '-c', label='Right (C)')
    center_line, = pp.plot_date(x_time, y_center, '-y', label='Center (C)')
    sink_line, = pp.plot_date(x_time, y_sink, '-g', label='Heatsink (C)')
    fig.legend()

    # Sub function for the updating
    def update(frame):
        x_time.append(datetime.now())
        lid = test_utils.get_lid_temperature(ser)
        y_lid.append(lid)
        hs, right, left, center = test_utils.get_plate_temperatures(ser)
        y_sink.append(hs)
        #y_plate.append(statistics.mean([right, left, center]))
        y_left.append(left)
        y_right.append(right)
        y_center.append(center)

        if _callback:
            _callback(lid, hs, right, left, center)

        if(len(x_time) > SAMPLE_LIMIT):
            x_time.pop(0)
            y_lid.pop(0)
            y_sink.pop(0)
            #y_plate.pop(0)
            y_left.pop(0)
            y_right.pop(0)
            y_center.pop(0)
            
        lid_line.set_data(x_time, y_lid)
        #plate_line.set_data(x_time, y_plate)
        left_line.set_data(x_time, y_left)
        right_line.set_data(x_time, y_right)
        center_line.set_data(x_time, y_center)
        sink_line.set_data(x_time, y_sink)
        if(len(x_time) == SAMPLE_LIMIT):
            fig.gca().set_xlim(left=x_time[0], right = x_time[-1])
    
    max_x = datetime.now() + timedelta(milliseconds=SAMPLE_INTERVAL * SAMPLE_LIMIT)
    fig.gca().set_xlim(left=datetime.now(), right = max_x)
    fig.gca().set_ylim(bottom=0, top=100)
    animation = FuncAnimation(fig, update, interval=SAMPLE_INTERVAL)
    pp.show()

if __name__ == '__main__':
    graphTemperatures()
    exit(1)

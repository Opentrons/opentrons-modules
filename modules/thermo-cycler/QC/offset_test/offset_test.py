"""
This test script is for internal use only, this uses two Eutechnics 4500 Thermometers to read
well temperatures. The thermometers are placed in the hottest and coldest locations on the well plate.
This determines the delta temperature offsets of the well.
"""

import sys, os, time
import csv
import threading
import subprocess
from datetime import datetime
sys.path.insert(0, os.path.abspath(''))
# Thermocycler mini driver
import TC_mini_driver
import optparse

#---------------------------------------#
TOTAL_RUNS = 1
GCODES = True
TC_GCODE_ROUNDING_PRECISION = 2
COVER_TEMP = 105
PLATE_TEMP_PRE = 105
end_time = 4
PLATE_TEMP_HOLD_1 = (70, 60*end_time)
# Following temp durations have been increased by 10 sec to accommodate for ramp time
COOL_PLATE_TEMP = [(30 , 20)]
fan_control = 0.2 #can only be from 0-1
fan_control_cool = 0.5
#---------------------------------------#
SERIAL_ACK = '\r\n'

GCODES = {
    'OPEN_LID': 'M126',
    'CLOSE_LID': 'M127',
    'GET_LID_STATUS': 'M119',
    'SET_LID_TEMP': 'M140',
    'GET_LID_TEMP': 'M141',
    'DEACTIVATE_LID_HEATING': 'M108',
    'EDIT_PID_PARAMS': 'M301',
    'SET_PLATE_TEMP': 'M104',
    'GET_PLATE_TEMP': 'M105',
    'SET_RAMP_RATE': 'M566',
    'DEACTIVATE': 'M18',
    'DEVICE_INFO': 'M115',
    'FAN_CONTROL': 'M106'
}
#---------------------------------------#

PROTOCOL_STEPS = [
    '{} S{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_PRE, SERIAL_ACK),	# 0
    '{} S{}{}'.format(GCODES['SET_LID_TEMP'], COVER_TEMP, SERIAL_ACK),		# 1
    '{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], PLATE_TEMP_HOLD_1[0], PLATE_TEMP_HOLD_1[1], SERIAL_ACK),		# 2
    '{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], COOL_PLATE_TEMP[0][0], COOL_PLATE_TEMP[0][1], SERIAL_ACK),# 3
    '{}{}'.format(GCODES['DEACTIVATE_LID_HEATING'], SERIAL_ACK)
    ]
#---------------------------------------#
#These parameters are for cycle protocol
TOP_LID_TEMP = 105
HI_TEMP_PLATE = 95
LO_TEMP_PLATE = 70
HI_TEMP_TIME = 32
LO_TEMP_TIME = 32
ramp_time = 12
PAUSE_TIME = ramp_time+ 40 #32 is equal HI_TEMP_TIME/LO_TEMP_TIME
PRE_HOLD_TEMP = 40 + ramp_time
#---------------------------------------#
CYCLE_PROTOCOL_STEPS = [
    '{} S{}{}'.format(GCODES['SET_LID_TEMP'], TOP_LID_TEMP, SERIAL_ACK),	#0 # SET TOP LID to 105C
    '{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], HI_TEMP_PLATE, PRE_HOLD_TEMP, SERIAL_ACK), # 1 # HEAT PLATE for 0.5min
    '{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], LO_TEMP_PLATE, LO_TEMP_TIME, SERIAL_ACK), # 2 # COOL PHASE
    '{} S{} H{}{}'.format(GCODES['SET_PLATE_TEMP'], HI_TEMP_PLATE, HI_TEMP_TIME, SERIAL_ACK), # 3 # HOT TEMP PHASE
    '{}{}'.format(GCODES['DEACTIVATE_LID_HEATING'], SERIAL_ACK)
    ]
#---------------------------------------#
#M11 cont\r\n
GCODE_DEBUG_PRINT_MODE = 'M111 cont{}'.format(SERIAL_ACK)
#stat\r\n
GET_STAT = 'stat{}'.format(SERIAL_ACK)
#M18\r\n
DEACTIVATE = '{}{}'.format(GCODES['DEACTIVATE'], SERIAL_ACK)
#---------------------------------------#
#conver time into milli seconds
def time_to_seconds():
   dt = datetime.now() - start_time
   ms = (dt.days * 24 * 60 * 60 + dt.seconds) * 1000 + dt.microseconds / 1000.0
   return ms/1000

#Fan duty cycle function
def fan_power(duty_cycle):
    FAN_CONTROL = '{} S{}{}'.format(GCODES['FAN_CONTROL'], duty_cycle, SERIAL_ACK)
    return FAN_CONTROL

def protocol(ser):
    time.sleep(0.5)
    print('Fan control @', fan_control)
    ser._send(FAN_CONTROL)
    print("Executing ",PROTOCOL_STEPS[2])
    ser._send(PROTOCOL_STEPS[1])
    ser._send(PROTOCOL_STEPS[2]) # Hold temp
    print("Holding temperature for time: ", PLATE_TEMP_HOLD_1)
    time.sleep(PLATE_TEMP_HOLD_1[1])
    print("RUN COMPLETED")
    #time.sleep(40)
    print("Cooling plate")
    ser._send(FAN_CONTROL_COOL)
    ser._send(PROTOCOL_STEPS[3])
    time.sleep(40)
    ser._send(DEACTIVATE)


def cycle_protocol(ser):
    ending_time = 3 #3mins
    time.sleep(2)
    ser._send(fan_power(0.2)) # Manual mode for fan control
    print("Executing ")
    #initial pre heat for 3mins
    ser._send(CYCLE_PROTOCOL_STEPS[0]) # set lid temp
    ser._send(CYCLE_PROTOCOL_STEPS[1]) # HOLD TEMP
    #time.sleep(60*end_time + ramp_time)#Hold Temp 3 mins
    time.sleep(40 + ramp_time)#Hold Temp 3 mins
    for cycle in range(10):
        ser._send(CYCLE_PROTOCOL_STEPS[2])
        print("RAMP DOWN")
        time.sleep(PAUSE_TIME)
        ser._send(CYCLE_PROTOCOL_STEPS[3])
        print("RAMP UP")
        time.sleep(PAUSE_TIME)
    ser._send(DEACTIVATE)

def record_status(filename, ser):
    t_end = time.time() + 60 * 18
    t_end_2 = time.time() + 40
    threshold = 0.3
    STABLE_TEMP = 3
    STABLE_T2 = 20
    count = 1
    previous_reading = 0
    ret_temp = 0
    #run for some fixed amount time
    while time.time() < t_end:
        temps_list = ser.read_temp()
        #print(temps_list)
        #skip first reading
        if count > 1:
            previous_reading = ret_temp
            print("previous reading: ", previous_reading)
        #Fan logic after first cycle
        if time.time() > t_end_2:
            #Only for rampdown
            if previous_reading-temps_list['T1'] > 0.5:
                print("Delta 1",previous_reading-temps_list['T1'])
                ser._send(fan_power(0.4))
            elif previous_reading-temps_list['T1'] < 0.5:
                print("Delta 2",previous_reading-temps_list['T1'])
                ser._send(fan_power(0))
        count+=1 # increment count
        #create history temp variable
        ret_temp = temps_list['T1']
        #Record following data from thermocycler
        test_data['T1'] = temps_list['T1']
        test_data['T2'] = temps_list['T2']
        test_data['T3'] = temps_list['T3']
        test_data['T4'] = temps_list['T4']
        test_data['T5'] = temps_list['T5']
        test_data['T6'] = temps_list['T6']
        test_data['T.heatsink'] = temps_list['T.sink']
        test_data['T.lid'] = temps_list['T.Lid']
        test_data['time'] = time_to_seconds()
        log_file.writerow(test_data)
        print(test_data)
        data_file.flush()

if __name__== '__main__':
    parser = optparse.OptionParser(usage='usage: %prog [options] ')
    parser.add_option("-t", "--temperature", dest = "temperature",type = "str", default = "95C_to_70C", help = "Target Temperature")
    parser.add_option("-p", "--pause_time", dest = "pause_time", type = "str", default = "40", help = "pause_time ")
    (options, args) = parser.parse_args(args = None, values = None)
    #defined thermo-cycler
    tc = TC_mini_driver.thermocycler(port = 'COM31', timeout=0.5)
    tc._send(GCODE_DEBUG_PRINT_MODE)
    tc._send(PROTOCOL_STEPS[1])
    temperature = 0 # Create temp variable
    #heat the TOP LID to 105
    while temperature < 100:
        temp_dict = tc.read_temp()
        temperature = temp_dict['T.Lid']
        tc._send(fan_power(0.6)) #Fan duty cycle 60 %
        print("top lid temperature: ", temp_dict['T.Lid'])
        print("Waiting on temp lid to reach {}".format(PROTOCOL_STEPS[1]))

    file_name = "results/Temp_offset_%s_%sC.csv"%(datetime.now().strftime("%m-%d-%y_%H-%M"), options.temperature)
    print(file_name)
    #open subprocess for thermometers
    subprocess.Popen(['python',
                    'H1_thermometer.py','-t {0}'.format(options.temperature)],
                    creationflags=subprocess.CREATE_NEW_CONSOLE )
    subprocess.Popen(['python', 'C5_thermometer.py', '-t {0}'.format(options.temperature)],
                    creationflags=subprocess.CREATE_NEW_CONSOLE )
    #pause till therometers are readings data
    time.sleep(6) #Manual time pause to sync subprocess with thermocycler thread
    with open('{}.csv'.format(file_name), 'w', newline='') as data_file:
        test_data={'T1':None,'T2':None,'T3':None,'T4':None,'T5':None, 'T6':None,'T.lid':None,'T.heatsink':None, 'time': None}
        log_file = csv.DictWriter(data_file, test_data)
        log_file.writeheader()
        #threads
        print("Starting protocol and reading")
        recorder = threading.Thread(target=record_status, args=(file_name, tc, ))
        protocol_writer = threading.Thread(target=cycle_protocol, args=(tc,), daemon=True)
        process_list = [recorder, protocol_writer]
        start_time = datetime.now()
        print("start_time: ", start_time)
        for prc in process_list:
            prc.start()
        for proc in process_list:
            proc.join()

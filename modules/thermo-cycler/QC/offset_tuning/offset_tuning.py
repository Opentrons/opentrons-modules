import serial
import time
import threading
import logging
import serial_comm
import numpy
from pathlib import PurePath
from argparse import ArgumentParser


THIS_DIR = PurePath(__file__).parent
OPENTRONS_VID = 0x04d8
EUTECH_VID = 0x0403
TC_BAUDRATE = 115200
EUTECH_BAUDRATE = 9600

EUTECH_TEMPERATURE = None
TC_STATUS = {}
TUNING_OVER = False
TC_SERIAL = None
default_c = 0.15
all_c_offsets = []


log = logging.getLogger()

PRINTY_STAT_KEYS = ['Plt_Target', 'Plt_current', 'Cov_Target', 'T.Lid',
                    'T.sink', 'T.offset', 'Fan', 'millis']
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
    'SET_OFFSET_CONSTANTS': 'M116',
    'GET_OFFSET_CONSTANTS': 'M117',
    'DEBUG_MODE': 'M111',
    'PRINT_DEBUG_STAT': 'stat',
    'CONTINUOUS_DEBUG_STAT': 'cont',
    'SET_FAN_SPEED': 'M106',
    'AUTO_FAN': 'M107'

}

TC_GCODE_ROUNDING_PRECISION = 4
SERIAL_ACK = '\r\n'

LID_TEMP = 105
TUNING_TEMPS = (70, 72, 94, 70, 40)


def get_device_info():
    info = tc.send_and_get_response(GCODES['DEVICE_INFO'])
    return info


def parse_number_from_substring(substring, rounding_val):
    '''
    Returns the number in the expected string "N:12.3", where "N" is the
    key, and "12.3" is a floating point value

    For the temp-deck or thermocycler's temperature response, one expected
    input is something like "T:none", where "none" should return a None value
    '''
    try:
        value = substring.split(':')[1]
        if value.strip().lower() == 'none':
            return None
        return round(float(value), rounding_val)
    except (ValueError, IndexError, TypeError, AttributeError):
        print('Unexpected argument to parse_number_from_substring:')
        raise Exception('Unexpected argument to parse_number_from_substring:'
                        ' {}'.format(substring))


def parse_key_from_substring(substring) -> str:
    '''
    Returns the axis in the expected string "N:12.3", where "N" is the
    key, and "12.3" is a floating point value
    '''
    try:
        return substring.split(':')[0]
    except (ValueError, IndexError, TypeError, AttributeError):
        print('Unexpected argument to parse_key_from_substring:')
        raise Exception('Unexpected argument to parse_key_from_substring:'
                        ' {}'.format(substring))


def tc_info_to_dict(response_string):
    res_dict = {}
    serial_list = response_string.split(" ")
    serial_list = list(map(lambda x: x.strip(), serial_list))
    log.debug("Serial list:{}".format(serial_list))
    for item in serial_list:
        if item == '':
            continue
        key = item.split(':')[0]
        value = item.split(':')[1]
        res_dict[key] = value
    return res_dict


def tc_response_to_dict(response_string, separator):
    res_dict = {}
    serial_list = response_string.split(separator)
    serial_list = list(map(lambda x: x.strip(), serial_list))
    log.debug("Serial list:{}".format(serial_list))
    for substr in serial_list:
        if substr == '':
            continue
        key = parse_key_from_substring(substr)
        value = parse_number_from_substring(
                                    substr,
                                    TC_GCODE_ROUNDING_PRECISION)
        res_dict[key] = value
    return res_dict


def get_tc_stats():
    global TC_STATUS
    _tc_status = {}
    tc.send_and_get_response(GCODES['DEBUG_MODE'])
    # port.reset_input_buffer()
    t = 0
    while not TUNING_OVER:
        # Get debug status
        status_res = tc.send_and_get_response(GCODES['PRINT_DEBUG_STAT'])
        _tc_status = tc_response_to_dict(status_res, '\t')
        if not _tc_status:
            continue
        temp_res = tc.send_and_get_response(GCODES['GET_PLATE_TEMP'])

        # Get current plate temp
        if temp_res:
            current_temp = tc_response_to_dict(temp_res, ' ')
            _tc_status['Plt_current'] = current_temp['C']
        TC_STATUS = _tc_status

        # Print status every 2 seconds
        if time.time() - t >= 2:
            print("TC status: {}".format(short_status()), end=" ")
            for (k, v) in TC_STATUS.items():
                if k not in PRINTY_STAT_KEYS:
                    log.debug("{}: {}".format(k, v))
            print()
            t = time.time()
        time.sleep(1)


def short_status():
    sh_st = {}
    for (k, v) in TC_STATUS.items():
        if k in PRINTY_STAT_KEYS:
            sh_st[k] = v
    return sh_st


def log_status():
    log.info("TC_status: {}".format(short_status()))
    log.info("Eutech Temp: {}".format(EUTECH_TEMPERATURE))


def get_eutech_temp(port):
    global EUTECH_TEMPERATURE
    port.write('T\r\n'.encode())
    time.sleep(10)  # takes a long time to start getting any data
    t = 0
    while not TUNING_OVER:
        temp = port.readline().decode().strip()
        if temp:
            try:
                EUTECH_TEMPERATURE = float(temp)
                if time.time() - t > 1:
                    print("EUTECH probe: {}".format(EUTECH_TEMPERATURE))
                    t = time.time()
            except ValueError:     # ignore non-numeric responses from Eutech
                pass
        time.sleep(0.01)


def tc_temp_stability_check(target, tolerance=0.02):
    # Compares sensor_op & target values over 10 seconds
    # Returns true if sensor_op is within allowed range of target throughout
    for i in range(10):
        if abs(target - TC_STATUS['Plt_current']) > tolerance:
            return False
        time.sleep(2)
    return True


def eutech_temp_stability_check(tolerance, target=None):
    # Compares sensor_op & target values over 10 seconds
    # Returns true if sensor_op is within allowed range of target throughout
    if target is None:
        prev_temp = EUTECH_TEMPERATURE
        for i in range(10):
            if abs(prev_temp - EUTECH_TEMPERATURE) > tolerance:
                return False
            prev_temp = EUTECH_TEMPERATURE
            time.sleep(2)
    else:
        for i in range(10):
            if abs(target - EUTECH_TEMPERATURE) > tolerance:
                return False
            time.sleep(2)
    return True


def offset_tuning():
    global all_c_offsets
    log.info("STARTING OFFSET TUNER..")
    while EUTECH_TEMPERATURE is None or TC_STATUS == {}:
        time.sleep(0.4)
        log.debug("Waiting for temp statuses from both thermometer & TC")

    # # Heat lid:
    tc.send_and_get_response(GCODES['SET_LID_TEMP'])
    while TC_STATUS['T.Lid'] < 105:
        pass
    # Heat up heatsink:
    tc.send_and_get_response('{} S10'.format(GCODES['SET_PLATE_TEMP']))
    set_fan(False, 0)
    while TC_STATUS['T.sink'] < 57:
        pass
    set_fan(True)
    tuning_done = False

    #Initial adjustments:
    c_adjustment(TUNING_TEMPS[0], t_sink=53, temp_tolerance=0.009)
    if args.b == True:
        b_adjustment()
        c_adjustment(TUNING_TEMPS[0], t_sink=53, temp_tolerance=0.009)

    # Thermocycler prepped. Start c tuning:
    attempts = 3
    while not tuning_done:
        if attempts != 3:
            c_adjustment(TUNING_TEMPS[0],
                         t_sink=53, temp_tolerance=0.2)
        for temp in TUNING_TEMPS[1:]:
            if c_adjustment(temp) == 'adjusted':
                # Start over
                tuning_done = False
                attempts -= 1
                break
            else:
                tuning_done = True
                continue
        if attempts == 0:
            tuning_done = True

    if attempts == 0:
        all_c_offsets.append('FAIL')
        log.info("=====> NO ATTEMPTS REMAINING! TUNING FAILED <=====")
    else:
        all_c_offsets.append(get_offset('C'))
        log.info("=====> TUNING DONE <=====")


def stabilize_everything(temp, t_sink=None):
    log.info("Waiting for thermocycler to stabilize...")
    while not tc_temp_stability_check(temp):
        time.sleep(0.5)
    log.info("TC Temperature stabilized.")
    log_status()
    if t_sink:
        log.info("Waiting for heatsink to reach 51-55 C...".format(t_sink))
        if TC_STATUS['T.sink'] - t_sink > 2:
            set_fan(False, 0.25) # increase fan speed to cool heatsink
        while abs(TC_STATUS['T.sink'] - t_sink) > 2:
            time.sleep(0.1)
        set_fan(True)
        log.info("Heatsink temperature achieved.")
        log_status()

    log.info("Waiting for Eutech probe to stabilize...")
    while not eutech_temp_stability_check(tolerance=0.01):
        time.sleep(0.5)
    log.info("EUTECH probe temp stabilized.")
    log_status()


def set_fan(auto, speed=None):
    if not auto:
        tc.send_and_get_response(
            '{} S{}'.format(GCODES['SET_FAN_SPEED'], speed))
    else:
        tc.send_and_get_response(GCODES['AUTO_FAN'])


def get_offset(param):
    res = tc.send_and_get_response(GCODES['GET_OFFSET_CONSTANTS'])
    offsets = tc_response_to_dict(res, '\n')
    val = offsets[param]
    log.info("=======================")
    log.info("Current {} is {}".format(param, val))
    log.info("=======================")
    return val


def set_offset(param, val):
    val = round(val, TC_GCODE_ROUNDING_PRECISION)
    log.info("=======================")
    log.info("Setting offset {} to {}".format(param, val)) 
    tc.send_and_get_response('{} {}{}'.format(GCODES['SET_OFFSET_CONSTANTS'],
                                              param, val))


def c_adjustment(temp, t_sink=None, temp_tolerance=0.2):
    log.info("TUNING FOR {}C ====>".format(temp))
    tc.send_and_get_response('{} S{}'.format(GCODES['SET_PLATE_TEMP'], temp))
    time.sleep(60)  # allow unit to ramp before checking stability
    stabilize_everything(temp, t_sink)
    adjust_count = 0
    while abs(EUTECH_TEMPERATURE - TC_STATUS['Plt_current']) > temp_tolerance:
        adjust_count += 1
        log.info("Temperatures unequal. Tuning c..")
        c = get_offset('C')
        new_c = c + EUTECH_TEMPERATURE - TC_STATUS['Plt_current']
        if temp_tolerance >= 0.05:
            buffer_tolerance = temp_tolerance - 0.04
            # allow up to buffer_tolerance deviation from Plt_current
            difference = round((EUTECH_TEMPERATURE - TC_STATUS['Plt_current']), 3)
            if difference > 0:
                buffer_tolerance = buffer_tolerance*(-1) 
            new_c = c + (difference + buffer_tolerance)
        set_offset('C', new_c)
        time.sleep(30)
        stabilize_everything(temp, t_sink)

    if adjust_count == 0:
        return 'not adjusted'
    else:
        return 'adjusted'


def b_adjustment():
    log.info("ADJUSTING B OFFSET ====>")
    # Eq: y = mx + k
    # y = Difference in temperatures
    # x = b_offset
    # Solve for m & k using observed datapoints
    # Then find b_offset for ideal condition of y =  94C - 40C
    get_offset('B')     # Just for comparison & documentation purposes
    b_vals = (0.005, 0.06)
    ideal_temps = (70, 94, 70, 40)
    observed_temps = {}
    for val in b_vals:
        log.info("Temps for B = {}".format(val))
        set_offset("B", val)
        b_temps = []
        for temp in ideal_temps:
            tc.send_and_get_response('{} S{}'.format(GCODES['SET_PLATE_TEMP'],
                                                     temp))
            stabilize_everything(temp)
            b_temps.append(EUTECH_TEMPERATURE)
            observed_temps[str(val)] = b_temps
            log.info(" {} -> {}".format(temp, EUTECH_TEMPERATURE))
    log.info("observed values = {}".format(observed_temps))
    x1 = b_vals[0]
    # observed for 94 - observed for 40
    y1 = observed_temps[str(x1)][1] - observed_temps[str(x1)][3]

    x2 = b_vals[1]
    # observed for 94 - observed for 40
    y2 = observed_temps[str(x2)][1] - observed_temps[str(x2)][3]
    [k, m] = get_eq_constants(x1, y1, x2, y2)
    new_b = (54 - k) / m
    set_offset("B", new_b)


def get_eq_constants(x1, y1, x2, y2):
    # Eq: y = mx + k
    # or: k + xm = y
    # Solve for k & m
    # Hence, coefficient arrays are
    m = numpy.array([[1, x1], [1, x2]])
    n = numpy.array([y1, y2])
    result_array = numpy.linalg.solve(m, n)
    return result_array


def cooldown_tc():
    log.info("Deactivating thermocycler.")
    tc.send_and_get_response(GCODES['DEACTIVATE'])
    tc.send_and_get_response('{} S{}'.format(GCODES['SET_FAN_SPEED'], 0.6))
    while TC_STATUS['T.sink'] > 30:
        time.sleep(1)
    time.sleep(60)
    log.info("Cooldown done.")
    tc.send_and_get_response(GCODES['AUTO_FAN'])


def end_tuning():
    tc.send_and_get_response(GCODES['OPEN_LID'])
    log.info("Disabling debug mode.")
    tc.send_and_get_response('{} S0'.format(GCODES['DEBUG_MODE']))


def build_arg_parser():
    arg_parser = ArgumentParser(description="Thermocycler offset tuner")
    arg_parser.add_argument("-L", "--loglevel", required=False,
                            default='INFO')
    arg_parser.add_argument("-p", "--eutech_port_name", required=True)
    arg_parser.add_argument("-m", "--tc_port_name", required=True)
    arg_parser.add_argument("-b", action = "store_true") # flag for adjusting b_offset in offset function
    arg_parser.add_argument("-f", action = "store_true") # flag to run offset tuning 3 times and calculate an appropriate c_offset
    return arg_parser


def calc_final_c_offset(record_vals):
    # Calculate and set the final c_offset
    log.info("all c_offsets = {}".format(record_vals))
    final_c = None

    # If none of the tuning attempt failed
    if 'FAIL' not in record_vals:
        log.info("============ TUNING SUCCESSFUL ===========")
        if record_vals[0] == record_vals[1]:
            final_c = (record_vals[1]+record_vals[2])/2
            log.info("Trial 1 and trial 2 equal. Averaging trial 2/3 c_offsets to get final c_offset.")
        elif record_vals[2] == record_vals[1]:
            final_c = record_vals[2]
            log.info("Trial 2 and trial 3 equal. Selecting trial 3 c_offset to get final c_offset.")
        elif (record_vals[2] > record_vals[1] and record_vals[2] < record_vals[0]) or (record_vals[2] > record_vals[0] and record_vals[2] < record_vals[1]):
            final_c = record_vals[2]
            log.info("Trial 3 in between Trial 1 and 2. Selecting trial 3 c_offset to get final c_offset.")
        elif record_vals[2] < record_vals[0] and record_vals[2] < record_vals[1]:
            final_c = (record_vals[1]+record_vals[2])/2
            log.info("All trials decreasing. Averaging trial 2/3 c_offsets to get final c_offset.")
        elif record_vals[2] == max(record_vals):
            final_c = (record_vals[1]+record_vals[2])/2
            log.info("Trial 3 has highest c_offset. Averaging trial 2/3 c_offsets to get final c_offset.")
        else:
            log.info("============ ERROR: UNACCOUNTED FOR C_OFFSET TREND ===========")
            log.info("Selecting trial 3 c_offset to get final c_offset.")
            final_c = record_vals[2]

    # if at least one tuning attempt failed
    else:
        fail_count = 0 
        log.info("============ ERROR: TUNING FAILURE REPORTED ===========")
        log.info("Please check logs to determine whether it is OK to pass this unit.")

        # count total number of failures
        for i in range(len(record_vals)):
            if record_vals[i] == 'FAIL':
                fail_count += 1

        if fail_count > 1:
            log.info("Not enough successful tunes. Returning c_offset to default of {}.".format(default_c))
            final_c = default_c
        else:
            if record_vals[0] == 'FAIL': # if first attempt fails, take average of attempt 2/3
                final_c = (record_vals[1]+record_vals[2])/2
                log.info("Trial 1 failed. Averaging trial 2/3 c_offsets to get final c_offset.")
            elif record_vals[1] == 'FAIL': # if second attempt fails, take last c_offset
                final_c = record_vals[2]
                log.info("Trial 2 failed. Selecting trial 3 c_offset to get final c_offset.")
            elif record_vals[2] == 'FAIL': # if third attempt fails, take 2nd c_offset
                final_c = record_vals[1]
                log.info("Trial 3 failed. Selecting trial 2 c_offset to get final c_offset.")
            else:
                log.info("============ ERROR: UNACCOUNTED FOR C_OFFSET TREND ===========")
                final_c = record_vals[2]

    if final_c != None:
        set_offset('C', final_c)
        log.info("============ ^^ FINAL C_OFFSET ^^ ===========")
    else:
        log.info("============ No c_offset established ===========")


if __name__ == '__main__':
    # Logging stuff
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    log_level = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(log_level, int):
        raise ValueError('Invalid log level: %s' % args.loglevel)

    # Connections
    print("Searching for connected devices..")
    # tc_port = serial_comm.find_port(OPENTRONS_VID)
    tc = serial_comm.SerialComm(args.tc_port_name, log_level)
    eutech_port = serial.Serial(args.eutech_port_name,
                                EUTECH_BAUDRATE,
                                timeout=1)
    # Disable continuous debug stat if in debug mode
    tc.send_and_get_response(GCODES['PRINT_DEBUG_STAT'])
    tc_info = get_device_info()
    TC_SERIAL = tc_info_to_dict(tc_info)['serial']

    # Configure logging
    logging.root.handlers = []
    logging.basicConfig(
                level=log_level,
                format="%(asctime)s [%(levelname) - 5.5s] %(message)s",
                handlers=[
                    logging.FileHandler("{}.log".format(TC_SERIAL)),
                    logging.StreamHandler()])

    log.info("Connected to thermocycler")
    print("================")
    log.info("[ Device info ] {}".format(tc_info))
    print("================")
    time.sleep(1)
    print("Please record the serial & model number in spreadsheet. Find the "
          "average well from QC sheet and attach thermometer probe to it")
    input("When you are ready to resume with offset tuning, press ENTER")

    # Close lid if open:
    lid_status = tc.send_and_get_response(GCODES['GET_LID_STATUS'])
    if lid_status != "Lid:closed":
        log.error("LID IS NOT CLOSED !!!")
        if input("To close lid, enter Y").upper() != 'Y':
            raise RuntimeError("Cannot proceed tuning with lid open. Exiting")
        tc.send_and_get_response(GCODES['CLOSE_LID'])
        print("Done.")

    cycles = 1
    get_offset('B')  # print b_offset at start of test
    get_offset('C')  # print c_offset at start of test

    if args.f == True:  # if flag included, run entire tuning procedure 3 times
        cycles = 3
        log.info("Flag for complete tuning, running {} cycles of tuning.".format(cycles))
        set_offset('C', default_c)
        log.info("For complete tune, starting c_offset at default of {}".format(default_c))

    # Threads
    tc_status_fetcher = threading.Thread(target=get_tc_stats, daemon=True)
    eutech_temp_fetcher = threading.Thread(target=get_eutech_temp,
                                               args=(eutech_port,), daemon=True)

    # Start threads
    eutech_temp_fetcher.start()
    tc_status_fetcher.start()

    for c in range(cycles):
        OFFSET_TUNER = threading.Thread(target=offset_tuning)
        OFFSET_TUNER.start()
        OFFSET_TUNER.join()
        cooldown_tc()

        # If first 2 tuning attempts fail, go not continue tuning
        if len(all_c_offsets) > 1:
            if all_c_offsets[0] == 'FAIL' and all_c_offsets[1] == 'FAIL':
                log.info("First 2 tuning attempts failed. Ending tuning")
                break

    TUNING_OVER = True
    end_tuning()

    # If flag included, calculate and set final c_offset using data from all 3 tuning cycles
    if args.f == True:
        calc_final_c_offset(all_c_offsets)

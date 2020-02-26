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
# tc = None
TC_SERIAL = None


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
    global record_vals
    record_vals = []

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
        log.info("=====> NO ATTEMPTS REMAINING! TUNING FAILED <=====")
    else:
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
    while not eutech_temp_stability_check(tolerance=0.019):
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
    log.info("=======================")
    tc.send_and_get_response('{} {}{}'.format(GCODES['SET_OFFSET_CONSTANTS'],
                                              param, val))


def c_adjustment(temp, t_sink=None, temp_tolerance=0.2):
    log.info("TUNING FOR {}C ====>".format(temp))
    tc.send_and_get_response('{} S{}'.format(GCODES['SET_PLATE_TEMP'], temp))
    stabilize_everything(temp, t_sink)
    if abs(EUTECH_TEMPERATURE - TC_STATUS['Plt_current']) > temp_tolerance:
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
        return 'adjusted'
        time.sleep(5)
    else:
        return 'not adjusted'
    time.sleep(3)
    stabilize_everything(temp, t_sink)

def b_adjustment():
    log.info("ADJUSTING B OFFSET ====>")
    # Eq: y = mx + k
    # y = Difference in temperatures
    # x = b_offset
    # Solve for m & k using observed datapoints
    # Then find b_offset for ideal condition of y =  94C - 40C
    get_offset('B')     # Just for comparison & documentation purposes
    b_vals = (0.005, 0.05)
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
    tc.send_and_get_response('{} S{}'.format(GCODES['SET_FAN_SPEED'], 0.6))
    tc.send_and_get_response(GCODES['OPEN_LID'])
    while TC_STATUS['T.sink'] > 30:
        time.sleep(1)
    log.info("Cooldown done.")
    tc.send_and_get_response(GCODES['AUTO_FAN'])


def end_tuning():
    # write_summary(record_vals)
    log.info("Deactivating thermocycler.")
    tc.send_and_get_response(GCODES['DEACTIVATE'])
    cooldown_tc()
    log.info("Disabling debug mode.")
    tc.send_and_get_response('{} S0'.format(GCODES['DEBUG_MODE']))


def build_arg_parser():
    arg_parser = ArgumentParser(description="Thermocycler offset tuner")
    arg_parser.add_argument("-L", "--loglevel", required=False,
                            default='INFO')
    arg_parser.add_argument("-p", "--eutech_port_name", required=True)
    arg_parser.add_argument("-b", action = "store_true")
    return arg_parser


if __name__ == '__main__':
    # Logging stuff
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    log_level = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(log_level, int):
        raise ValueError('Invalid log level: %s' % args.loglevel)

    # Connections
    print("Searching for connected devices..")
    tc_port = serial_comm.find_port(OPENTRONS_VID)
    tc = serial_comm.SerialComm(tc_port, log_level)
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

    # Threads
    tc_status_fetcher = threading.Thread(target=get_tc_stats)
    eutech_temp_fetcher = threading.Thread(target=get_eutech_temp,
                                           args=(eutech_port,))
    OFFSET_TUNER = threading.Thread(target=offset_tuning)
    OFFSET_TUNER.start()
    eutech_temp_fetcher.start()
    tc_status_fetcher.start()
    OFFSET_TUNER.join()

    end_tuning()
    TUNING_OVER = True

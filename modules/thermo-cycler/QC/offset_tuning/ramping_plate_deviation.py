import serial
import time
import threading
import logging
import serial_comm
from pathlib import PurePath
from argparse import ArgumentParser


THIS_DIR = PurePath(__file__).parent
OPENTRONS_VID = 0x04d8
EUTECH_VID = 0x0403
TC_BAUDRATE = 115200
EUTECH_BAUDRATE = 9600

EUTECH_TEMP_LEFT = None
EUTECH_TEMP_CENTER = None
EUTECH_TEMP_RIGHT = None
TC_STATUS = {}
# tc = None
TC_SERIAL = None
DEVIATION_CHECK_OVER = False

log = logging.getLogger()

PRINTY_STAT_KEYS = ['Plt_Target', 'Plt_current', 'Cov_Target', 'T.Lid',
                    'T.sink', 'Fan']
GCODES = {
    'OPEN_LID': 'M126',
    'CLOSE_LID': 'M127',
    'GET_LID_STATUS': 'M119',
    'SET_LID_TEMP': 'M140',
    'SET_PLATE_TEMP': 'M104',
    'GET_PLATE_TEMP': 'M105',
    'DEACTIVATE': 'M18',
    'DEVICE_INFO': 'M115',
    'DEBUG_MODE': 'M111',
    'PRINT_DEBUG_STAT': 'stat',
    'CONTINUOUS_DEBUG_STAT': 'cont',
    'SET_FAN_SPEED': 'M106',
    'AUTO_FAN': 'M107',
}

TC_GCODE_ROUNDING_PRECISION = 4
SERIAL_ACK = '\r\n'

CHECKPOINTS = [4, 10, 94, 70]


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
    # Get debug status
    status_res = tc.send_and_get_response(GCODES['PRINT_DEBUG_STAT'])
    _tc_status = tc_response_to_dict(status_res, '\t')
    if not _tc_status:
        return
    temp_res = tc.send_and_get_response(GCODES['GET_PLATE_TEMP'])

    # Get current plate temp
    if temp_res:
        current_temp = tc_response_to_dict(temp_res, ' ')
        _tc_status['Plt_current'] = current_temp['C']
    TC_STATUS = _tc_status

    print("TC status: {}".format(short_status()), end=" ")
    for (k, v) in TC_STATUS.items():
        if k not in PRINTY_STAT_KEYS:
            log.debug("{}: {}".format(k, v))
    print()


def short_status():
    sh_st = {}
    for (k, v) in TC_STATUS.items():
        if k in PRINTY_STAT_KEYS:
            sh_st[k] = v
    return sh_st


def log_status():
    log.info("TC_status: {}".format(short_status()))


def get_eutech_temp(port_l, port_c, port_r):
    global EUTECH_TEMP_LEFT
    global EUTECH_TEMP_CENTER
    global EUTECH_TEMP_RIGHT
    port_l.write('T\r\n'.encode())
    port_c.write('T\r\n'.encode())
    port_r.write('T\r\n'.encode())
    time.sleep(10)  # takes a long time to start getting any data
    lt = 0
    ct = 0
    rt = 0
    while not DEVIATION_CHECK_OVER:
        temp_l = port_l.readline().decode().strip()
        temp_c = port_c.readline().decode().strip()
        temp_r = port_r.readline().decode().strip()
        if temp_l:
            try:
                EUTECH_TEMP_LEFT = float(temp_l)
                if time.time() - lt > 1:
                    lt = time.time()
            except ValueError:     # ignore non-numeric responses from Eutech
                pass
        if temp_c:
            try:
                EUTECH_TEMP_CENTER = float(temp_c)
                if time.time() - ct > 1:
                    ct = time.time()
            except ValueError:     # ignore non-numeric responses from Eutech
                pass
        if temp_r:
            try:
                EUTECH_TEMP_RIGHT = float(temp_r)
                if time.time() - rt > 1:
                    rt = time.time()
            except ValueError:     # ignore non-numeric responses from Eutech
                pass
        if temp_l and temp_c and temp_r:
            log.info("Left: {}\t Center: {}\t Right: {}".format(
                    EUTECH_TEMP_LEFT, EUTECH_TEMP_CENTER, EUTECH_TEMP_RIGHT))
        time.sleep(0.1)


def tc_temp_stability_check(target, tolerance=0.02):
    # Compares sensor_op & target values over 10 seconds
    # Returns true if sensor_op is within allowed range of target throughout
    for i in range(10):
        get_tc_stats()
        if abs(target - TC_STATUS['Plt_current']) > tolerance:
            return False
        time.sleep(2)
    return True


def checkpoints_cycle():
    log.info("STARTING OPEN/CLOSE CYCLER..")
    while EUTECH_TEMP_LEFT is None or EUTECH_TEMP_CENTER is None \
            or EUTECH_TEMP_CENTER is None:
        time.sleep(0.4)
        log.debug("Waiting for temp statuses from all thermometers")
    # Make sure the lid is open..
    log.info("======> Opening lid <=======")
    tc.send_and_get_response(GCODES['OPEN_LID'])
    log.info("------- Opened -------")
    while (CHECKPOINTS):
        # Heat up heatsink:
        temp = CHECKPOINTS.pop()
        log.info("Setting TC to {}C".format(temp))
        tc.send_and_get_response('{} S{}'.format(GCODES['SET_PLATE_TEMP'],
                                 temp))
        log.info(tc.send_and_get_response(GCODES['GET_LID_STATUS']))
        time.sleep(2)   # Wait for the peltiers to start ramping
        saved_left_temp = EUTECH_TEMP_LEFT
        saved_center_temp = EUTECH_TEMP_CENTER
        saved_right_temp = EUTECH_TEMP_RIGHT

        log.info("======> Closing lid <=======")
        tc.send_and_get_response(GCODES['CLOSE_LID'])
        log.info("------- Closed -------")
        left_deviation = EUTECH_TEMP_LEFT - saved_left_temp
        center_deviation = EUTECH_TEMP_CENTER - saved_center_temp
        right_deviation = EUTECH_TEMP_RIGHT - saved_right_temp
        log.info("=======================")
        log.info("Closing deviation: left: {}\t center: {}\t right: {}".format(
                    left_deviation, center_deviation, right_deviation))
        log.info("=======================")

        stabilize_plate(temp)
        log.info("======> Opening lid <=======")
        tc.send_and_get_response(GCODES['OPEN_LID'])
        log.info("------- Opened -------")
        stabilize_plate(temp)
    log.info("=====> CHECKPOINTS DONE <=====")


def stabilize_plate(temp, t_sink=None):
    log.info("Waiting for thermocycler to stabilize...")
    get_tc_stats()
    while not tc_temp_stability_check(temp):
        get_tc_stats()
        time.sleep(0.5)
    time.sleep(5)   # Give some time for temp probes to stabilize
    log.info("TC Temperature stabilized.")
    log_status()


def set_fan(auto, speed=None):
    if not auto:
        tc.send_and_get_response(
            '{} S{}'.format(GCODES['SET_FAN_SPEED'], speed))
    else:
        tc.send_and_get_response(GCODES['AUTO_FAN'])


def cooldown_tc():
    tc.send_and_get_response('{} S{}'.format(GCODES['SET_FAN_SPEED'], 0.6))
    tc.send_and_get_response(GCODES['OPEN_LID'])
    while TC_STATUS['T.sink'] > 30:
        time.sleep(1)
        get_tc_stats()
    log.info("Cooldown done.")
    tc.send_and_get_response(GCODES['AUTO_FAN'])


def end_deviation_check():
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
    arg_parser.add_argument("-p_left", "--eutech_port_left", required=True)
    arg_parser.add_argument("-p_center", "--eutech_port_center", required=True)
    arg_parser.add_argument("-p_right", "--eutech_port_right", required=True)
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
    left_eutech_port = serial.Serial(args.eutech_port_left,
                                     EUTECH_BAUDRATE,
                                     timeout=1)
    center_eutech_port = serial.Serial(args.eutech_port_center,
                                       EUTECH_BAUDRATE,
                                       timeout=1)
    right_eutech_port = serial.Serial(args.eutech_port_right,
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
                    logging.FileHandler("dev-check-{}.log".format(TC_SERIAL)),
                    logging.StreamHandler()])

    log.info("Connected to thermocycler")
    print("================")
    log.info("[ Device info ] {}".format(tc_info))
    print("================")
    time.sleep(1)
    input("Attach thermometer probe."
          " When you are ready to resume with offset tuning, press ENTER")
    # # Heat lid:
    tc.send_and_get_response(GCODES['SET_LID_TEMP'])
    while not TC_STATUS.get('T.Lid'):
        get_tc_stats()
        time.sleep(0.5)

    while TC_STATUS.get('T.Lid') < 105:
        time.sleep(2)
        get_tc_stats()
    # Threads
    eutech_temp_fetcher = threading.Thread(target=get_eutech_temp,
                                           args=(left_eutech_port,
                                                 center_eutech_port,
                                                 right_eutech_port))
    checkpoint_cycler = threading.Thread(target=checkpoints_cycle)
    checkpoint_cycler.start()
    eutech_temp_fetcher.start()
    checkpoint_cycler.join()

    end_deviation_check()
    DEVIATION_CHECK_OVER = True

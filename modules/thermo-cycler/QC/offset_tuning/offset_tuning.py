import serial
import csv
from datetime import datetime
import time
import threading
import logging
from pathlib import PurePath
from serial.tools.list_ports import comports
from argparse import ArgumentParser


THIS_DIR = PurePath(__file__).parent
OPENTRONS_VID = 0x04d8
EUTECH_VID = 0x0403
TC_BAUDRATE = 115200
EUTECH_BAUDRATE = 9600

EUTECH_TEMPERATURE = None
TC_STATUS = {}
TUNING_OVER = False
log = logging.getLogger(__name__)
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

TC_GCODE_ROUNDING_PRECISION = 2
SERIAL_ACK = '\r\n'

LID_TEMP = 105
TUNING_TEMPS = (70, 72, 94, 70, 40)


def find_port(vid):
    retries = 5
    while retries:
        for p in comports():
            if p.vid == vid:
                print("Available: {0}->\t(pid:{1:#x})\t(vid:{2:#x})".format(
                        p.device, p.pid, p.vid))
                print("Port found:{}".format(p.device))
                time.sleep(1)
                return p.device
        retries -= 1
        time.sleep(0.5)
    raise RuntimeError('Could not find Opentrons Thermocycler'
                       'connected over USB')


def connect_to_module(port_name):
    print('Connecting to module...')
    port = serial.Serial(port_name, 115200, timeout=1)
    print('Connected.')
    return port


def get_device_info(port, lock):
    port.reset_input_buffer()
    info = send_and_get_response(port, lock, GCODES['DEVICE_INFO'])
    port.reset_input_buffer()

    return info


def send_and_get_response(tc_port, lock, cmd):
    with lock:
        response_str = ''
        cmd += SERIAL_ACK
        log.debug("Sending: {}".format(cmd))
        tc_port.write(cmd.encode())
        raw_response = bytearray(b'')
        time.sleep(0.1)
        while True:
            raw_response.extend(tc_port.readline())
            if not tc_port.in_waiting:
                break
        response_str = raw_response.decode().replace('ok', '').strip()
        log.debug("Received:{}".format(response_str))
        return response_str


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


def get_tc_stats(port, lock, over):
    global TC_STATUS
    _tc_status = {}
    send_and_get_response(port, lock, GCODES['DEBUG_MODE'])
    port.reset_input_buffer()
    t = 0
    while not over:
        # Get debug status
        status_res = send_and_get_response(port, lock,
                                           GCODES['PRINT_DEBUG_STAT'])
        _tc_status = tc_response_to_dict(status_res, '\t')
        temp_res = send_and_get_response(port, lock, GCODES['GET_PLATE_TEMP'])

        # Get current plate temp
        if temp_res:
            current_temp = tc_response_to_dict(temp_res, ' ')
            _tc_status['Plt_current'] = current_temp['C']
        TC_STATUS = _tc_status

        # Print status every 2 seconds
        if time.time() - t >= 2:
            print("TC status:", end=" ")
            for (k, v) in TC_STATUS.items():
                if k in PRINTY_STAT_KEYS:
                    print("{}: {} \t".format(k, v), end=" ")
                if k not in PRINTY_STAT_KEYS:
                    log.debug("{}: {}".format(k, v))
            print()
            t = time.time()
        time.sleep(1)


def get_eutech_temp(port, over):
    global EUTECH_TEMPERATURE
    port.write('T\r\n'.encode())
    time.sleep(10)  # takes a long time to start getting any data
    t = 0
    while not over:
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


def offset_tuning(tc_port, lock):
    global record_vals
    record_vals = []
    time.sleep(100)
    # # Heat lid:
    send_and_get_response(tc_port, lock, GCODES['SET_LID_TEMP'])
    # Heat up heatsink:
    send_and_get_response(tc_port, lock,
                          '{} S10'.format(GCODES['SET_PLATE_TEMP']))
    while TC_STATUS['T.sink'] < 58:
        pass
    tuning_done = False
    # Thermocycler prepped. Start tuning:
    while not tuning_done:
        c_adjustment(TUNING_TEMPS[0], t_sink=53, temp_tolerance=0.009)
        for temp in TUNING_TEMPS:
            log.info("==== Tuning for {}C ====".format(temp))
            if c_adjustment(temp) == 'adjusted':
                # Start over
                tuning_done = False
                break
            else:
                tuning_done = True
                continue


def c_adjustment(temp, t_sink=None, temp_tolerance=0.1):
    send_and_get_response(tc_port, lock,
                          '{} S{}'.format(GCODES['SET_PLATE_TEMP'], temp))
    log.info(" ---- Waiting for thermocycler to stabilize ----")
    while not tc_temp_stability_check(temp):
        time.sleep(0.5)
    log.info(" ---- TC Temperature stabilized ----")
    log.info(" ---- Waiting for Eutech probe to stabilize ---")
    while not eutech_temp_stability_check(tolerance=0.03):
        time.sleep(0.5)
    log.info(" ---- EUTECH probe temp stabilized ---- ")
    log.info(" ---- Waiting for heatsink to reach ~53C ----")
    if TC_STATUS['T.sink'] > 55:
        set_fan(0.4)
    elif TC_STATUS['T.sink'] < 51:
        set_fan(0)
    while abs(TC_STATUS['T.sink'] - 53) > 2:
        pass
    # turn auto fan on


def set_fan(speed):
    send_and_get_response(tc_port, lock,
            '{} {}'.format(GCODES['SET_FAN_SPEED']), speed)

# def tuning_readjustments(temp, t_sink=None, temp_tolerance=0.1):
#     global c_changed
#     c_changed = False
#
#     # Check tuning at given temp
#     log.info(" ---- Set plate to {}C and check if"
#              " tuning adjustment required ---- ".format(temp))
#     send_and_get_response(tc_port, lock,
#                           '{} S{}'.format(GCODES['SET_PLATE_TEMP'], temp))
#     # Wait until temperature stabilizes
#     while not tc_temp_stability_check(temp):
#         time.sleep(0.5)
#     log.info(" ---- TC Temperature stabilized ----")
#     while not eutech_temp_stability_check():
#         time.sleep(0.5)
#     log.info(" ---- EUTECH probe temp stabilized ---- ")
#
#     # Record experimental temperature before adjusted c_offset
#     record_vals.append([temp, EUTECH_TEMPERATURE])
#
#     # Tune the offset at tempC
#     if abs(EUTECH_TEMPERATURE - TC_STATUS['Plt_current']) > 0.1:
#         log.info(" ====> Temperatures not equal. Updating offset ====> ")
#         serial_line = send_and_get_response(tc_port,
#                                              lock,
#                                              GCODES['GET_OFFSET_CONSTANTS'])
#         offset_dict = tc_response_to_dict(serial_line, '\n')
#         c_offset = offset_dict['C']
#
#         # Determine whether EUTECH_TEMP is too low or too high
#         # allow up to +-0.1C deviation from Plt_current
#         difference = round((EUTECH_TEMPERATURE - TC_STATUS['Plt_current']),
#                        3)
#         acceptable_tolerance = 0.1
#         if difference > 0:
#             acceptable_tolerance = -0.1
#         c_offset += (difference + acceptable_tolerance)
#         log.info("Setting C offset to {}".format(c_offset))
#         c_changed = True    # change marker to show that c was changed
#         # record the new C value to add to summary csv
#         record_vals.append(['c_offset', c_offset])
#         print('record_vals = ', record_vals)
#         send_and_get_response(tc_port, lock,
#                               '{} C{}'.format(GCODES['SET_OFFSET_CONSTANTS'],
#                                               c_offset))
#         time.sleep(4)
#         retries = 10
#         while not eutech_temp_stability_check(temp, tolerance=0.1):
#             print("Temp probe not yet stable. Retry#{}".format(retries))
#             retries -= 1
#             time.sleep(5)
#             if retries == 0:
#                 raise Exception("Unable to tune offset at {}C".format(temp))
#
#     if c_changed is True:
#         record_vals.append([temp, EUTECH_TEMPERATURE])
#
#     print("\n-------------------")
#     print("Offset tuning at {}C passed".format(temp))
#     print("-------------------")


def cooldown_tc(tc_port, lock):
    send_and_get_response(tc_port, lock,
                          '{} S{}'.format(GCODES['SET_FAN_SPEED'], 0.6))
    send_and_get_response(tc_port, lock, GCODES['OPEN_LID'])
    while TC_STATUS['T.sink'] > 30:
        time.sleep(1)
    send_and_get_response(tc_port, lock, GCODES['AUTO_FAN'])


def write_summary(record_vals):
    info = get_device_info(tc_port, lock)
    start = info.find('TC')
    end = info.find(' model')
    tc_serial = info[start:end]
    print('record_vals = ', record_vals)
    f_name = "tc-tuning_{}.csv".format(tc_serial)

    with open(f_name, 'w', newline='') as f:
        writer = csv.writer(f)
        test = {'target_temp': None, 'experimental_temp': None}
        log_file = csv.DictWriter(f, test)
        log_file.writeheader()  # writes header to csv
        for item in record_vals:
            write_csv(log_file, test, item)
        writer.writerow({'{}'.format(
                        datetime.now().strftime("%m-%d-%y_%H:%M_%p"))})
        f.flush()


def write_csv(log_file, test, record_vals):
    test['target_temp'] = record_vals[0]
    test['experimental_temp'] = record_vals[1]
    log_file.writerow(test)


def end_tuning(lock, tc_port):
    write_summary(record_vals)
    log.info("Deactivating thermocycler.")
    send_and_get_response(tc_port, lock, GCODES['DEACTIVATE'])
    log.info("Disabling debug mode.")
    send_and_get_response(tc_port, lock,
                          '{} S0'.format(GCODES['DEBUG_MODE']))
    cooldown_tc(tc_port, lock)


def build_arg_parser():
    arg_parser = ArgumentParser(description="Thermocycler offset tuner")
    arg_parser.add_argument("-L", "--loglevel", required=False,
                            default='INFO')
    arg_parser.add_argument("-p", "--eutech_port_name", required=True)
    return arg_parser


if __name__ == '__main__':
    # Logging stuff
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    numeric_level = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s' % args.loglevel)
    logging.basicConfig(level=numeric_level)

    # Connections
    print("Searching for connected devices..")
    tc_port = serial.Serial(find_port(OPENTRONS_VID),
                            TC_BAUDRATE,
                            timeout=60)
    eutech_port = serial.Serial(args.eutech_port_name,
                                EUTECH_BAUDRATE,
                                timeout=1)
    lock = threading.Lock()
    # Disable continuous debug stat if in debug mode
    send_and_get_response(tc_port, lock, GCODES['PRINT_DEBUG_STAT'])
    log.info("Connected to thermocycler")
    print("-------------------")
    log.info("Device info:{}".format(get_device_info(tc_port, lock)))
    print("-------------------")
    time.sleep(1)
    print("Please record the serial & model number in spreadsheet. Find the "
          "average well from QC sheet and attach thermometer probe to it")
    input("When you are ready to resume with offset tuning, press ENTER")

    # Close lid if open:
    lid_status = send_and_get_response(tc_port, lock, GCODES['GET_LID_STATUS'])
    if lid_status != "Lid:closed":
        log.error("LID IS NOT CLOSED !!!")
        if input("To close lid, enter Y").upper() != 'Y':
            raise RuntimeError("Cannot proceed tuning with lid open. Exiting")
        send_and_get_response(tc_port, lock, GCODES['CLOSE_LID'])
        print("Done.")
        tc_port.reset_input_buffer()

    # Threads
    tc_status_fetcher = threading.Thread(target=get_tc_stats,
                                         args=(tc_port, lock, TUNING_OVER))
    eutech_temp_fetcher = threading.Thread(target=get_eutech_temp,
                                           args=(eutech_port, TUNING_OVER))
    OFFSET_TUNER = threading.Thread(target=offset_tuning, args=(tc_port, lock))
    OFFSET_TUNER.start()
    eutech_temp_fetcher.start()
    tc_status_fetcher.start()
    OFFSET_TUNER.join()

    end_tuning()
    TUNING_OVER = True  # TODO: Test this. It's probably wrong

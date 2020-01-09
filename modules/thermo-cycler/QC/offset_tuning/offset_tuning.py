import serial
import time
import threading
from pathlib import PurePath
from serial.tools.list_ports import comports

THIS_DIR = PurePath(__file__).parent
OPENTRONS_VID = 0x04d8
EUTECH_VID = 0x0403
TC_BAUDRATE = 115200
EUTECH_BAUDRATE = 9600

EUTECH_TEMPERATURE = 0
TC_STATUS = {}
TEMP_STATUS = {}

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
    'CONTINUOUS_DEBUG_STAT': 'cont'
}

TC_GCODE_ROUNDING_PRECISION = 2
SERIAL_ACK = '\r\n'

LID_TEMP = 105


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


def get_device_info(port):
    cmd = '{}{}'.format(GCODES['DEVICE_INFO'], SERIAL_ACK)
    port.write(cmd.encode())
    time.sleep(1)
    info = port.readline()
    return info


def send_tc(tc_port, lock, cmd, get_response=False):
    with lock:
        response = None
        cmd += SERIAL_ACK
        print("Sending: {}".format(cmd))
        tc_port.write(cmd.encode())
        if get_response:
            time.sleep(0.1)
            response = tc_port.readline()
        return response


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
    serial_list = response_string.decode().split(separator)
    serial_list = list(map(lambda x: x.strip(), serial_list))
    for substr in serial_list:
        if substr == '':
            continue
        key = parse_key_from_substring(substr)
        value = parse_number_from_substring(
                                    substr,
                                    TC_GCODE_ROUNDING_PRECISION)
        res_dict[key] = value
    return res_dict


def get_tc_stats(port, lock):
    send_tc(port, lock, GCODES['DEBUG_MODE'])
    time.sleep(1)
    port.reset_input_buffer()
    send_tc(port, lock, GCODES['CONTINUOUS_DEBUG_STAT'])
    time.sleep(1)
    while True:
        with lock:
            serial_line = port.readline()
            if serial_line:
                TC_STATUS = tc_response_to_dict(serial_line, '\t')
                TC_STATUS['Plt_current'] = TC_STATUS['T1'] + TC_STATUS['T2'] \
                    + TC_STATUS['T3'] + TC_STATUS['T4'] \
                    + TC_STATUS['T5'] + TC_STATUS['T6']
                print("TC status: {}".format(TC_STATUS))
                print("---------------------")
        time.sleep(0.1)


def get_eutech_temp(port):
    port.write('T\r\n'.encode())
    time.sleep(10)  # takes a long time to start getting any data
    while True:
        temp = port.readline().decode().strip()
        if temp:
            try:
                EUTECH_TEMPERATURE = float(temp)
                print("EUTECH probe: {}".format(EUTECH_TEMPERATURE))
            except:     # ignore non-numeric responses
                pass
        time.sleep(0.01)


def tc_temp_stability_check(target, tolerance=0.2):
    # Compares sensor_op & target values over 10 seconds
    # Returns true if sensor_op is within allowed range of target throughout
    for i in range(10):
        if abs(target - TC_STATUS['Plt_current']) > 0.2:
            return False
        time.sleep(1)
    return True


def eutech_temp_stability_check(target, tolerance=0.2):
    # Compares sensor_op & target values over 10 seconds
    # Returns true if sensor_op is within allowed range of target throughout
    for i in range(10):
        if abs(target - EUTECH_TEMPERATURE) > tolerance:
            return False
        time.sleep(1)
    return True


def offset_tuning(tc_port, lock):
    while not EUTECH_TEMPERATURE and not TC_STATUS:
        time.sleep(0.1)
    # 1. Set lid temp
    send_tc(tc_port, lock, '{} {}'.format(GCODES['SET_LID_TEMP'], LID_TEMP))
    while TC_STATUS['T.Lid'] < LID_TEMP:
        time.sleep(1)
    # 2. Set TC to 10C to heat up the heatsink and bring it to usual operating
    #    temp level
    send_tc(tc_port, lock, '{} {}'.format(GCODES['SET_PLATE_TEMP'], 10))
    while TC_STATUS['T.sink'] < 50:
        time.sleep(0.5)
    # 3. Set TC to 70C to start tuning
    send_tc(tc_port, lock, '{} {}'.format(GCODES['SET_PLATE_TEMP'], 70))
    # Wait until temperature stabilizes
    while not tc_temp_stability_check(70):
        time.sleep(0.5)
    # Make sure heatsink is around 53C
    while abs(53 - TC_STATUS['T.sink']) > 1:
        time.sleep(0.5)
    # Tune the offset at 70
    if EUTECH_TEMPERATURE != TC_STATUS['Plt_current']:
        serial_line = send_tc(tc_port, lock, GCODES['GET_OFFSET_CONSTANTS'],
                              get_response=True)
        offset_dict = tc_response_to_dict(serial_line, ' ')
        c_offset = offset_dict['C']
        c_offset += EUTECH_TEMPERATURE - TC_STATUS['Plt_current']
        send_tc(tc_port, lock,
                '{} C{}'.format(GCODES['SET_OFFSET_CONSTANTS'], c_offset))
        time.sleep(4)
        retries = 5
        while not eutech_temp_stability_check(70, tolerance=0):
            retries -= 1
            time.sleep(1)
            if retries == 0:
                raise Exception("Unable to tune offset at 70")
        print("-------------------")
        print("Offset tuning at 70 passed")
        print("-------------------")


if __name__ == '__main__':
    print("Searching for connected devices..")
    tc_port = serial.Serial(find_port(OPENTRONS_VID),
                            TC_BAUDRATE,
                            timeout=1)
    eutech_port = serial.Serial(find_port(EUTECH_VID),
                                EUTECH_BAUDRATE,
                                timeout=1)
    print("Connected to thermocycler")
    print("-------------------")
    print("Device info:{}".format(get_device_info(tc_port)))
    print("-------------------")
    time.sleep(1)
    print("Please record the serial & model number in spreadsheet. Find the "
          "average well from QC sheet and attach thermometer probe to it")
    input("When you are ready to resume with offset tuning, press ENTER")
    lock = threading.Lock()
    tc_status_fetcher = threading.Thread(target=get_tc_stats,
                                         args=(tc_port, lock))
    eutech_temp_fetcher = threading.Thread(target=get_eutech_temp,
                                           args=(eutech_port,))
    offset_tuner = threading.Thread(target=offset_tuning, args=(tc_port, lock))
    eutech_temp_fetcher.start()
    tc_status_fetcher.start()
    tc_status_fetcher.join()

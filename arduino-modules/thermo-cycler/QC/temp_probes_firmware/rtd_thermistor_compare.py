import serial
import time

from opentrons.legacy_api.modules.tempdeck import TempDeck

arduino_port = '/dev/ttyUSB0'
temp_deck_port = '/dev/ttyACM0'

results_file_path = 'rtd_results.csv'

start_time = time.time()


def update_temp_deck(temp_deck):
    temp_deck._driver.update_temperature()
    time.sleep(0.1)


def log_temperatures(arduino_port, temp_deck=None):
    global start_time, results_file_path
    res = ''
    for i in range(10):
        arduino_port.reset_input_buffer()
        arduino_port.write(b'1')
        time.sleep(0.2)
        res = arduino_port.readline().decode().strip().split('\r')[0]
        if res:
            break;
    temp_list = res.split('\t')
    data = '{time},{t1},{t2},{rtd},{td}'.format(
        time=round(time.time() - start_time, 2),
        t1=temp_list[0],
        t2=temp_list[1],
        rtd=temp_list[2],
        td=temp_deck.temperature if temp_deck else '0'
    )
    print(data)
    with open(results_file_path, 'a+') as f:
        f.write(data + '\r\n')


if __name__ == '__main__':
    arduino = serial.Serial(arduino_port, 115200, timeout=2)
    time.sleep(2)
    temp_deck = TempDeck(port=temp_deck_port)
    temp_deck.connect()
    temp = 110
    temp_increment = -5
    while temp > 0:
        temp_deck.set_temperature(temp)
        while temp_deck.temperature != temp_deck.target:
            time.sleep(0.2)
            update_temp_deck(temp_deck)
            log_temperatures(arduino, temp_deck)
        timestamp = time.time()
        stabilize_time = 20 * 60  # 20 minutes
        while time.time() - timestamp < stabilize_time:
            time.sleep(0.2)
            update_temp_deck(temp_deck)
            log_temperatures(arduino, temp_deck)
        temp += temp_increment
    temp_deck.deactivate()

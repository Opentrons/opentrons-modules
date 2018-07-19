import time

from opentrons.drivers.temp_deck import TempDeck


stabilize_seconds = 60 * 3
target_temperatures = [4, 95]
file_name = '/Users/andysigler/Documents/opentrons-modules/modules/temp-deck/tests/data.csv'

td = TempDeck()
error = td.connect('/dev/tty.usbmodem1411')
if error:
    print(error)
    exit()

td.update_temperature()

def log_temp():
    global file_name
    time.sleep(1)
    td.update_temperature()
    with open(file_name, 'a+') as f:
        f.write('{0}, {1}\r\n'.format(td.target, td.temperature))
    print(td.target, td.temperature)

try:
    with open(file_name, 'w+') as f:
        f.write('target, current\r\n')
    input('\nHit ENTER when ready to start...')
    for target in target_temperatures:
        print('setting temp', target)
        td.set_temperature(target)
        while td.temperature != td.target:
            log_temp()
        for i in range(stabilize_seconds):
            log_temp()
finally:
    td.disengage()

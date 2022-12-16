import test_utils
import argparse
import time

def parse_args():
    parser = argparse.ArgumentParser(description="Test GetRPM data at a speed")
    parser.add_argument('-s', '--speed', type=float, required=False, 
        default=200, help='rpm ')
    parser.add_argument('-t', '--time', type=float, required=False, 
        default=15, help='test time')
    parser.add_argument('-f', '--file', type=argparse.FileType('w'), 
        required=False, help='file to write data to')
    return parser.parse_args()

if __name__ == '__main__':
    ser = test_utils.build_serial()
    args = parse_args()

    file = args.file

    if file:
        file.write('time,speed\n')

    test_utils.set_speed(ser, speed=args.speed)
    time.sleep(5)
    start = time.time()
    min = test_utils.get_speed(ser)[0]
    max = min
    while time.time() < start + args.time:
        speed = test_utils.get_speed(ser)[0]
        if speed < min:
            min = speed 
        if speed > max:
            max = speed
        if file:
            file.write(f'{time.time()},{speed}\n')
        time.sleep(0.1)
    ser.write(b'G28\n')

    print(f'Min speed: {min} rpm')
    print(f'Max speed: {max} rpm')

import test_utils
import argparse
import time

def parse_args():
    parser = argparse.ArgumentParser(description="Cycle temperatures")
    parser.add_argument('-s', '--speed', required=False, default=100, help='rpm ')
    parser.add_argument('-t', '--time', required=False, default=15, help='test time')
    return parser.parse_args()

if __name__ == '__main__':
    ser = test_utils.build_serial()
    args = parse_args()

    test_utils.set_speed(ser, speed=args.speed)
    print('time,speed')
    time.sleep(5)
    start = time.time()
    while time.time() < start + args.time:
        speed = test_utils.get_speed(ser)
        print(f'{time.time()},{speed[0]}')
        time.sleep(0.1)
    ser.write(b'G28\n')

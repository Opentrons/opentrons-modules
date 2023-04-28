from test_utils import *


if __name__ == '__main__':
    ser = build_serial()
    ser.write('dfu\n'.encode())
    exit(0)

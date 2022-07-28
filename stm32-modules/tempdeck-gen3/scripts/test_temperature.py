#!/usr/bin/env python3

import test_utils

import argparse
import sys
import time

def parse_args():
    parser = argparse.ArgumentParser(description="Run a simple PCR cycle")
    parser.add_argument('-t', '--temp', type=float, required=True,
                        help='The target temperature to control towards')
    parser.add_argument('-p', '--port', type=str, required=False, default=None,
                        help='The USB port that the thermocycler is connected to')
    parser.add_argument('-d', '--debug', required=False, action='store_true',
                        help='Enable debugging print outputs with the temperature')
    return parser.parse_args()

def main():
    args = parse_args()

    tempdeck = test_utils.Tempdeck(port = args.port)
    time_start = time.time()
    try:
        tempdeck.deactivate()
        tempdeck.set_temperature(temp = args.temp)
        print('time\tplate temp\theatsink temp')
        while(True):
            plate, heatsink = tempdeck.get_temperatures()
            print(f'{time.time() - time_start}\t{plate}\t{heatsink}')
            time.sleep(0.25)
    except KeyboardInterrupt:
        tempdeck.deactivate()
        sys.stderr.write('Done!\n')

if __name__ == '__main__':
    main()

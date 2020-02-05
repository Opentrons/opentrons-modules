import serial
from serial.tools.list_ports import comports
import time
import threading
import logging


TC_BAUDRATE = 115200
SERIAL_ACK = '\r\n'
log = logging.getLogger(__name__)


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


class SerialComm:
    def __init__(self, port_name, log_level=logging.INFO):
        self.port = self.connect_to_module(port_name)
        self.serial_lock = threading.Lock()
        logging.basicConfig(level=log_level)
        self.port.reset_input_buffer()

    def connect_to_module(self, port_name):
        print('Connecting to module...')
        port = serial.Serial(port_name, 115200, timeout=60)
        print('Connected.')
        return port

    def send_and_get_response(self, cmd):
        with self.serial_lock:
            response_str = ''
            cmd += SERIAL_ACK
            log.debug("Sending: {}".format(cmd))
            self.port.write(cmd.encode())
            raw_response = bytearray(b'')
            time.sleep(0.1)
            while True:
                raw_response.extend(self.port.readline())
                if not self.port.in_waiting:
                    break
            response_str = raw_response.decode().replace('ok', '').strip()
            log.debug("Received:{}".format(response_str))
            return response_str


if __name__ == '__main__':
    pass

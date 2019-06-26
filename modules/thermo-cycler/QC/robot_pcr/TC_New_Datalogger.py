import aiohttp
import asyncio
import csv
import time
from argparse import ArgumentParser

IP = "10.10.3.79"
TC_SERIAL = "dummySerial"

def build_arg_parser():
    arg_parser = ArgumentParser(
        description="Thermocycler temperature data logger")
    arg_parser.add_argument("-F", "--csv_file_name", required=True)
    return arg_parser

async def fetch(client):
    async with client.get(f'http://{IP}:31950/modules/{TC_SERIAL}/data') as resp:
        assert resp.status == 200
        return await resp.json()

async def main():
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    start_ticks = time.time()
    with open('{}.csv'.format(args.csv_file_name), mode='w') as data_file:
        data_writer = csv.writer(data_file, delimiter=',', quoting=csv.QUOTE_NONE)
        data_writer.writerow(["Time", "Lid Current", "Plate Current", "Plate Target", "Hold countdown"])
    while True:
        async with aiohttp.ClientSession() as client:
            data = await fetch(client)
            with open('{}.csv'.format(args.csv_file_name), mode='a') as data_file:
                data_writer = csv.writer(data_file, delimiter=',', quoting=csv.QUOTE_NONE)
                res = time.localtime()
                timestamp = '{}:{}:{}'.format(res.tm_hour, res.tm_min, res.tm_sec)
                data_writer.writerow([timestamp, 
                                      data['data']['lidTemp'],
                                      data['data']['currentTemp'], 
                                      data['data']['targetTemp'],
                                      data['data']['holdTime']])
        await asyncio.sleep(1)

loop = asyncio.get_event_loop()
loop.run_until_complete(main())

# {
#     'status': self.status,
#     'data': {
#         'lid': self.lid_status,
#         'lidTarget': self.lid_target,
#         'lidTemp': self.lid_temp,
#         'currentTemp': self.temperature,
#         'targetTemp': self.target,
#         'holdTime': self.hold_time,
#         'rampRate': self.ramp_rate
#     }
# }
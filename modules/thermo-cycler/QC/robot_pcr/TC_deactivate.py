import aiohttp
import asyncio
from argparse import ArgumentParser

# IP = "10.10.3.79"
TC_SERIAL = "dummySerial"

def build_arg_parser():
    arg_parser = ArgumentParser(
        description="Thermocycler deactivator")
    arg_parser.add_argument("-IP", "--robot_ip_address", required=True)
    return arg_parser

async def send(client, bot_ip):
    async with client.post(f'http://{bot_ip}:31950/modules/{TC_SERIAL}', data=b'{"command_type":"deactivate"}') as resp:
        assert resp.status == 200
        return await resp.json()

async def main():
    arg_parser = build_arg_parser()
    args = arg_parser.parse_args()
    
    async with aiohttp.ClientSession() as client:
        data = await send(client, args.robot_ip_address)
        print(data)
    await asyncio.sleep(1)

loop = asyncio.get_event_loop()
loop.run_until_complete(main())
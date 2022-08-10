#!/usr/bin/env python3
"""Script to combine multiple hex files into one."""
import argparse
from dataclasses import dataclass
import re
import zlib # for crc calc
from pathlib import Path

# REGEX to split a line in hex file.


INITIAL_CRC = 0xFFFFFFFF

class HexRecord:
    PATTERN = re.compile(r"^:([A-F0-9]{2})([A-F0-9]{4})(\d{2})([A-F0-9]*)([A-F0-9]{2})$")

    _SEGMENT_ADDR_TYPE = 2
    _LINEAR_ADDR_TYPE = 4

    def __init__(self, line: str):
        """
        Parse a single line from a hex file to generate a hex record
        
        Args:
            line: The full string line to parse
        """
        record = HexRecord.PATTERN.match(line)
        if not record:
            raise RuntimeError(
                f"'{line}' is not a valid hex file formatted line."
            )
        self._bytes = int(record.groups()[0], base=16)
        self._address = int(record.groups()[1], base=16)
        self._record_type = int(record.groups()[2], base=16)
        # Data is stored as a string array
        self._data = record.groups()[3]
    
    def is_segment_address(self) -> bool:
        return self._record_type == HexRecord._SEGMENT_ADDR_TYPE

    def is_linear_address(self) -> bool:
        return self._record_type == HexRecord._LINEAR_ADDR_TYPE
    
    def is_data(self) -> bool:
        return self._record_type == 0

    def get_segment_address_data(self) -> int:
        """If the record type is an address, get the offset data"""
        offset = int(self._data, base=16)
        return offset * 16
    
    def get_linear_address_data(self) -> int:
        """If the record type is a linear address, get the offset"""
        offset = int(self._data, base=16)
        return offset << 16

    def get_record_address(self) -> int:
        """
        All records have this field, but it is only usefulf or data records
        """
        return self._address
    
    def get_data(self) -> bytearray:
        return bytearray.fromhex(self._data)
        

class HexInfo:
    """Gathers the integrity info for a hex file"""

    def __init__(self, file: str, start_addr: int):
        """
        Parse a hex file and calculate a CRC to be stored within it
        
        Args:
            file: the path to the input file to read
            start_addr: the lowest address in the hex file that should be included
                        with the CRC calculation
        """
        self._crc = INITIAL_CRC
        self._len = 0
        self._current_extended_offset = 0
        self._current_linear_offset = 0
        self._start_address = start_addr
        with Path(file).open("r") as hex_file:
            # Each line of a hex file is known as a "record"
            for line in hex_file.readlines():
                self._read_single_hex_record(line)

    def _read_single_hex_record(self, line: str) -> None:
        """Read a single record (line) from a hex file"""
        # We are only interested in data and offset commands.
        #  https://en.wikipedia.org/wiki/Intel_HEX
        record = HexRecord(line)
        if record.is_data():
            address = ( record.get_record_address() + 
                        self._current_extended_offset + 
                        self._current_linear_offset)
            if address < self._start_address:
                return
            data = record.get_data()
            self._crc = zlib.crc32(data, self._crc)
            self._len += len(data)
            if len(data) != 0x10:
                print(f'Weird length {hex(len(data))} at {hex(address)}')
        elif record.is_linear_address():
            self._current_linear_offset = record.get_linear_address_data()
        elif record.is_segment_address():
            self._current_linear_offset = record.get_segment_address_data()



def hex_file_calc_crc(file: str, start_addr: int) -> HexInfo:
    info = HexInfo(crc=123456, len=0)


def main() -> None:
    """Entry point."""
    parser = argparse.ArgumentParser(description="Calculate the CRC32 of a hex file.")
    parser.add_argument(
        "input",
        metavar="INPUT",
        type=str,
        required=True,
        help="path of hex file to read",
    )
    parser.add_argument(
        "start",
        metavar="START",
        type=int,
        required=False,
        default=0x8008400,
        help="the starting address to calculate the crc from"
    )
    parser.add_argument(
        "output",
        metavar="OUTPUT",
        type=str,
        help=""
    )

    args = parser.parse_args()

    info = HexInfo(args.input, args.start)


if __name__ == "__main__":
    main()

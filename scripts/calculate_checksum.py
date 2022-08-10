#!/usr/bin/env python3
"""
Script to generate firmware integrity information for the STM32 module
startup application.

The script takes an Intel Hex file as an input, and parses it to generate
integrity information that can then be loaded into a binary using objcopy.

"""
import argparse
from dataclasses import dataclass
import re
import zlib # for crc calc
from pathlib import Path

# REGEX to split a line in hex file.


INITIAL_CRC = 0xFFFFFFFF

class HexRecord:
    """Represents a single record in a hex file."""
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
        All records have this field, but it is only useful for data records
        """
        return self._address
    
    def get_data(self) -> bytearray:
        return bytearray.fromhex(self._data)
        

class HexInfo:
    """
    Gathers the integrity info for a hex file.

    Upon initialization, this class:
      - Takes a first pass through the hex file to see the total length
        of the data, and allocates a byte array of that length filled 
        with 0xFF. The length does not include anything below the
        specified start_address.
      - Reads the file again, this time populating the internal byte array
        with the information held in the hex file.
      - Calculates a CRC32 using the standard Ethernet polynomial and a
        starting value of 0xFFFFFFFF, the same as the STM32 CRC peripheral.
    """

    def __init__(self, file: str, start_address: int):
        """
        Parse a hex file and calculate a CRC to be stored within it
        
        Args:
            file: the path to the input file to read
            start_addr: the lowest address in the hex file that should be included
                        with the CRC calculation
        """
        self._current_extended_offset = 0
        self._current_linear_offset = 0
        self._start_address = start_address
        with Path(file).open("r") as hex_file:
            # First pass, get the total length of the file
            self._max_address = 0
            for line in hex_file.readlines():
                address = self._read_record_for_address(line)
                if address > self._max_address:
                    self._max_address = address
        size = self._max_address - self._start_address
        # Create a bytearray filled with 0xFF as large as our hex
        self._data = bytearray([0xFF] * size)
        self._current_extended_offset = 0
        self._current_linear_offset = 0
        with Path(file).open("r") as hex_file:
            # Second pass, read the contents of the file into our array
            for line in hex_file.readlines():
                self._read_single_hex_record(line)
            self._crc = zlib.crc32(self._data, INITIAL_CRC) & 0xffffffff

    def _read_record_for_address(self, line: str) -> int:
        """
        Read a single record (line) from a hex file and
        return the max address of the record (aka the starting
        address + the number of bytes) 
        """
        record = HexRecord(line)
        if record.is_data():
            return ( record.get_record_address() + 
                     self._current_extended_offset + 
                     self._current_linear_offset +
                     len(record.get_data()))
        elif record.is_linear_address():
            self._current_linear_offset = record.get_linear_address_data()
        elif record.is_segment_address():
            self._current_linear_offset = record.get_segment_address_data()
        return 0

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
            address -= self._start_address
            data = record.get_data()
            # Copy the record data
            for i in range(len(data)):
                self._data[i+address] = data[i]
        elif record.is_linear_address():
            self._current_linear_offset = record.get_linear_address_data()
        elif record.is_segment_address():
            self._current_linear_offset = record.get_segment_address_data()
    
    def _int32_to_bytes(number: int) -> bytearray:
        """Write a big-endian int32 into a bytearray"""
        ret = bytearray(4)
        ret[0] = (number >> 24) & 0xFF
        ret[1] = (number >> 16) & 0xFF
        ret[2] = (number >> 8) & 0xFF
        ret[3] = (number >> 0) & 0xFF
        return ret

    def crc(self) -> int:
        return self._crc
    
    def size(self) -> int:
        return len(self._data)

    def serialize(self, name: str) -> bytearray:
        """
        Outputs a byte-serialized representation of the class.
        
        Order:
            32 bit CRC
            32 bit Byte Count
            32 bit Start Address
            Null-terminated string containing the name of the device
        """
        ret = HexInfo._int32_to_bytes(self._crc)
        ret += HexInfo._int32_to_bytes(len(self._data))
        ret += HexInfo._int32_to_bytes(self._start_address)
        ret += name.encode()
        ret += '\0'.encode()
        return ret

def main() -> None:
    """Entry point."""
    parser = argparse.ArgumentParser(description="Calculate the CRC32 of a hex file.")
    parser.add_argument(
        "input",
        metavar="INPUT",
        type=str,
        help="path of hex file to read",
    )
    parser.add_argument(
        "name",
        metavar="NAME",
        type=str,
        help="The name of the module, which may be checked by the startup app"
    )
    parser.add_argument(
        "start",
        metavar="START",
        type=str,
        help="the starting address to calculate the crc from"
    )
    parser.add_argument(
        "output",
        metavar="OUTPUT",
        type=str,
        help="The file to write the data to"
    )

    args = parser.parse_args()

    start = int(args.start, 0)

    print(f'Reading from {args.input} starting at {hex(start)}')

    info = HexInfo(args.input, start)

    print(f'  Program is {info.size()} bytes from start address')
    print(f'  crc32 is {hex(info.crc())}')

    with Path(args.output).open("w+b") as output_file:
        output_file.write(info.serialize(args.name))
    
    print(f'Wrote to {args.output} succesfully')

if __name__ == "__main__":
    main()

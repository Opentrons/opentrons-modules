#!/usr/bin/env python3
"""
Script to generate firmware integrity information for the STM32 module
startup application.

The script takes an Intel Hex file or a raw binary file as an input, and 
parses it to generate integrity information that can then be loaded into 
a binary using objcopy.
"""
import argparse
import re
import zlib # for crc calc
import sys
from pathlib import Path

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

    def crc(self) -> int:
        return self._crc
    
    def size(self) -> int:
        return len(self._data)

    def start_address(self) -> int:
        return self._start_address


class BinInfo:
    """
    Gathers the integrity info for a raw binary file.

    Upon initialization, this class:
      - Reads the length of the bin file, subtracting the start_offset value
      - Discards the first start_offset bytes
      - Reads the rest of the file and calculates a CRC32 using the standard 
        Ethernet polynomial and a starting value of 0xFFFFFFFF, the same as 
        the STM32 CRC peripheral.
    """

    def __init__(self, file: str, start_address: int, file_start: int = 0x08008000):
        """
        Parse a bin file and calculate a CRC to be stored within it
        
        Args:
            file: the path to the input file to read
            start_address: the lowest address in the bin file that should be included
                        with the CRC calculation
            file_start: the memory address of the first byte in the file
        """
        self._start_offset = start_address - file_start
        self._file_start = file_start
        with Path(file).open("rb") as bin_file:
            bin_file.seek(self._start_offset)
            data = bin_file.read()
            self._size = len(data)
            self._crc = zlib.crc32(data) & 0xffffffff
            
    def crc(self) -> int:
        return self._crc
    
    def size(self) -> int:
        return self._size

    def start_address(self) -> int:
        return self._file_start + self._start_offset

def int32_to_bytes(number: int) -> bytearray:
    """Write a little-endian int32 into a bytearray"""
    ret = bytearray(4)
    ret[3] = (number >> 24) & 0xFF
    ret[2] = (number >> 16) & 0xFF
    ret[1] = (number >> 8) & 0xFF
    ret[0] = (number >> 0) & 0xFF
    return ret
    
def serialize(info, name: str) -> bytearray:
    """
    Outputs a byte-serialized representation of the class.
    
    Order:
        32 bit CRC
        32 bit Byte Count
        32 bit Start Address
        Null-terminated string containing the name of the device
    """
    ret = int32_to_bytes(info.crc())
    ret += int32_to_bytes(info.size())
    ret += int32_to_bytes(info.start_address())
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
        help="path of hex or bin file to read",
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
    filename = str(args.input)


    if filename.count('.bin') > 0:
        info = BinInfo(filename, start)
    elif filename.count('.hex') > 0:
        info = HexInfo(filename, start)
    else:
        print(f'ERR: file {filename} must be a .bin or .hex file', file=sys.stderr)
        exit(-1)

    print(f'Reading from {filename} starting at {hex(start)}')
    print(f'  Program is {info.size()} bytes from start address')
    print(f'  crc32 is {hex(info.crc())}')

    with Path(args.output).open("w+b") as output_file:
        output_file.write(serialize(info, args.name))
    
    print(f'Wrote to {args.output} succesfully')
    exit(0)

if __name__ == "__main__":
    main()

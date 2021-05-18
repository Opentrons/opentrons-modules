#!/usr/bin/env python3

import argparse
import csv
import sys

class NTCG104ED104DTDSXGenerator:
    def __init__(self, csvfile,
                 which_resistance,
                 at_space_depth = 0,
                 incremental_space_depth = 2):
        self._csv = csvfile
        next(self._csv) # part number header
        next(self._csv) # R25 coefficient
        next(self._csv) # B25 coefficient
        next(self._csv) # empty line
        headers = next(self._csv)
        next(self._csv) # units
        self._lines = [l for l in self._csv]
        self._extractor = {'nominal': self.Nom, 'min': self.Min, 'max': self.Max}[which_resistance]
        self._start_space = ' ' * at_space_depth
        self._incremental_space = ' ' * incremental_space_depth

    @staticmethod
    def Temp(elems) -> float:
        return elems[0]

    @staticmethod
    def Min(elems) -> float:
        return elems[1]

    @staticmethod
    def Nom(elems) -> float:
        return elems[2]

    @staticmethod
    def Max(elems) -> float:
        return elems[3]

    @staticmethod
    def B25T(elems) -> float:
        return elems[4]

    @staticmethod
    def negDT(elems) -> float:
        return elems[5]

    @staticmethod
    def posDT(elems) -> float:
        return elems[6]

    def includes(self):
        return '\n'.join(['#include <cstdint>', '#include <array>', '#include <utility>'])

    def name(self):
        return 'NTCG104ED104DTDSX'

    def tablename(self):
        return f'{self.name()}_TABLE'

    def array_type(self):
        return f'std::array<std::pair<double, int16_t>, {len(self._lines)}>'

    def generate_header(self):
        return '\n'.join([
            self.includes(),
            'namespace lookups {',
            f'{self._incremental_space} [[nodiscard]] auto {self.name()}(void) -> const {self.array_type()}&;',
            '};',
            ''
        ])

    def generate_source(self):
        output_lines = [f'{{ {self._extractor(l)}, {self.Temp(l)} }}' for l in self._lines]
        joiner = ',\n' + self._incremental_space + self._start_space
        output_body = self._start_space + self._incremental_space + joiner.join(output_lines)
        total = len(output_lines)
        array_header = f'{self._start_space}{self.array_type()} {self.tablename()} = {{ {{'
        array_footer = self._start_space + '} };\n'
        function = '\n'.join([
            f'[[nodiscard]] auto lookups::{self.name()}(void) -> const {self.array_type()}& {{',
            f'{self._incremental_space}return {self.tablename()};',
            '}'
            ])
        return '\n'.join([
            self.includes(),
            '#include "thermistor_lookups.hpp"',
            '',
            '',
            array_header,
            output_body,
            array_footer,
            '',
            function,
            ''
        ])

def get_args():
    parser = argparse.ArgumentParser(__file__, 'Generate a thermistor lookup table')
    parser.add_argument('infile',  metavar='INCSV', type=argparse.FileType('r'),
                        help='The CSV to read from')
    parser.add_argument('outheader', metavar='OUTHEADER',
                        type=argparse.FileType('w'),
                        help='The destination path to write the generated header file'
                        )
    parser.add_argument('outsource', metavar='OUTSOURCE',
                        type=argparse.FileType('w'),
                        help='The destination path to write the generated source file')
    return parser.parse_args()

def generate(args):
    generator = NTCG104ED104DTDSXGenerator(csv.reader(args.infile), 'nominal')
    args.outheader.write(generator.generate_header())
    args.outsource.write(generator.generate_source())

if __name__ == '__main__':
    args = get_args()
    generate(args)

#!/usr/bin/env python3

import argparse
import csv
import sys

class ThermistorGenerator:
    '''
    Base class for thermistor generators. Inherited classes should set up the following:
      - In the __init__ function, populate a variable `self._lines` containing every line with
        data to be written
      - Define `name(self)` to return the name of the table
      - Define `generate_table(self) to return a C++ table containing lookups from uint16_t to float
        (from the input temperature to the expected resistance of the thermistor)
    '''
    def __init__(self):
        self._lines = []

    def name(self):
        raise NotImplementedError()
    def tablename(self):
        return f'{self.name()}_TABLE'

    def array_type(self):
        return f'std::array<std::pair<double, int16_t>, {len(self._lines)}>'

    def generate_header(self):
        return '\n'.join([
            f'{self._incremental_space}struct {self.name()}{{',
            f'{self._incremental_space}// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)',
            f'{self._incremental_space}{self._incremental_space}[[nodiscard]] auto operator()() -> const {self.array_type()}&;',
            f'{self._incremental_space}}};'
        ])

    def generate_function(self):
        return '\n'.join([
            f'[[nodiscard]] auto lookups::{self.name()}::operator()() -> const {self.array_type()}& {{',
            f'{self._incremental_space}return {self.tablename()};',
            '}'
            ])

class NTCG104ED104DTDSXGenerator(ThermistorGenerator):
    def __init__(self, csvfile,
                 which_resistance,
                 at_space_depth = 0,
                 incremental_space_depth = 2):
        self._csv = csv.reader(csvfile)
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

    def name(self):
        return 'NTCG104ED104DTDSX'

    def generate_table(self):
        output_lines = [f'{{ {self._extractor(l)}, {self.Temp(l)} }}' for l in self._lines]
        joiner = ',\n' + self._incremental_space + self._start_space
        output_body = self._start_space + self._incremental_space + joiner.join(output_lines)
        total = len(output_lines)
        array_header = f'{self._start_space}{self.array_type()} {self.tablename()} = {{ {{'
        array_footer = self._start_space + '} };\n'
        return '\n'.join([
            array_header,
            output_body,
            array_footer,
        ])
    

# Requires a CSV file manually copied from an xlsv file from
# the part vendor. Contains temperature & nominal resistance
class KS103J2Generator(ThermistorGenerator):
    def __init__(self, csvfile,
                 at_space_depth = 0,
                 incremental_space_depth = 2):
        self._csv = csv.reader(csvfile)
        headers = next(self._csv)
        self._lines_full = [l for l in self._csv]
        # CSV includes an absurd number of elements, incrementing
        # by 0.05 C per line. We will stick with 1C increments
        # because the converter code assumes int for temperature
        self._lines = self._lines_full[::20]
        self._start_space = ' ' * at_space_depth
        self._incremental_space = ' ' * incremental_space_depth
    
    @staticmethod
    def Temp(elems) -> int:
        temporary = float(elems[0])
        return int(temporary)
    
    @staticmethod
    def Resistance(elems) -> float:
        # Need to return kilohms
        return float(elems[1]) / 1000
    
    def name(self):
        return 'KS103J2G'
    
    def generate_table(self):
        output_lines = [f'{{ {self.Resistance(l)}, {self.Temp(l)} }}' for l in self._lines]
        joiner = ',\n' + self._incremental_space + self._start_space
        output_body = self._start_space + self._incremental_space + joiner.join(output_lines)
        total = len(output_lines)
        array_header = f'{self._start_space}{self.array_type()} {self.tablename()} = {{ {{'
        array_footer = self._start_space + '} };\n'
        return '\n'.join([
            array_header,
            output_body,
            array_footer,
        ])

class NXFT15XV103FA2B030Generator(NTCG104ED104DTDSXGenerator):
    def __init__(self, csvfile,
                 which_resistance,
                 at_space_depth = 0,
                 incremental_space_depth = 2):
        self._csv = csv.reader(csvfile, delimiter=' ', skipinitialspace=True)
        next(self._csv) # R25 coefficient
        next(self._csv) # B25 coefficient
        next(self._csv) # empty line
        headers = next(self._csv)
        next(self._csv) # units
        self._lines = [l for l in self._csv]
        self._extractor = {'nominal': self.Nom, 'min': self.Min, 'max': self.Max}[which_resistance]
        self._start_space = ' ' * at_space_depth
        self._incremental_space = ' ' * incremental_space_depth

    def name(self):
        return 'NXFT15XV103FA2B030'

# Meta-generator that generates source/header files containing the thermistor
# table classes from above
class SourceAndHeaderGenerator:
    def __init__(self, ntc_file, ks_file, nxf_file,
                 at_space_depth = 0,
                 incremental_space_depth = 2):
        self.generators = []
        if ntc_file:
            self.generators.append(NTCG104ED104DTDSXGenerator(ntc_file, 'nominal'))
        if ks_file:
            self.generators.append(KS103J2Generator(ks_file))
        if nxf_file:
            self.generators.append(NXFT15XV103FA2B030Generator(nxf_file, 'nominal'))

    def includes(self):
        return '\n'.join(['#include <cstdint>', '#include <array>', '#include <utility>'])
    
    def generate_header(self):
        ret = '\n'.join([
            '#pragma once',
            '',
            self.includes(),
            'namespace lookups {'
        ])
        for generator in self.generators:
            ret += '\n' + generator.generate_header()
        return ret + '\n}\n'

    def generate_source(self):
        ret = '\n'.join([
            self.includes(),
            '#include "thermistor_lookups.hpp"',
            '',
            '',
        ])

        for generator in self.generators:
            ret += '\n' + generator.generate_table() + '\n\n' + generator.generate_function() + '\n'

        return ret

def get_args():
    parser = argparse.ArgumentParser(__file__, 'Generate a thermistor lookup table')
    parser.add_argument('--ntcfile',  type=argparse.FileType('r'),
                        required=False, default=None,
                        help='The CSV to read NTCG104 info from')
    parser.add_argument('--nxffile',  type=argparse.FileType('r'),
                        required=False, default=None,
                        help='The CSV to read NXFT15 info from')
    parser.add_argument('--ksfile', type=argparse.FileType('r'),
                        required=False, default=None,
                        help='The CSV to read KS103j2 info from')
    parser.add_argument('outheader', metavar='OUTHEADER',
                        type=argparse.FileType('w'),
                        help='The destination path to write the generated header file'
                        )
    parser.add_argument('outsource', metavar='OUTSOURCE',
                        type=argparse.FileType('w'),
                        help='The destination path to write the generated source file')
    return parser.parse_args()

def generate(args):
    generator = SourceAndHeaderGenerator(args.ntcfile,
                                         args.ksfile,
                                         args.nxffile)
    args.outheader.write(generator.generate_header())
    args.outsource.write(generator.generate_source())

if __name__ == '__main__':
    args = get_args()
    generate(args)

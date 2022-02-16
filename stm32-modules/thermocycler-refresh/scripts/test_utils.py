#!/usr/bin/env python3

from matplotlib import pyplot as pp, gridspec
import argparse
import csv
import io
import json
import re
import serial
import datetime
import time
from enum import Enum

from serial.tools.list_ports import grep
from typing import Any, Callable, Dict, Tuple, List, Optional

from dataclasses import dataclass, asdict

def build_parent_parser(desc: str) -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=desc)
    p.add_argument('-o', '--output', choices=['plot','json', 'csv'],
                   action='append',
                   help='Output type; plot draws a matplotlib graph, json dumps a data blob. Pass twice to do both.')
    p.add_argument('-f', '--file', type=argparse.FileType('w'), help='Path to write the json blob (ignored if not -ojson)',
                   default=None, dest='output_file')
    p.add_argument('-p','--port', type=str, help='Path to serial port to open. If not specified, will try to find by usb details', default=None)
    p.add_argument('-s', '--sampling-time', dest='sampling_time', type=float, help='How frequently to sample in seconds', default=0.1)
    return p

def build_serial(port: str = None) -> serial.Serial:
    if not port:
        avail = list(grep('.*hermocycler*'))
        if not avail:
            raise RuntimeError("could not find thermocycler")
        return serial.Serial(avail[0].device, 115200)
    return serial.Serial(port, 115200)

def guard_error(res: bytes, prefix: bytes=  None):
    if res.startswith(b'ERR'):
        raise RuntimeError(res)
    if prefix and not res.startswith(prefix):
        raise RuntimeError(f'incorrect response: {res} (expected prefix {prefix})')


_TEMP_RE = re.compile('^M141 T:(?P<target>.+) C:(?P<temp>.+) OK\n')
def get_lid_temperature(ser: serial.Serial) -> float:
    ser.write(b'M141\n')
    res = ser.readline()
    guard_error(res, b'M141')
    res_s = res.decode()
    match = re.match(_TEMP_RE, res_s)
    return float(match.group('temp'))

_TEMP_DEBUG_RE = re.compile('^M105.D HST:(?P<HST>.+) FRT:(?P<FRT>.+) FLT:(?P<FLT>.+) FCT:(?P<FCT>.+) BRT:(?P<BRT>.+) BLT:(?P<BLT>.+) BCT:(?P<BCT>.+) HSA.* OK\n')
# Returns: Heatsink, Right Plate, Left Plate, Center Plate
def get_plate_temperatures(ser: serial.Serial) -> Tuple[float, float, float, float]:
    ser.write(b'M105.D\n')
    res = ser.readline()
    guard_error(res, b'M105.D')
    res_s = res.decode()
    match = re.match(_TEMP_DEBUG_RE, res_s)
    temp_r = (float(match.group('FRT')) + float(match.group('BRT'))) / 2.0
    temp_c = (float(match.group('FCT')) + float(match.group('BCT'))) / 2.0
    temp_l = (float(match.group('FLT')) + float(match.group('BLT'))) / 2.0
    temp_hs = float(match.group('HST'))
    return temp_hs, temp_r, temp_l, temp_c

_THERMAL_POWER_RE = re.compile(
    '^M103.D L:(?P<L>.+) C:(?P<C>.+) R:(?P<R>.+) H:(?P<H>.+) F:(?P<F>.+) OK\n'
)
def get_thermal_power(ser: serial.Serial) -> Tuple[float, float, float, float, float]:
    ser.write(b'M103.D\n')
    res = ser.readline()
    guard_error(res, b'M103.D ')
    res_s = res.decode()
    match = re.match(_THERMAL_POWER_RE, res_s)
    left = float(match.group('L'))
    center = float(match.group('C'))
    right = float(match.group('R'))
    heater = float(match.group('H'))
    fans = float(match.group('F'))
    print(res)
    return left, center, right, heater, fans

_PLATE_TEMP_RE = re.compile('^M105 T:(?P<target>.+) C:(?P<temp>.+) OK\n')
# JUST gets the base temperature of the plate
def get_plate_temperature(ser: serial.Serial) -> float:
    ser.write(b'M105\n')
    res = ser.readline()
    guard_error(res, b'M105')
    res_s = res.decode()
    match = re.match(_PLATE_TEMP_RE, res_s)
    return float(match.group('temp'))

# Sets peltier PWM as a percentage. Be careful!!!!!
def set_peltier_debug(power: float, direction: str, peltiers: str, ser: serial.Serial):
    if(power < 0.0 or power > 1.0):
        raise RuntimeError(f'Invalid power input: {power}')
    if(direction != 'C' and direction != 'H'):
        raise RuntimeError(f'Invalid direction input: {direction}')
    if(peltiers != 'L' and peltiers != 'R' and peltiers != 'C' and peltiers != 'A'):
        raise RuntimeError(f'Invalid peltier selection: {peltiers}')
    
    print(f'Setting peltier {peltiers} to {direction} at power {power}')
    ser.write(f'M104.D {peltiers} P{power} {direction}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M104.D OK')
    print(res)

# Sets fan PWM as a percentage. Loud.
def set_fans_manual(power: float, ser: serial.Serial):
    if(power< 0.0 or power > 1.0):
        raise RuntimeError(f'Invalid power input: {power}')
    percent = power * 100
    print(f'Setting fan PWM to {percent}%')
    ser.write(f'M106 S{power}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M106 OK')
    print(res)

# Turns fans to automatic mode
def set_fans_automatic(ser: serial.Serial):
    print(f'Setting fans to automatic mode')
    ser.write(f'M107\n'.encode())
    res = ser.readline()
    guard_error(res, b'M107 OK')
    print(res)

# Sets heater PWM as a percentage.
def set_heater_debug(power: float, ser: serial.Serial):
    if(power< 0.0 or power > 1.0):
        raise RuntimeError(f'Invalid power input: {power}')
    percent = power * 100
    print(f'Setting heater PWM to {percent}%')
    ser.write(f'M140.D S{power}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M140.D OK')
    print(res)

# Sets the heater target as a temperature in celsius
def set_lid_temperature(temperature: float, ser: serial.Serial):
    print(f'Setting lid temperature target to {temperature}C')
    ser.write(f'M140 S{temperature}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M140 OK')
    print(res)

# Turn off the heater!
def deactivate_lid(ser: serial.Serial):
    print('Deactivating heater')
    ser.write('M108\n'.encode())
    res = ser.readline()
    guard_error(res, b'M108 OK')
    print(res)

# Set the heater PID constants
def set_heater_pid(p: float, i: float, d: float, ser: serial.Serial):
    print(f'Setting heater PID to P={p} I={i} D={d}')
    ser.write(f'M301 SH P{p} I{i} D{d}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M301 OK')
    print(res)

# Sets the plate target as a temperature in celsius
def set_plate_temperature(temperature: float, ser: serial.Serial):
    print(f'Setting plate temperature target to {temperature}C')
    ser.write(f'M104 S{temperature}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M104 OK')
    print(res)

# Turn off the plate!
def deactivate_plate(ser: serial.Serial):
    print('Deactivating plate')
    ser.write('M14\n'.encode())
    res = ser.readline()
    guard_error(res, b'M14 OK')
    print(res)

# Set the peltier PID constants
def set_peltier_pid(p: float, i: float, d: float, ser: serial.Serial):
    print(f'Setting peltier PID to P={p} I={i} D={d}')
    ser.write(f'M301 SP P{p} I{i} D{d}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M301 OK')
    print(res)

# Debug command to move the hinge motor
def move_lid_angle(angle: float, overdrive: bool, ser: serial.Serial):
    print(f'Moving lid by {angle}º overdrive = {overdrive}')
    if(overdrive):
        ser.write(f'M240.D {angle} O\n'.encode())
    else:
        ser.write(f'M240.D {angle}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M240.D OK')
    print(res)

# Debug command to engage/disengage the solenoid.
def set_solenoid(engaged: bool, ser: serial.Serial):
    value = 1
    if not engaged:
        value = 0
    print(f'Setting solenoid to {engaged}')
    ser.write(f'G28.D {value}\n'.encode())
    res = ser.readline()
    guard_error(res, b'G28.D OK')
    print(res)

def move_seal_steps(steps: int, ser: serial.Serial):
    print(f'Moving seal by {steps} steps')
    ser.write(f'M241.D {steps}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M241.D OK')
    print(res)

class SealParam(Enum):
    VELOCITY = 'V'
    ACCELERATION = 'A'
    STALLGUARD_THRESHOLD = 'T'
    STALLGUARD_MIN_VELOCITY = 'M'
    RUN_CURRENT = 'R'
    HOLD_CURRENT = 'H'


# Debug command to set a seal parameter
def set_seal_param(param: SealParam, value: int, ser: serial.Serial):
    print(f'Setting {param} ({param.value}) to {value}')
    ser.write(f'M243.D {param.value} {value}\n'.encode())
    res = ser.readline()
    guard_error(res, b'M243.D OK')
    print(res)

class PositionStatus(Enum):
    Open = 0
    Closed = 1
    Unknown = 2

# Debug command to get lid status
# Returns [lid status, seal status]
_LID_STATUS_RE = re.compile('^M119 Lid:(?P<lid>.+) Seal:(?P<seal>.+) OK\n')
def get_lid_status(ser: serial.Serial) -> Tuple[PositionStatus, PositionStatus]:
    print('Getting lid status')
    ser.write(f'M119\n'.encode())
    res = ser.readline()
    guard_error(res, b'M119 Lid:')
    #print(res)
    res_s = res.decode()
    match = re.match(_LID_STATUS_RE, res_s)
    lid_s = str(match.group('lid'))
    seal_s = str(match.group('seal'))
    ret = [PositionStatus.Unknown, PositionStatus.Unknown]
    if lid_s == 'closed':
        ret[0] = PositionStatus.Closed
    elif lid_s == 'open':
        ret[0] = PositionStatus.Open
    return ret

# Function to fully open the lid
def open_lid(ser: serial.Serial):
    lid_status = get_lid_status(ser)[0]
    if lid_status == PositionStatus.Open:
        print('Lid already open')
        return
    print('Opening lid')
    set_solenoid(True, ser)
    move_lid_angle(120, False, ser)
    move_lid_angle(3, True, ser)
    set_solenoid(False, ser)

# Function to fully close the lid
def close_lid(ser: serial.Serial):
    lid_status = get_lid_status(ser)[0]
    if lid_status == PositionStatus.Closed:
        print('Lid already closed')
        return
    print('Closing lid')
    move_lid_angle(-120, False, ser)
    move_lid_angle(-3, True, ser)
    
# UTILITIES FOR STEP RESPONSE

def sample_until_condition(
        ser: serial.Serial,
        period: float,
        sampler: Callable[[serial.Serial], any],
        done: Callable[[serial.Serial], bool] ):
    try:
        print('Beginning data collection')
        while True:
            sampler(ser)
            if done(ser):
                break
            time.sleep(period)
    except KeyboardInterrupt:
        print('Sampling complete (keyboard interrupt')
    except RuntimeError as re:
        print(f'Sampling complete (Error: {re}')

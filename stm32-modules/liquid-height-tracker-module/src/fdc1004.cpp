// Key facts you need from the datasheet:
//     -I2C address is fixed: 0x50
//     -Registers: MEAS1_MSB..MEAS4_LSB = 0x00-0x07, CONF_MEAS1..4 = 0x08-0x0B, FDC_CONF = 0x0C, 
//         MANUFACTURER_ID = 0xFE (expect 0x5449), DEVICE_ID = 0xFF (expect 0x1004)
//     -CONF_MEASx (16-bit): bits [15:13] = CHA (channel, 0-3), 
//         bits [12:10] = CHB (0b111 = disabled = single-ended), 
//         bits [9:5] = CAPDAC (0-31)
//     -FDC_CONF: bit 15 = reset, bits [11:10] = rate (01=100Hz, 10=200Hz,
//         11=400Hz), bits [3:0] = trigger/done flags for channels 1-4 (bit 3-channel)
//     -Result conversion: combine MSB<<8 | (LSB>>8) into a signed 24-bit value, 
//         sign-extend from bit 23, then capacitance_pF = raw / 2^19 + capdac * 3.125

#include "liquid-height-tracker-module/fdc1004.hpp"

using namespace fdc1004;


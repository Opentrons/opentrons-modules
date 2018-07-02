# Mag Deck

The [Opentrons Mag-Deck](https://shop.opentrons.com/products/magdeck)

The Mag-Deck is controlled by GCodes over a USB serial-port, making it easy to write control software or simply send commands manually over a serial-port or with an application like [CoolTerm](http://freeware.the-meiers.org/).

The rest of this document details the GCode commands available for the Mag-Deck, and the responses it will send back.

1. [Overview](#overview)
   * [Terminator and Acknowledgement](#terminator-and-acknowledgement)
2. [Commands](#commands):
   * [Home](#home): `G28.2`
   * [Move](#move): `G0`
   * [Get Position](#get-position): `M114.2`
   * [Probe Plate](#probe-plate): `G38.2`
   * [Get Plate Position](#get-plate-position): `M836`
   * [Get Device Information](#get-device-information): `M115`
   * [Enter USB Bootloader](#enter-usb-bootloader): `dfu`

## Overview

Below are the commands the Mag-Deck can receive, and the expected responses. The commands and responses are heavily borrowed from [GCode](https://en.wikipedia.org/wiki/G-code) and [Smoothieware's implementation](http://smoothieware.org/supported-g-codes).

In the example commands and responses shown below, values are shown as their byte representation, so a return character followed by a newline character will look like `\r\n`. This means that when the Mag-Deck sends a response `ok\r\nok\r\n`, when printed out in ASCII it would look like the following:
```
ok
ok
```

Also in the examples, commands sent from a host machine are prepended with a `->`. Responses sent back from a Mag-Deck are prepended with a `<-`.

### Terminator and Acknowledgement

* The Mag-Deck's serial port operates at `115200` baudrate
* All command sequences must end with a return and newline character (`\r\n`), in order to get a response from the Mag-Deck
* Multiple individual commands can be combined into the same sequence, all ending with a single `\r\n`.
* Once the device has received a `\r\n` character sequence, it will process all commands immediately and in order of arrival.
* After processing all received commands, the device will respond with it's acknowledgement characters, `ok\r\nok\r\n`.

Example where the host simply sends the return and newline characters:
```
-> \r\n
<- ok\r\nok\r\n
```

Example where the host sends data that the Mag-Deck does not expect, followed by the terminating characters:
```
-> foobarfoobarfoobar\r\n
<- ok\r\nok\r\n
```

## Commands
### Home
#### Code: `G28.2`

Home the device, lowering the platform downwards until the lower homing-endstop is triggered. Once homed, the current position will be set as position `0`.

#### Example:

Homing the device downward until the lower-endstop is triggered:
```
-> G28.2\r\n
<- ok\r\nok\r\n
```

### Move
#### Code: `G0`

Sets the device's position in millimeters. The target position can be a floating point or integer.

#### Arguments:
* `Z`: The target coordinate along the `Z` axis (in millimeters)

#### Example:

Move to 10.12 millimeters above the lower homing-endstop:
```
-> G0 Z10.12\r\n
<- ok\r\nok\r\n
```

### Get Position
#### Code: `M114.2`

Get the device's current position, specifically the distance in millimeters from the lower homing-endstop. If the device has just been powered on, or the motor has skipped for some reason, this value will not be accurate.

#### Example:
Getting the current position:
```
-> M114.2\r\n
<- 12.34\r\nok\r\nok\r\n
```

### Probe Plate
#### Code: `G38.2`

The sets the device into a probing state, where the platform will slowly rise at low current, and will purposefully press into any attached piece of labware (labware must be securely attached so it can withstand the upward force).

After pressing upwards, the platform will move downwards, measuring the distance it travels until triggering the lower homing-endstop.

#### Example:
```
-> G38.2\r\n
<- \r\nok\r\nok\r\n
```

### Get Plate Position
#### Code: `M836`

Retrieve the measured distance found from a previously executed probe command (`G38.2`), in millimeters.

#### Example:
```
-> M836\r\n
<- 12.34\r\nok\r\nok\r\n
```

### Get Device Information
#### Code: `M115`
Gets the device's unique serial identifier, hardware model, and firmware version.
#### Example:
```
-> M115\r\n
<- serial:MDV0118052801 model:mag_deck_v1 version:edge-11aa22b\r\nok\r\nok\r\n
```

### Enter USB Bootloader
#### Code: `dfu`
Put the device into the USB bootloader, to receive a new firmware image from the host machine, using `avrdude`. After receiving this command, the device will wait 1 second before entering the bootloader.
#### Example:
Placing the device into bootloader mode:
```
-> dfu\r\n
<- Restarting and entering bootloader in 1 second...\r\nok\r\nok\r\n
```

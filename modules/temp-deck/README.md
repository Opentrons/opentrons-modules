# Temperature Deck

The [Opentrons Temperature-Deck](https://shop.opentrons.com/products/tempdeck) is a device that holds a steady temperature between 4-95 degrees Celsius, and is designed to connect seamlessly with the OT2 liquid handler.

The Temperature-Deck is controlled by GCodes over a USB serial-port, making it easy to write control software or simply send commands manually over a serial-port or with an application like [CoolTerm](http://freeware.the-meiers.org/).

The rest of this document details the GCode commands available for the Temperature-Module, and the responses it will send back.

1. [Overview](#overview)
   * [Terminator and Acknowledgement](#terminator-and-acknowledgement)
3. [Commands](#commands):
   * [Disengage](#disengage): `M18`
   * [Set Temperature](#set-temperature): `M104`
   * [Get Temperature](#get-temperature): `M105`
   * [Get Device Information](#get-device-information): `M115`
   * [Enter USB Bootloader](#enter-usb-bootloader): `dfu`

## Overview

Below are the commands the Temperature-Deck can receive, and the expected responses. The commands and responses are heavily borrowed from [GCode](https://en.wikipedia.org/wiki/G-code) and [Smoothieware's implementation](http://smoothieware.org/supported-g-codes).

In the example commands and responses shown below, values are shown as their byte representation, so a return character followed by a newline character will look like `\r\n`. This means that when the Temperature-Deck sends a response `ok\r\nok\r\n`, when printed out in ASCII it would look like the following:
```
ok
ok
```

Also in the examples, commands sent from a host machine are prepended with a `->`. Responses sent back from a Temperature-Deck are prepended with a `<-`.

### Terminator and Acknowledgement

* The Temperature-Deck's serial port operates at `115200` baudrate
* All command sequences must end with a return and newline character (`\r\n`), in order to get a response from the Temperature-Deck
* Multiple individual commands can be combined into the same sequence, all ending with a single `\r\n`.
* Once the device has received a `\r\n` character sequence, it will process all commands immediately and in order of arrival.
* After processing all received commands, the device will respond with it's acknowledgement characters, `ok\r\nok\r\n`.

Example where the host simply sends the return and newline characters:
```
-> \r\n
<- ok\r\nok\r\n
```

Example where the host sends data that the Temperature-Deck does not expect, followed by the terminating characters:
```
-> foobarfoobarfoobar\r\n
<- ok\r\nok\r\n
```

## Commands
### Disengage
#### Code: `M18`

Stops the device from holding any target temperature, and will fully power-down the the device. In the case that the device is told to disengage while it is at a hot temperature, it will actively cool down to a temperature that will not burn (about 55 degrees Celsius).

#### Example:

Stopping the device from holding a temperature:
```
-> M18\r\n
<- ok\r\nok\r\n
```

### Set Temperature
#### Code: `M104`

Sets the device's target temperature in degrees Celsius. The target value can be a floating point or integer.

#### Arguments:
* `S`: The target temperature in degrees Celsius (`S<celsius>`)
* `P`: The Kp value used by the device's PID control loop (`P<Kp>`)
* `I`: The Ki value used by the device's PID control loop (`I<Ki>`)
* `D`: The Kd value used by the device's PID control loop (`D<Kd>`)

#### Example:

Setting the temperature to `42.123` degrees:
```
-> M104 S42.123\r\n
<- ok\r\nok\r\n
```

Setting the temperature to `98` degrees, and changing the PID loop's Kp, Ki, and Kd tunings:
```
-> M104 S98 P0.4 I0.2 D0.2\r\n
<- ok\r\nok\r\n
```

### Get Temperature
#### Code: `M105`

Get the device's current temperature, and the target temperature. If the device is not targetting a temperature (disengaged), then it will return `none` as it's target temperature.

In the device's response, the target temperature is labelled as `T`, and the current temperature is labelled as `C`.

#### Example:
Getting the temperature while targetting `85` degrees Celsius:
```
-> M105\r\n
<- T:85.000 C:42.123\r\nok\r\nok\r\n
```
Getting the temperature while the device is disengaged:
```
-> M105\r\n
<- T:none C:42.123\r\nok\r\nok\r\n
```

### Get Device Information
#### Code: `M115`
Gets the device's unique serial identifier, hardware model, and firmware version.
#### Example:
Getting the temperature while targetting `85` degrees Celsius:
```
-> M115\r\n
<- serial:TDV0118052801 model:temp_deck_v1 version:edge-11aa22b\r\nok\r\nok\r\n
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

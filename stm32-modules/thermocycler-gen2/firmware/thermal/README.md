# Thermal Subsystem

The Thermocycler contains two primary thermal subassemblies, the _peltier plate_ and the _lid heater_.

## Peltier Plate

The plate consists of a 3 column array of peltier modules, where each column is individually controllable (referred to as the __left__, __right__, and __center__ regions). A total of six thermistors (one per peltier region) are mounted on the plate for thermal feedback.

### Plate Offset

The temperature measured by the thermistors is not necessarily equal to the temperature of the plate. In order to compensate for this temperature differential, a set of calibration coefficients are provided to make a more accurate estimate of the temperature in the wells.

- There are three offset coefficents, `A`, `B`, and `C`.
- There are two inputs to the calibration function: The current uncalibrated temperature of the thermistor (`T`), and the current temperature measured on the heat sink (`H`).
- The output is the Calibrated Temperature, `Tc`

The offset compensation is as follows: `Tc = (A * H) + ((B + 1) * T) + C`

Furthermore, there are separate B and C coefficients for each channel (L/C/R).

### Ramp Control & Volumetric Overshoot

When a new target temperature is set, the `plate_control` module enters a simple state machine. In order to ensure that the temperature of liquids in the plate is appropriate, the state machine includes __volumetric overshoot__ based on the maximum volume of liquid in a well (configured by the command to set the plate temperature).

```mermaid
graph TD
    Start[New command]
    Overshoot{Target temperature > 0.5ºC away?}
    OvershootYes(Set target based on overshoot calculation)
    OvershootNo(Set target to the user-specified temperature)
    HotOrCold{Target > current temperature?}
    InitialHeat(Initial Heat)
    InitialCool(Initial Cool)
    OvershootPhase(Stay in overshoot temperature)
    HoldingPhase(Stay at user-specified temperature indefinitely)
    Off(Off)

    Start -->|Set temp command| Overshoot
    Overshoot -->|Yes| OvershootYes --> HotOrCold
    Overshoot -->|No| OvershootNo --> HotOrCold
    HotOrCold -->|Yes| InitialHeat
    HotOrCold -->|No| InitialCool
    InitialHeat -->|Target temperature reached| OvershootPhase
    InitialCool -->|Target temperature reached| OvershootPhase
    OvershootPhase -->|10 seconds pass| HoldingPhase

    Start -->|Disable command| Off
```

### Integral Windup Compensation

A byproduct of PID control is integral windup, wherein the integral term grows so much while moving to a target that it causes a significant overshoot after reaching the target. The firmware deals with this in two ways:
- When moving to a temperature that is _away_ from ambient temperature (relative to starting temperature), the firmware utilizes `conditional integration`. The peltiers are controlled open-loop until getting within the "proportional band" from the target, at which point the PID control begins.
- When moving to a temperature that is _towards_ ambient temperature (relative to starting temperature), the firmware uses PID control for the entire ramp. However, the PID controller is armed to reset the integral term once the error from the target temperature is less than 3ºC. This prevents excessive overshoot that `contional integration` would cause in this scenario.

### Heating to cold temperatures

When __heating__ to a temperature that is less than the current heatsink temperature, special compensation must be made to reduce unwanted overshoot. The firmware sets the effective Overshoot Target to __2ºC__ less than the "actual" target; the peltiers will overshoot this target and end up very close to the original overshoot target.

### Thermistor Drift Errors

The firmware raises an error if the thermistors on the plate seem to have excessively drifted. If the following conditions are met, then the thermal plate task enters an error state until the device is reset:
- There is an active temperature target
- The plate control has finisehd the Overshoot Phase and 30 additional seconds have passed
- The hottest thermistor and the coldest thermistor on the plate are more than 4ºC apart from one another

## Lid Heater

The lid heater consists of a single resistive heating pad and a single thermistor for temperature feedback. The lid's primary purpose is to prevent condensation in the wells by providing a temperature above that of the peltier plate.

## Fans

### Control

The fans are controlled by a PWM channel on Timer 16

### Tachometer Feedback

The tachometers for the fans are monitored by Channels 1 and 2 on Timer 4. The input channels are configured for Input Capture monitoring on rising edges. Testing shows that the approximate low range of the fan tachometer is ~40 pulses per second

- The timer is set to run at 100kHz and run for a total period of 0.25 seconds
- When the timer starts, a DMA transaction is configured for each channel to move the first three Input Capture readings into buffers. The first reading is effectively ignored.
- When the timer overflows, the difference between the two readings is saved into a variable for each channel. This gives the _period_ of the fan tachometer.
- - The interrupt also clears the buffers, in order to be able to detect an idle fan.

The timer is configured in One Pulse Mode, so the interrupt is free to reenable the timer and it will start counting from 0.

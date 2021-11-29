# Thermocycler Refresh firmware source

This folder (stm32-modules/thermocycler-refresh/firmware) contains code that _only_ runs on the cross-built microcontroller binary.

## Peripheral Allocation

### Oscillators/Debug
Pin | Use
--- | ---
PF0 | High freq osc in
PF1 | High freq osc out
PC14 | Low freq osc in
PC15 | High freq osc out
PA14 | SWDIO Clock
PA13 | SWDIO Data

### Timers
Timer | Use | Notes
----- | --- | -----
TIM1 | Peltier PWM (CH1, CH2, and CH3) | 25kHz
TIM7 | System base timer | 1000Hz (milliseconds timer)
TIM15 | Heater PWM (CH1) | 25kHz

### USB
Pin | Use
--- | ---
PA11 | USB DM
PA12 | USB DP

### Thermal System
Pin | Configuration | Usage
--- | ---------- | -----
PC4 | I2C2 SCL| ADC SCL
PA8 | I2C2 SDA | ADC SDA
PA9 | Rising Interrupt w/ pullup | ADC 1 Alert
PA10 | Rising Interrupt w/ pullup | ADC 2 Alert
PC0 | TIM1 CH1 PWM | Left Peltier Drive
PA7 | GPIO Out | Left Peltier Direction
PC1 | TIM1 CH2 PWM | Center Peltier Drive
PB0 | GPIO Out | Center Peltier Direction
PC2 | TIM1 CH3 PWM | Right Peltier Drive
PB1 | GPIO Out | Right Peltier Direction
PE7 | GPIO Out | Peltier enable (active __high__)
PA2 | TIM15 CH1 PWM | Heater Drive
PD7 | GPIO Out | Heater enable (active __high__)

### Misc
Pin | Use
--- | ---
PE6 | Heartbeat LED (active __high__)

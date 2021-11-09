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
Timer | Use
----- | ---
TIM7 | System base timer

### USB
Pin | Use
--- | ---
PA11 | USB DM
PA12 | USB DP

### Misc
Pin | Use
--- | ---
PE6 | Heartbeat LED (active __high__)

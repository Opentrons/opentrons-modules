# Thermistor Task
The thermistor task runs on a fixed frequency basis. The control loop is as follows:
- Sleep for 100 milliseconds
- Read each thermistor via the ADS1115
- Send the updated readings to the Thermal Control Task

## Changing Heatdeck Temperature with Arduino

#### Step 1
Install Arduino IDE on [Windows](https://www.arduino.cc/en/Guide/Windows) or [Mac](https://www.arduino.cc/en/Guide/MacOSX). This is where you can open the Heatdeck code, and edit the temperature value.
#### Step 2
[Download the opentrons-modules repository](https://github.com/OpenTrons/opentrons-modules.git), and open the Heatdeck Arduino code, named `heatdeck.ino`.
#### Step 3
Edit the variable `target_temperature` to your desired temperature. For example, if I wanted 45 degrees C, I would set:
```arduino
const double target_temperature = 45;
```
#### Step 4
Make sure you are connected to the Heatdeck by [selecting the correct board and port](https://www.arduino.cc/en/Guide/ArduinoUno#toc5). The Heatdeck uses an `Arduino/Genuino Uno`.
#### Step 5
[Uploaded Program](https://www.arduino.cc/en/Guide/ArduinoUno#toc6), and make sure no errors have occured.

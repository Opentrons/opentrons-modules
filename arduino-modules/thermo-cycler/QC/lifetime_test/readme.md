################## Lifetime test TC_datalogger script ######################

This is the script for controlling a Thermocycler to go through a set number of
1- hour PCR cycles. In the 1 hour, the thermocycler will cycle through multiple
temperatures defined in the script.

To run the script:
1. Upload firmware from `TC_lifetime_test` branch onto the thermocycler.
2. In a terminal/ commandline window, navigate to this directory (`/opentrons-modules/modules/thermo-cycler/QC/lieftime_test`)
3. Run the script with the following command:
`python TC_datalogger.py -P port_name -F a_filename`
Write the actual port name and specify a file name. The script will create a new
csv file with this name.

For example:
  `python TC_datalogger.py -P /dev/cu.usbmodem1421 -F lifetimeTest_TC_1`
or
  `python TC_datalogger.py -P COM10 -F lifetimeTest_TC_2`

The script will deactivate the thermocycler when all the runs are completed.
To stop/ abort the script, enter Ctrl+C in the terminal/command line window.
You can then use Arduino serial monitor to deactivate the thermocycler by entering
the gcode `M18`

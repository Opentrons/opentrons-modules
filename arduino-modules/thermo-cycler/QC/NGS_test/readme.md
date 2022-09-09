################## NGS test script ######################

This is the script for controlling a Thermocycler to go through a set number of
1- hour PCR cycles. In the 1 hour, the thermocycler will cycle through multiple
temperatures defined in the script.

To run the script:
1. Upload firmware onto the thermocycler.
2. In a terminal/ commandline window, navigate to this directory (`/opentrons-modules/modules/thermo-cycler/QC/NGS_test`)
3. Run the script with the following command:
`python NGS_test_protocol.py -P port_name -F a_filename`
Write the actual port name and specify a file name. The script will create a new
csv file with this name.

For example:
  `python TC_datalogger.py -P /dev/cu.usbmodem1421 -F lifetimeTest_TC_1`
or
  `python TC_datalogger.py -P COM10 -F lifetimeTest_TC_2`

To stop/ abort the script, enter Ctrl+C in the terminal/command line window.
You can then use Arduino serial monitor to deactivate the thermocycler by entering
the gcode `M18`

---------------------------
TROUBLESHOOTING TIPS:
---------------------------
If you get errors while running the script:
1. If you get an error saying 'serial not recognized' then you will need to install pyserial module by typing in commandline- `python3 -m pip install pyserial`. Once the module is installed, run the script again

2. If you get syntax errors, make sure you are running python3. You can check your version by typing `python --version` in the commandline window. Usually the default for most systems is python 2 but you will mostly already have python3 on your computer. You can check this with `python3 --version`. If you see a python3 version, then run the script by typing `python 3 NGS_test_protocol.py ....` instead of just `python`.

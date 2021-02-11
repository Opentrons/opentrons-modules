## Running an NGS prep protocol on a robot ##
There are 3 scripts in this directory:

1. `tc_science_test_HWv3.py`: The protocol (template) to run NGS prep on a robot
2. `TC_New_Datalogger.py`: A script that will run on a computer to get current
thermocycler status data from the robot and save to a csv file.
3. `TC_deactivate`: A script to run on the user's computer to deactivate
the thermocycler until the app provides a way to do this.

# Using the scripts: #

1. Push the latest api version from edge branch onto the robot.
2. Open the run app with appropriate flags:
Using a terminal, navigate to opentrons repository and enter
`make -C app dev OT_APP_DEV_INTERNAL__ENABLE_THERMOCYCLER=1`
3. In the app, connect to your robot. Make sure APIv2 flag is enabled. If it
isn't, enable it and power cycle the robot.
4. Connect the thermocycler and upload your protocol (make sure you have made
  any changes necessary to the template provided).
5. Calibrate all labware.
6. *Before starting a run*, using a terminal/shell, run TC_New_Datalogger.py:
  `python TC_New_Datalogger.py -F ngs_thermocycler_data`
  This will start logging thermocycler status data (plate & lid temperature)
  into the csv file.
7. Start the protocol run.
8. When the protocol is done, it should keep the thermocycler plate at the
  holding temperature defined in the protocol. Once the labware in thermocycler
  has been removed, deactivate the thermocycler by running:
  `python TC_deactivate.py`

Lid Testing Instructions- WINDOWS OS
-------------------------------------
*Pulling latest changes on branch from git:*
1. Open Git Bash
2. Navigate to firmware directory:
`cd Documents\opentrons\opentrons-modules`
3. Check you branch name (should show up in parentheses next to the current path). If you need to switch to a different branch, do `git checkout branch_name`
4. Make sure you don't have any uncommitted changes by checking with `git status`
5 Pull with `git pull`

*Uploading firmware:*
1. Open Arduino
2. Open the thermocycler sketch (if not already open) from File->sketchbook->modules->thermo-cycler->thermo-cycler-arduino
3. Go to Tools->Board and select 'Opentrons Thermocycler M0'
4. Go to Tools->Port and select the thermocycler port (should show up as a COM port with description- 'Opentrons Thermocycler M0')
5. Upload the firmware by clicking the arrow button below the menu items or from Sketch->Upload

*Running the python script:*
To run the script we need to know the COM port that the thermocycler's connected to.
You can find it out from Arduino (Tools->Port) or from Device manager->Ports
1. Open Command Prompt (cmd)
2. cd into the script directory: `Documents\opentrons\opentrons-modules\modules\thermo-cycler\QC\lid_testing`
3. The script, TC_lid_tester.py, takes 2 arguments to successfully run:
       -P: COM port of thermocycler
       -F: filename to save data into
So, eg command to run the script:
`python TC_lid_tester.py -P COM3 -F lid_test_data`

*The script will run until 1000 rounds of Open+Close are complete.
To change the number of rounds, see TC_lid_tester.py, line 8.
To stop the script before that, use Ctrl+Pause/Break in the cmd window.*

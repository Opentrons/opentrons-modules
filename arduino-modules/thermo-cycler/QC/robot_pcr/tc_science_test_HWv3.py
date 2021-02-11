## Thermocycler Protocol Template

COVER_TEMP = 100
PLATE_TEMP_PRE = 10
PLATE_TEMP_HOLD_1 = (94, 180)
PLATE_TEMP_HOLD_2 = (94, 10)
PLATE_TEMP_HOLD_3 = (70, 30)
PLATE_TEMP_HOLD_4 = (72, 30)
PLATE_TEMP_HOLD_5 = (72, 300)
PLATE_TEMP_POST = 4 
CYCLES = 29

def run_temp_profile(thermocycler):
    ## Close lid ######
    thermocycler.close()
    ## Set LID temp
    thermocycler.heat_lid()
    thermocycler.wait_for_lid_temp()
    #_____Lid temp reached____
    ## Set HOLD1 temp
    thermocycler.set_temperature(PLATE_TEMP_HOLD_1[0], PLATE_TEMP_HOLD_1[1])
    thermocycler.wait_for_hold()
    for i in range(CYCLES):
        ## Set HOLD2 temp
        thermocycler.set_temperature(PLATE_TEMP_HOLD_2[0], PLATE_TEMP_HOLD_2[1])
        thermocycler.wait_for_hold()
        ## Set HOLD3 temp
        thermocycler.set_temperature(PLATE_TEMP_HOLD_3[0], PLATE_TEMP_HOLD_3[1])
        thermocycler.wait_for_hold()
        ## Set HOLD4 temp
        thermocycler.set_temperature(PLATE_TEMP_HOLD_4[0], PLATE_TEMP_HOLD_4[1])
        thermocycler.wait_for_hold()
    ## Set HOLD5 temp
    thermocycler.set_temperature(PLATE_TEMP_HOLD_5[0], PLATE_TEMP_HOLD_5[1])
    thermocycler.wait_for_hold()
    thermocycler.stop_lid_heating()
    ## Set LAST temp
    thermocycler.set_temperature(PLATE_TEMP_POST)

def run(ctx):
    plate = ctx.load_labware_by_name('generic_96_wellplate_340ul_flat', 2)
    tiprack = ctx.load_labware_by_name('opentrons_96_tiprack_300ul', 1)
    thermocycler = ctx.load_module('thermocycler', 10)
    magmod = ctx.load_module('magdeck', 3)
    plate_mag = magmod.load_labware_by_name('biorad_96_wellplate_200ul_pcr')
    if thermocycler.lid_status != 'open':
        thermocycler.open()
    tc_plate = thermocycler.load_labware_by_name('biorad_96_wellplate_200ul_pcr')
    # pipettes
    pipette = ctx.load_instrument('p300_multi', 'left', tip_racks=[tiprack])
    
    ## Set PRE temp
    thermocycler.set_temperature(PLATE_TEMP_PRE)
    thermocycler.wait_for_temp()
    ctx.pause("Robot paused. Put the sample plate in thermocycler and press Resume")
    
    # To bypass a calibration bug
    pipette.pick_up_tip(tiprack.wells()[0])
    pipette.aspirate(100, plate_mag.wells()[0])
    pipette.dispense(100, plate_mag.wells()[1])
    pipette.drop_tip()

    # Your transfers
    pipette.transfer(200, plate.wells(), tc_plate.wells())
    # Start cycling
    thermocycler.lid_target = COVER_TEMP
    run_temp_profile(thermocycler)
    # Should wait here
    ## Open lid 
    # thermocycler.open()
    # And carry on with transfers, if any
    
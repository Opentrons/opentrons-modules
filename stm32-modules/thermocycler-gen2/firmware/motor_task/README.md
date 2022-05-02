# Motor Task

## Lid state machines

### Overall state machine
```mermaid
graph TD
    A(Idle)
    C{Check state}
    D{Check state}
    F(Retract Seal)
    Falt(Retract Seal)
    G(Extend Seal)
    E[Send ACK and return to idle]
    H(Open hinge motor)
    I(Close hinge motor)

    LiftCheck{Lid is open?}
    LiftFail[Return error]
    LiftRun(Run Plate Lift action)

    A -->|Open Lid command| C
    A -->|Close Lid command| D
    C ---->|Lid open| E
    D ---->|Lid closed| E
    C -->|Lid closed or unknown| F
    D -->|Lid Open or unknown| Falt
    F --> H 
    Falt --> I
    I --> G 
    G --> E
    H ---> E
    A -->|Plate Lift command| LiftCheck
    LiftCheck -->|No| LiftFail
    LiftCheck -->|Yes| LiftRun --> E
```

### Seal motor sub-state machine
```mermaid
graph TD
    Start1[Extend seal]
    OpenFull1(Open full distance)
    RetractFull1(Retract until stall)
    Done1[Done]
    
    Start1 ----->|Seal is extended| Done1
    Start1 ---->|Seal is homed| OpenFull1
    Start1 -->|Seal position unknown| RetractFull1
    RetractFull1 ---> OpenFull1
    OpenFull1 --> Done1

    Start2[Retract Seal]
    RetractFull2(Retract until stall)
    OpenSome2(Open a few mm)
    Done2[Done]

    Start2 -->|Seal position unknown| OpenSome2
    Start2 ---->|Seal is extended| RetractFull2
    Start2 -->|Seal is homed| Done2
    OpenSome2 --> RetractFull2
    RetractFull2 --> Done2
```

### Hinge motor sub-state machine
```mermaid
graph TD
    StartO[Open hinge]
    OpenToSwitch(Open to limit switch)
    OpenOverdrive(Overdrive into limit switch)
    DoneO[Done]
    StartC[Close hinge]
    CloseToSwitch(Close to limit switch)
    CloseOverdrive(Overdrive into limit switch)
    DoneC[Done]
    LiftStart[Plate Lift]
    LiftOpen(Open past 90º)
    LiftReturn(Close past limit switch)
    RunOpenHinge[Start Open Hinge action]

    LatchO1(Open latch)
    LatchO2(Close latch)
    LatchC1(Open latch)
    LatchC2(Close latch)

    StartO -->|Hinge at open position| DoneO
    StartO -->|Hinge close or unknown| LatchO1 --> OpenToSwitch
    OpenToSwitch --> LatchO2 --> OpenOverdrive --> DoneO

    StartC -->|Closed switch engaged| DoneC
    StartC -->|Closed switch not engaged| LatchC1 --> CloseToSwitch
    CloseToSwitch --> CloseOverdrive --> LatchC2 --> DoneC

    LiftStart --> LiftOpen --> LiftReturn --> RunOpenHinge
```

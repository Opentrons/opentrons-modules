# Firmware Architecture
This section provides a general overview of the firmware architecture for the Temp Deck Gen3.

## Drivers
Hardware peripherals attached to the STM32 are classified into drivers. These drivers are not given their own tasks, but are rather utilized _by_ the software tasks. Lower level hardware control, such as simple access to internal peripherals, is covered by 'Hardware Policy' code. Drivers are distinct in that they maintain their own state.

- __EEPROM Driver__ - Provides functionality to read and write the EEPROM on the system over I2C. See `../../include/common/core/m24128.hpp`
- __LED Driver__ - Provides functionality to write to the LED's on the system. Uses I2C.
- __PID Driver__ - Provides a unified interface to calculate Proportional Integral Derivative control. See `../../include/common/core/pid.hpp`
- __ADS1115 Driver__ - Provides an interface to control the ADS1115 ADC IC on the main board. See `../../include/common/core/ads1115.hpp`
- __Thermistor Conversion Driver__ - Provides an interface to convert an ADC reading into a temperature in ºC.
- __GCode Parser__ - Accepts a buffer of characters and attempts to parse out any valid GCode commands.

## Hardware Policy
Hardware Policy drivers provide low-level access to peripherals that don't require their own state tracking. Additionally, when compiling for a host-side target (tests or simulator), the policy is what changes to allow the task & hardware drivers to simulate their functionality. The Task Descriptions below all contain a list of the functionalities implemented by the task's respective policy.

## Tasks
The functionality of the Temp Deck Firmware is split into a set of FreeRTOS tasks. Each task maintains its own stack, and the relative priority between tasks can be configured to reflect the importance of timing between the different system tasks.

The interface to each task is defined by the definition of its message queue. In `tempdeck-gen3/tasks.hpp`, a parameterized definition of these tasks is defined. This header is generic so that different targets (firmware vs tests vs simulator) can define the actual message queue separately. For example, `firmware/firmware_tasks.hpp` provides the specific definitions for the firmware target. This file is only needed in the main.cpp for the firmware target.


## Message Passing
The sole intended form of interprocess communication on the firmware is via message passing. As shown in the example class diagram below, a QueueAggregator class acts as a mediator between the tasks - there is no direct link between actual tasks.

A single header (`QueueTypes` in the diagram) provides the types of each queue on the system. 

```mermaid
classDiagram
    class Task1 {
        -Queue
    }
    class Task2 {
        -Queue
    }
    class QueueAggregator~Queues...~ {
        -Tuple~Queues...~ queues
        +get_queue_idx~QueueType~()
        +ProvideQueue(queue)
        +SendToQueue(message, address)
    }
    class QueueTypes{
        +QueueType1
        +QueueType2
    }

    Task1 ..> QueueAggregator
    Task2 ..> QueueAggregator
    Task1 ..> QueueTypes
    Task2 ..> QueueTypes
    QueueAggregator ..> QueueTypes
```

The recipient of a message may be specified in a few different ways:
- If a message is _only_ valid for one specific task's queue, it can be automatically deduced
- A `Tag` structure defined in each Queue type may be passed as an argument to specifiy which queue to send to
- An enumerated index value (or _address_) exists for each task. Any task is able to check its own address, and this may be attached to a message to specify the _return address_ of the response.
  - Example: `Task2` can receive a specific message from multiple source tasks. When `Task1` sends this message to `Task2`, it attaches its return address. When `Task2` responds, it uses that return address to specify which task to reply to.

### Task Descriptions
Each task on the system is either _periodic_, running at a fixed frequency; or it is _message driven_, running every time there is a new message available and sleeping otherwise. The tasks on the system are defined as follows:

- __Host Comms task__: _(message-driven)_ See `./host_comms_task/`. This task is responsible for parsing incoming USB data and parsing it into messages to dispatch to other tasks on the system, as well as receiving messages from other tasks to serialize into USB messages back to the host. Policy functionality:
  - Read from & write to onboard USB connection. See `./host_comms_task/`
- __System Task__: _(message-driven)_ See `./system/`. This task is responsible for miscellaneous system tasks with low timing requirements. These include updating the User Interface lights and managing the shutdown sequence before jumping to the bootloader. Policy functionality includes:
  - Read from & write to internal flash for updating & reading the device serial number. 
  - Enter the DFU bootloader
  - Write or read an array of I2C bytes (thread-safe via semaphore) for the EEPROM driver
- __UI Task__: _(message-driven)_ See `./ui/`. This task is driven by a timer (__UI Timer__) to periodically update the LED's on the system based on the current thermal system state. Other tasks send their current state to the UI Task, which decides what to display based on the combined state of the main tasks. Policy functionality:
  - Write data to the LED's over I2C
- __Thermistor Task__: _(periodic)_ See `./thermistor/`. The thermistor task manages periodic reading of the thermistors on the system. At the end of each read, the data is sent to the Thermal Task. Policy functinonality includes:
  - Write or read a 16-byte I2C register (thread-safe via semaphore) for the ADS1115 driver
  - Sleep the task until a specific millisecond tick value
- __Thermal Control Task__: _(message-driven)_ See `./thermal_control/`. The thermal task controls the Peltiers and Fans, as well as managing the high level control logic for the thermal elements. Policy functionality:
  - Enable or disable the peltiers
  - Set the direction and pulse width of the peltiers
  - Set the speed of the fans
  - Get the latest tachometer reading from the fans
  - Get the latest current reading from the peltiers

### Task Layout

The tasks on the system are organized in a hierarchial fashion. The class diagram below is used to show the relationships between tasks. An arrow from one task to another means that the first task sends messages to the second task.

- The drivers that each task contains are listed as member variables (e.g. `Peltiers`, `Fans`).
- The messages that a task receives are listed as functions (e.g. `UpdateUI()`). Messages for which an Acknowledge is sent are prefixed with a `+`.

```mermaid
classDiagram
    class HostCommsTask {
        GCode Parser
        IncomingGcode()
        +ForceUSBDisconnectMessage()
    }

    class UITimer

    class UITask {
        LED Driver
        UpdateUI()
        SetCurrentThermalState()
        +Shutdown()
    }

    class SystemTask{
        EnterBootloader()
        +GetDeviceInfo()
        +SetSerial()
    }
    class ThermistorTask{
        ADS1115 Driver
    }
    class ThermalControlTask {
        Thermistor Conversion Driver
        PID Driver
        EEPROM Driver
        ThermistorData()
        +SetTemperature()
        +GetTemperature()
        +GetThermistorData()
        +Disable()
        +GetThermalOffsets()
        +UpdateThermalOffsets()
    }

    HostCommsTask --|> SystemTask : Commands
    HostCommsTask --|> ThermalControlTask : Commands
    ThermistorTask --|> ThermalControlTask : Temperatures
    UITimer --|> UITask : Update Messages
    ThermalControlTask --|> UITask : Errors
    SystemTask --|> HostCommsTask : Shutdown
    SystemTask --|> ThermalControlTask : Shutdown
    SystemTask --|> UITask : Shutdown
```

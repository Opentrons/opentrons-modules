# Host Comms Task

The host comms task control loop runs every time there is a new pending message. The messages are as follows:
- If there is USB data available, try to parse out one or more gcode messages.
  - For every gcode that is read, distribute it to the correct task(s) for handling.
- If there is a ForceUSBDisconnectMessage message, disable USB.
- Any other message is a response to a gcode. The appropriate response will be sent to the USB host.

An interrupt in `freertos_comms_task.cpp` parses any incoming USB data. When a full line of data is received (either a full buffer or a newline), a message is sent to the Host Comms task with the new USB data.

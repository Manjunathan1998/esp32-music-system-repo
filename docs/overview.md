# High-level architecture, purpose, key concepts

The system uses FREE RTOS for managing tasks.

There are following tasks:

- appTask
- serialTask
- uiTask
- playMp3FileTask
- encoderTask

appTask - main task in which all events managing happens.
It uses xQueueReceive() which is a queue pipe from which we receiving commands or "events". Depending on the commands we fire certain logic inside switch-case.

These commands can be sent via the xQueueSend() function from any other tasks.

TIP. If you want to implement a new behaviour, like say Joystick, it can be done in like: joystick

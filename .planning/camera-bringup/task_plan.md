# ESP32-P4 OV2640 camera bring-up

## Goal

Flash and debug the connected ESP32-P4 and OV2640 hardware until SCCB detection, DVP stream startup, and sustained valid JPEG capture are confirmed.

## Success criteria

- Firmware flashes successfully through the detected serial port.
- Boot log identifies the ESP32-P4 and reaches the application.
- OV2640 is detected over SCCB with the expected PID.
- DVP capture starts at 320x240 JPEG.
- At least 90 valid frames are captured without repeated invalid-frame restarts.

## Phases

- [completed] Confirm serial port and flash diagnostic firmware.
- [completed] Capture and classify boot/camera logs.
- [completed] Correct software configuration or implementation issues found on hardware.
- [completed] Correct the physical camera power/control/SCCB wiring using measured voltages and continuity.
- [completed] Rebuild, reflash, and verify sustained JPEG capture after SCCB identification passes.
- [completed] Record the verified wiring and final settings.
- [in_progress] Prepare the ESP32-P4 project repository and migration documentation.
- [pending] Audit non-camera modules against ESP32-P4 GPIO voltage domains and peripheral constraints.
- [pending] Migrate the next STM32 module with a build/flash/runtime verification loop.

## Constraints

- Keep the current OV2640 DVP interface unless evidence shows it cannot work.
- Raw JPEG UART output is enabled only after diagnostic capture is stable and logs are disabled.
- Do not modify the STM32 project.
- Keep an explicit patch record for any local ESP-IDF changes that are not inside the project tree.

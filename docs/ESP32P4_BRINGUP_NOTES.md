# ESP32-P4 Bring-up Notes

This file records verified decisions and mistakes from the ESP32-P4 migration.
Update it whenever a hardware or firmware assumption changes.

The active "do not repeat" checklist is maintained in `docs/KNOWN_ISSUES.md`.

## Verified Camera State

- Board: ESP32-P4, revision v1.0.
- ESP-IDF: `C:\Espressif\frameworks\esp-idf-v5.5.5`.
- Project: `C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view`.
- Serial port: `COM25`.
- Console/JPEG baud rate: `921600`.
- Camera module: ATK-MC2640 / OV2640 DVP module.
- Camera clock: module onboard oscillator. Do not drive XCLK from ESP32-P4.
- Current output: 320x240 JPEG over UART0.
- Current validation: 20 seconds captured 144 complete JPEG frames; saved frame was viewable.

## Do Not Repeat

| Issue | What Happened | Fixed Rule |
|---|---|---|
| Used GPIO0-GPIO15 for 3.3 V camera IO | DNESP32P4M VDDPST1 was measured around 1.2-1.35 V, so SCCB could not work reliably there. | Keep OV2640 SCCB/DVP on verified 3.3 V GPIO domains unless VDDPST1 is deliberately powered at 3.3 V and re-measured. |
| Treated OV2640 SCCB as normal ACK-based I2C | The STM32 code ignores the ninth SCCB bit. ACK scanning was misleading. | Verify OV2640 by reading MID/PID with the STM32-style SCCB sequence. |
| Changed RESET/PWDN polarities experimentally | The ST project already had a verified power/reset sequence. | Preserve the ST sequence: PWDN high 30 ms, PWDN low 80 ms, RESET low 30 ms/high 80 ms plus 150 ms delay, repeated as implemented. |
| Drove XCLK from ESP32-P4 | The ATK-MC2640 module has onboard clock and does not need MCU XCLK. | Keep `CONFIG_EXAMPLE_DVP_XCLK_PIN=-1`. |
| Let IDF DVP JPEG ISR parse zero-byte buffers | ESP-IDF DVP driver could panic on empty JPEG buffers. | Keep the local IDF patch that returns 0 when JPEG buffer is NULL or shorter than 4 bytes. |
| Assumed `uart_write_bytes()` works without driver install | Logs worked, but raw JPEG UART writes failed until the UART driver was installed. | Install UART driver before raw JPEG output. |
| Left logs mixed with raw JPEG | Text logs pollute image stream. | After camera stream starts and UART is initialized, disable logs before sending raw JPEG bytes. |

## Current Camera Bring-up Sequence

1. Run the ST-compatible RESET/PWDN sequence.
2. Run SCCB line-drive test and MID/PID probe.
3. Initialize ESP-Video DVP device.
4. Start V4L2 capture at 320x240 JPEG.
5. Install UART0 driver and switch to raw JPEG output.
6. Send only complete JPEG frames from `FFD8` to `FFD9`.

## Validation Commands

Build:

```powershell
cd C:\Users\heshizhe\Desktop\ESP32P4\p4_xcam_view
. .\scripts\idf-env.ps1
$env:CCACHE_DISABLE='1'
idf.py build
```

Flash:

```powershell
idf.py -p COM25 -b 460800 flash
```

Runtime expectation:

- Before raw mode: boot log reaches `OV2640 stream started`.
- After raw mode: serial data contains repeated complete JPEG frames.
- JPEG frame header should begin with `FF D8 FF E0 00 10 4A 46 49 46`.
- JPEG frame tail should end with `FF D9`.

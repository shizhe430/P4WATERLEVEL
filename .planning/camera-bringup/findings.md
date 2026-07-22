# Findings

## Initial hardware state

- Serial port: COM25.
- USB serial bridge: CH343, VID:PID 1A86:55D3.
- Cursor and project settings both select COM25.
- No competing serial process was detected.
- Diagnostic firmware is configured for ESP32-P4, OV2640 DVP QVGA JPEG, and UART0 at 921600 baud.

## First flash attempt

- The ROM downloader connected successfully and identified ESP32-P4 revision v1.0.
- The original image required chip revision v3.1-v3.99, so esptool rejected it before writing.
- ESP-IDF v5.5.5 explicitly supports pre-v3 P4 chips through `ESP32P4_SELECTS_REV_LESS_V3` and `ESP32P4_REV_MIN_100`.
- Forcing the incompatible v3.1 image is not acceptable because ESP-IDF documents major hardware differences between pre-v3 and v3.x silicon.

## First hardware boot

- The pre-v3 firmware flashed and booted successfully on the ESP32-P4 v1.0.
- OV2640 detection failed on SCCB port 0 with SCL GPIO14 and SDA GPIO15 at 100 kHz; address 0x30 did not acknowledge.
- Both MIPI CSI and DVP were enabled in the active configuration. The shared example selects the MIPI device path first, so the MIPI path must be disabled for isolated DVP bring-up.
- With MIPI disabled, the application uses the DVP device path (`/dev/video2`) correctly, but GPIO14/15 still produces no ACK at OV2640 address 0x30.
- Swapping the SCCB lines to SCL GPIO15/SDA GPIO14 also produced no ACK, ruling out a simple SCL/SDA reversal.
- The development-board I2C header pair SCL GPIO32/SDA GPIO33 also produced no ACK.
- A powered scan on GPIO14/15 reported both bus lines high but all probes timed out instead of returning NACK. This points to an electrically invalid I2C transaction (often pull-up/rise-time or wiring), not merely the wrong sensor address.
- GPIO open-drain SCCB at approximately 25 kHz also received no ACK at address 0x30 and found no device across 0x08-0x77. The failure is independent of the ESP-IDF I2C peripheral transaction engine.
- The diagnostic power sequence reported SCL=1, SDA=1, RESET=1, and PWDN=0 after enabling 20 MHz XCLK. Firmware-side clock and control sequencing are active; the camera side still does not acknowledge.
- Re-reading the verified STM32 implementation found material differences: the module uses its onboard oscillator (no MCU XCLK), PWDN is asserted high for 30 ms then released low for 80 ms, and RESETB is pulsed low 30 ms/high 80 ms with 150 ms post-delay twice. The initial ESP port had not preserved this board-specific sequence.
- The verified STM32 software SCCB implementation deliberately ignores the ninth ACK/don't-care bit and always continues the read transaction. ACK-based I2C probing is therefore not sufficient for this module; PID must be read using the same SCCB behavior.
- The ATK-MC2640 V2.2 schematic confirms that the module has no onboard pull-up resistors on SCL or SDA. The ESP diagnostic enables its internal GPIO pull-ups, matching the STM32 open-drain plus internal-pull-up configuration.
- The module connector is a 2x9 header. Viewed from the lens/silkscreen side with the pins at the bottom, the upper row is `GND, SCL, SDA, D0, D2, D4, D6, PCLK, PWDN`; the lower row is `3.3V, VSYNC, HREF, RST, D1, D3, D5, D7, FLASH`.
- Schematic pin numbers are: 1 GND, 2 3.3V, 3 SCL, 4 VSYNC, 5 SDA, 6 HREF, 7 D0, 8 RESET, 9 D2, 10 D1, 11 D4, 12 D3, 13 D6, 14 D5, 15 PCLK, 16 D7, 17 PWDN, 18 FLASH.
- The current active `sdkconfig` sets DVP XCLK to GPIO20, not the previously recorded disabled value. The module does not expose XCLK and uses its own 24 MHz oscillator, so the final configuration should disable this unused output explicitly.
- Correction: GPIO14-GPIO17 are independent IOs, but GPIO0-GPIO15 belong to the VDDPST1 IO power domain. DNESP32P4M powers VDDPST1 from adjustable VO4, and the user's measured SCL/SDA idle voltage of about 1.2-1.35 V shows this domain is not currently 3.3 V. These pins should not be used for 3.3 V OV2640 SCCB/DVP bring-up unless VO4/VDDPST1 is deliberately reconfigured and verified.
- The rebuilt firmware retained DVP XCLK `-1`, flashed successfully to COM25, and booted on ESP32-P4 v1.0.
- The SCCB electrical self-test passed exactly: released lines `11`, SCL driven low `01`, and SDA driven low `10`. GPIO14/15 are controllable and neither bus line is shorted or stuck.
- The exact STM32-style sequence, including software reset and three full MID/PID reads, returned `MID=0xFFFF PID=0xFFFF` on all attempts. The sensor never drove SDA low during a read, so the remaining fault is on the powered camera/module/wiring side rather than ESP SCCB bit timing.
- A 20 ms input activity test reported only 6 transitions each on PCLK, VSYNC, and HREF. The identical, extremely low counts are consistent with floating/coupled inputs, not an active OV2640 DVP stream. The module is not entering its normal powered-and-released operating state, or the DVP/control wiring does not reach the expected module pins.
- The active wiring and `sdkconfig` have been moved to 3.3 V IO domains: SCL GPIO32, SDA GPIO33, RESET GPIO16, PWDN GPIO17, D0-D5 GPIO18-GPIO23, D6 GPIO26, D7 GPIO27, PCLK GPIO28, VSYNC GPIO29, and HREF/DE GPIO30.
- Final camera validation succeeded on COM25 at 921600 baud. A 20 second binary capture found 144 complete JPEG frames. The saved frame had a valid JFIF header and rendered correctly.
- The working raw output path requires installing the UART driver before calling `uart_write_bytes`; ESP-IDF boot/log output alone does not mean the UART driver API is ready.
- Raw JPEG output must disable logs after stream startup because any text log mixed into the byte stream can break a simple upper-computer JPEG parser.
- The ESP-IDF DVP JPEG ISR needed a local guard for empty JPEG buffers. Without it, zero-byte frames can lead to `Load access fault` in `esp_cam_ctlr_dvp_get_jpeg_size`.
- The OV2640 320x240 JPEG register table was adjusted to match the verified STM32 behavior more closely: `R_DVP_SP=0x02`, `QS=0x03`, and `IMAGE_MODE=IMAGE_MODE_JPEG_EN`.
- Visible project documentation now records the verified camera pin map and lessons learned: `docs/PIN_MAPPING.md` and `docs/ESP32P4_BRINGUP_NOTES.md`.

## Errors encountered

| Error | Attempts | Current resolution |
|---|---:|---|
| OV2640 SCCB no ACK | GPIO14/15, swapped 15/14, GPIO32/33, hardware I2C at 10 kHz, GPIO bit-bang | Physical VCC/GND/SCCB/control-pin verification required |
| `rg.exe` access denied in this desktop session | 1 | Used PowerShell `Select-String` for source inspection |
| PDF path lost Chinese characters through Python stdin | 1 | Passed resolved file paths through environment variables |
| Full single-thread rebuild exceeded the 10-minute command limit | 1 | About 1100 objects completed; continue with the existing incremental build instead of invalidating it again |
| COM25 busy during first flash attempt | 1 | Closed the open SSCOM window, then flashed successfully |
| First serial reset entered ROM download mode and emitted binary data | 1 | Kept DTR/IO0 high and pulsed only RTS/EN for normal application reset |
| `uart_write_bytes()` returned failure for every valid JPEG | 1 | Installed and configured the UART driver before raw JPEG output |
| Raw JPEG stream contained no frames during short capture windows | 2 | Extended capture after confirming valid-frame warmup and then fixed UART driver initialization |
| ESP-IDF DVP driver panicked on zero-byte JPEG buffers | 1 | Added local empty-buffer guard and saved it as a project patch |

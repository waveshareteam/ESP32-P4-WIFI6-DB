# Local Arduino-ESP32 board files

[中文版本](./README_CN.md)

This directory provides the board registration and variant files that let the
`ESP32-P4-WIFI6-DB` be used from the Arduino IDE. It is not a modification of
the official Arduino-ESP32 core sources.

## Versions and board defaults

- Arduino-ESP32: `3.3.11`
- Prebuilt ESP32-P4 package bundled with Arduino-ESP32:
  `tools/esp32-arduino-libs/esp32p4`
- Board entry: `Waveshare ESP32-P4-WIFI6-DB`
- Build target: `esp32p4`
- Flash: `32MB`
- PSRAM: `BOARD_HAS_PSRAM` is defined automatically by the board entry
- Default partition: `default_32MB` (13MB APP, 6.75MB SPIFFS)

`boards.txt` here is the complete Arduino-ESP32 `3.3.11` file, not a small
fragment containing a single board. Back up the original Arduino-ESP32 file
before installing:

1. If your installation has no local board modifications, you can replace the
   file of the same version with the one in this directory.
2. If your installation already contains other boards or local modifications,
   merge only the entries starting with `waveshare_esp32_p4_wifi6_db.` so that
   other definitions are not overwritten.
3. Copy the whole `variants/waveshare_esp32_p4_wifi6_db` directory into the
   `variants` directory of the same Arduino-ESP32 version.
4. Restart the Arduino IDE and select `Waveshare ESP32-P4-WIFI6-DB`.

The `UART0 / Hardware CDC` upload mode from the board menu is recommended.
After uploading, open the serial monitor at `115200` baud. If you use USB CDC,
a different partition scheme or a different upload speed, confirm the matching
hardware connection and the memory requirements of the example.

## Board-specific differences

`pins_arduino.h` follows the `firmware/esp32_p4_wifi6_db` BSP and this
project's schematic:

- I2C1: SDA=`GPIO7`, SCL=`GPIO8`, 400 kHz by default; general I2C and audio
  examples use `Wire1`.
- ES8311 I2S: MCLK/BCLK/WS/DOUT/DIN=`GPIO13/12/10/9/11`, amplifier
  enable=`GPIO53`; the board has one microphone and one speaker.
- SDMMC: CLK/CMD/D0~D3=`GPIO43/44/39/40/41/42`, card power enable=`GPIO45`
  (active low).
- ESP-Hosted SDIO: CLK/CMD/D0~D3/reset=`GPIO18/19/14/15/16/17/54`.
- The MIPI-DSI LCD has no reset GPIO; the backlight controller is at register
  `0x96` of address `0x45` on the shared I2C bus.
- The GT911 reset and interrupt pins are both unconnected, so the 7C mapping of
  GPIO23, GPIO30 and GPIO31 must not be carried over to this board.

The current BSP and the two Arduino display examples default to the 10.1-inch A
JD9365 (800×1280). The board also supports the 8-inch JD9365, the 7-inch
ILI9881C and the 5-inch HX8394. The LCD resolution, panel controller and DSI
rate in the Arduino examples must match the attached panel.

## Relationship with the examples

The general GPIO, I2C, SDMMC and board information examples in
`examples/Arduino` use this variant. `mipi_dsi` and `mipi_csi` still need the
controller parameters selected for the attached LCD panel; `mipi_csi` currently
defaults to the 10.1-inch A JD9365 and must not keep using the 7C
EK79007/1024×600/GPIO30/31 configuration.

I2C initialization also differs per example: `mipi_dsi` uses the legacy I2C
API, while `mipi_csi` builds the new bus through `Wire1` and shares the
`i2c_master` handle. Do not copy the I2C initialization code of one example
into another.

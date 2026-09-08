# ESP32-P4-WIFI6-DB Arduino examples

[中文版本](./README_CN.md)

These examples target the Waveshare `ESP32-P4-WIFI6-DB`. Their source code and
board configuration were checked against Arduino-ESP32 `3.3.11`. The target
chip is the ESP32-P4; the onboard ESP32-C5 wireless coprocessor is attached
through ESP-Hosted over SDIO and is not an Arduino build target.

## Before you start

1. Install Arduino-ESP32 `3.3.11`. Do not mix the board files in this directory
   with a different core version.
2. Copy `esp32/variants/waveshare_esp32_p4_wifi6_db` into the Arduino-ESP32
   `variants` directory.
3. `esp32/boards.txt` is a complete snapshot of the Arduino-ESP32 `3.3.11`
   `boards.txt`. If your installation has no local modifications, you can back
   it up and replace it. If you have already added other boards, merge only the
   `waveshare_esp32_p4_wifi6_db` entries instead of overwriting existing board
   definitions.
4. Restart the Arduino IDE, then select `Waveshare ESP32-P4-WIFI6-DB` and the
   correct serial port.
5. The board entry defaults to 32 MB Flash, PSRAM enabled and the
   `default_32MB` partition scheme. Unless an example README says otherwise,
   use `115200` baud in the serial monitor.
6. For display or camera functionality, first install the libraries listed in
   the corresponding example README and select the panel you actually have
   connected.

## Examples

| Example | Purpose | Documentation |
| --- | --- | --- |
| `board_check` | Print chip, Flash, PSRAM and heap information | [README](./board_check/README.md) |
| `gpio` | Interactive GPIO testing through serial commands | [README](./gpio/README.md) |
| `i2c` | Scan 7-bit addresses on the I2C1 bus | [README](./i2c/README.md) |
| `i2s` | ES8311 48 kHz loopback from one microphone to one speaker | [README](./i2s/README.md) |
| `sdmmc` | microSD file read/write in 4-bit mode | [README](./sdmmc/README.md) |
| `mipi_dsi` | MIPI-DSI LCD color bar and backlight test | [README](./mipi_dsi/README.md) |
| `mipi_csi` | OV5647 camera live preview on the MIPI-DSI LCD | [README](./mipi_csi/README.md) |

## Default LCD configuration

- `mipi_dsi` and `mipi_csi` default to the 10.1-inch A JD9365 at 800×1280,
  RGB565, two DSI lanes, 1500 Mbps/lane.
- `mipi_dsi` selects `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A` in
  `esp_panel_drivers_conf.h`; `mipi_csi` selects `lcd_panel_type_jd9365` in
  `lcd_panel.h` and the panel configuration in `esp_lcd_jd9365.h`.
- For a different LCD, change the configuration as described in that example's
  README. The 10.1-inch B panel is 720×1280 and differs from the default A
  configuration.

## How the I2C bus is used

The shared I2C1 bus uses SDA/SCL on GPIO7/GPIO8 at 400 kHz by default.
Different examples initialize it differently, so they must not run at the same
time or initialize the same bus twice:

- `i2c` and `i2s` use Arduino `Wire1`.
- `mipi_csi` first uses `Wire1`, then obtains the new `i2c_master` bus handle
  and shares it between the ESP_Video camera SCCB and the LCD backlight.
- `mipi_dsi` uses the legacy API from `driver/i2c.h`.

The build and hardware status of the more complex examples still has to be
verified separately on the target board. The versions and parameters in this
directory's READMEs do not imply compatibility with other ESP32-P4 boards or
other Arduino-ESP32 versions.

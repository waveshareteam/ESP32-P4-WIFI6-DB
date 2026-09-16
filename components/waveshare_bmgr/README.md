# ESP32-P4 Board Manager Configuration

[中文](README_zh.md)

This component provides the [ESP Board Manager](https://github.com/espressif/esp-board-manager)
board definition and MIPI-DSI LCD amend configurations for the Waveshare
**ESP32-P4-WIFI6-DB** board. Board metadata, peripheral pins, device
parameters, and LCD initialization data are organized through YAML,
`sdkconfig.defaults.board`, and board-specific factory hooks.

## Contents

### Board definition

| Directory | Description |
| --- | --- |
| [`esp32_p4_wifi6_db`](esp32_p4_wifi6_db/README.md) | ESP32-P4-WIFI6-DB |

A board directory typically contains:

- `board_info.yaml`: board name, chip, and manufacturer information;
- `board_devices.yaml`: LCD, backlight, touch, audio, storage, and camera
  devices;
- `board_peripherals.yaml`: I2C, I2S, GPIO, LDO, and MIPI-DSI settings;
- `setup_device.c`: board-specific LCD and touch factory hooks;
- `lcd_brightness.c`: `custom` device driver for the I2C backlight
  controller;
- `sdkconfig.defaults.board`: board-level default Kconfig settings.

### LCD amends

Each directory under `amends/` describes an optional Waveshare DSI touch
display that can replace the board's default JD9365 10.1" panel. An amend
updates the DSI parameters, LCD DPI timing, touch resolution, and
`waveshare_lcd` factory selection together.

| Amend | Driver | Resolution | DSI lanes | Bitrate |
| --- | --- | ---: | ---: | ---: |
| [`lcd_5_dsi_touch_a`](amends/lcd_5_dsi_touch_a) | HX8394 | 720x1280 | 2 | 700 Mbps |
| [`lcd_7_dsi_touch_a`](amends/lcd_7_dsi_touch_a) | ILI9881C | 720x1280 | 2 | 1000 Mbps |
| [`lcd_8_dsi_touch_a`](amends/lcd_8_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 Mbps |
| [`lcd_10_1_dsi_touch_a`](amends/lcd_10_1_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 Mbps |

These are the same four panel options supported by the
[`waveshare_bsp` variant](../waveshare_bsp/esp32_p4_wifi6_db/README.md) of this board.

## Usage

`idf.py bmgr` runs in the ESP-IDF project directory. This board package is not
on the component registry, so pass `-c` with the path to this directory to make
the board discoverable, then select it:

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr
```

To use a different DSI display than the default 10.1" panel, add `-a` with the
corresponding amend directory:

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr -a <repo>/components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

[`examples/esp_idf/09_board_manager`](../../examples/esp_idf/09_board_manager/README.md)
is a complete project set up this way. See also [`amends/README.md`](amends/README.md)
and the board README at
[`esp32_p4_wifi6_db/README.md`](esp32_p4_wifi6_db/README.md).

## LCD configuration

`Kconfig` exposes the full upstream set of Waveshare panel choices (shared with
other Waveshare ESP32-P4 boards), but only the four panels listed above ship
with an amend directory in this repository. `Kconfig` provides these LCD
options:

- use the selected board's YAML LCD configuration by default;
- select a specific LCD driver and initialization timing through an amend;
- choose RGB565 or RGB888;
- configure 1 to 3 MIPI-DSI DPI frame buffers, with 3 as the default.

`lcd_panel_factory.c` implements the LCD driver factory overrides, while
`include/waveshare_lcd.h` provides the shared includes used for Board Manager
integration.

## Notes

- GPIOs, peripherals, and display timings should be checked against the target
  hardware revision and its Board Manager configuration.
- Apply an amend only to a compatible base board; display resolution, driver,
  and DSI bitrate settings are panel-specific.
- This repository provides configuration and driver integration code. The
  application still needs the required ESP-IDF, Board Manager, and LCD driver
  dependencies.

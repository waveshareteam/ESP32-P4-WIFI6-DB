# ESP32-P4-WIFI6-DB Board Definition

[中文](README_zh.md)

This directory provides the ESP Board Manager definition for the Waveshare
`ESP32-P4-WIFI6-DB`. The board uses the `esp32p4` chip and configuration
version `1.0.0`.

## Default configuration

- LCD: JD9365 MIPI-DSI, 800x1280, RGB565, 2 lanes, 1500 Mbps;
- Touch: GT911, 800x1280 coordinate range, with `0xBA` and `0x28` as candidate
  I2C addresses;
- Audio: ES8311 at I2C address `0x30`, I2S port 0, 48 kHz sample rate;
- Camera: CSI camera resource; `sdkconfig.defaults.board` selects OV5647 at
  RAW8 800x1280 50 fps;
- Storage: 4-bit SDMMC mounted at `/sdcard`, using `SDMMC_FREQ_HIGHSPEED`;
- MIPI power: LDO channel 3 at 2500 mV;
- LCD backlight: `custom` device `lcd_brightness` on the shared I2C bus,
  address `0x45`, register `0x96`, 100 % at start-up
  (`CONFIG_BSP_LCD_BRIGHTNESS_USE_I2C=y`);
- LCD uses 3 DPI frame buffers by default, with amends disabled.

## Peripheral mapping

| Peripheral | Configuration |
| --- | --- |
| I2C | Port 1, SDA GPIO7, SCL GPIO8 |
| I2S | Port 0, MCLK/BCLK/WS/DOUT/DIN on GPIO13/12/10/9/11 |
| Amplifier control | GPIO53 |
| SDMMC | CLK/CMD/D0-D3 on GPIO43/44/39/40/41/42 |
| SD power | GPIO45 |
| MIPI-DSI | Bus 0, 2 lanes, 1500 Mbps |

Not modeled by Board Manager (no corresponding device/peripheral type):
the ESP32-C5 Hosted Wi-Fi SDIO wiring (CLK GPIO18, CMD GPIO19, D0-D3
GPIO14-17, reset GPIO54). An application configures this transport directly
through `esp_hosted` / `esp_wifi_remote`.

## Files

- `board_info.yaml`: board metadata;
- `board_devices.yaml`: audio, LCD, backlight, touch, camera, SD card, and SD
  power-control devices;
- `board_peripherals.yaml`: bus, GPIO, LDO, and MIPI-DSI configuration;
- `setup_device.c`: LCD and GT911 touch factory hooks;
- `lcd_brightness.c` / `lcd_brightness.h`: `custom` device driver for the I2C
  backlight controller, exposing `custom_lcd_brightness_set()`;
- `sdkconfig.defaults.board`: default Flash, PSRAM, LCD, backlight, and OV5647
  settings.

## Usage

Run from the ESP-IDF project directory, pointing `-c` at the
`components/waveshare_bmgr` directory of this repository (the board package is
not on the component registry, so it is not discovered automatically):

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr
```

To use another Waveshare DSI touch display, add `-a` with the corresponding LCD
amend under `amends/`. See [`../amends/README.md`](../amends/README.md) for the
available display profiles, and
[`examples/esp_idf/09_board_manager`](../../../examples/esp_idf/09_board_manager/README.md)
for a complete project.

LCD, touch, and camera parameters should be checked against the actual hardware
revision. This README describes the Board Manager configuration in this
repository and does not replace hardware validation.

<div align="center">
  <h1>ESP32-P4-WIFI6-DB</h1>
  <p><strong>ESP32-P4 development board with an ESP32-C5 Wi-Fi 6 coprocessor and MIPI-DSI / MIPI-CSI interfaces</strong></p>
  <p>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-DB"></a>
  </p>
  <p>
    <a href="README_CN.md">简体中文</a> ·
    <a href="https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm">🛒 Product Page</a> ·
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-DB">📚 Wiki</a> ·
    <a href="components/esp32_p4_wifi6_db/">🔌 BSP</a> ·
    <a href="examples/esp_idf/">🧩 ESP-IDF Examples</a> ·
    <a href="examples/Arduino/">🔧 Arduino Examples</a> ·
    <a href="firmware/">📦 Firmware</a>
  </p>
  <img src="asserts/ESP32-P4-WIFI6-DB.jpg" alt="Waveshare ESP32-P4-WIFI6-DB" width="720">
</div>

---

## ✨ Overview

This repository provides the board support package (BSP), first-party ESP-IDF
and Arduino examples, ESP-Brookesia firmware source, prebuilt factory firmware
images, and the schematic for the Waveshare ESP32-P4-WIFI6-DB development board.

The board pairs the multimedia capabilities of ESP32-P4 with an ESP32-C5
wireless coprocessor connected through ESP-Hosted over SDIO. It exposes
two-lane MIPI-DSI for 5, 7, 8 and 10.1-inch panels, MIPI-CSI for a camera,
an ES8311 audio codec, microSD, USB, and expansion-header GPIO. See the
[product page](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm) for
ordering information.

## 🖥️ Hardware Overview

| Feature | Device / interface |
| --- | --- |
| Main processor | ESP32-P4 (`esp32p4` target) |
| Wireless | ESP32-C5 coprocessor over SDIO using ESP-Hosted, providing Wi-Fi 6 and Bluetooth |
| Display | Two-lane MIPI-DSI; selectable 5-inch HX8394, 7-inch ILI9881C, 8-inch JD9365 or 10.1-inch JD9365 panel |
| Touch | GT911 capacitive controller, polled over the shared I2C bus |
| Camera | MIPI-CSI; camera examples configure OV5647 |
| Audio | ES8311 codec with one analog microphone input and a speaker amplifier |
| Storage and expansion | microSD (4-bit SDMMC), USB Host, SPIFFS, expansion-header GPIO |
| Board support | Local component [`esp32_p4_wifi6_db`](components/esp32_p4_wifi6_db/) 0.0.1 |
| Schematic | [hardware/ESP32-P4-WIFI6-DB.pdf](hardware/ESP32-P4-WIFI6-DB.pdf) |

The full pinout, 40-pin header map and per-GPIO allocation are in the
[hardware reference](docs/en/hw-reference/user-guide-esp32-p4-wifi6-db.md).

### BSP pin assignment

| Resource | BSP configuration |
| --- | --- |
| I2C | SDA GPIO7, SCL GPIO8; shared by ES8311, GT911, LCD backlight and camera SCCB |
| Audio | ES8311 with one analog microphone input; I2S MCLK/BCLK/WS/DOUT/DIN on GPIO13/12/10/9/11; amplifier enable GPIO53 |
| microSD | 4-bit SDMMC; CLK/CMD/D0/D1/D2/D3 on GPIO43/44/39/40/41/42; card power enable GPIO45, active low; SDMMC IO power uses LDO channel 4 |
| Display | Two-lane MIPI-DSI; LCD reset GPIO unused; I2C backlight address `0x45`, brightness register `0x96` |
| Touch | GT911 polling; reset and interrupt GPIOs unused |
| Camera | MIPI-CSI capture; camera examples configure OV5647 |
| Storage / USB | SPIFFS and USB Host support |
| RTC | No external RTC support in the BSP (`BSP_CAPS_RTC=0`) |

See the [BSP README](components/esp32_p4_wifi6_db/README.md) and
[BSP API reference](components/esp32_p4_wifi6_db/API.md) for implementation details.

### ESP-IDF LCD selection

| BSP option | Panel controller | Resolution |
| --- | --- | --- |
| `CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A` | HX8394, 5-inch | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A` | ILI9881C, 7-inch | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A` | JD9365, 8-inch | 800 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A` | JD9365, 10.1-inch | 800 × 1280 |

The [BSP Kconfig](components/esp32_p4_wifi6_db/Kconfig) defaults to **10.1-inch JD9365**.
[Brookesia defaults](firmware/brookesia/sdkconfig.defaults) also select
**10.1-inch JD9365**. Select the attached panel before building. Defaults do not
replace an existing `sdkconfig`; check the project's configuration and generated
`build/config/sdkconfig.h` when determining the effective selection.

## 🚀 Quick Start

Install ESP-IDF (the BSP and Brookesia manifests require **>=5.5**) and open
its configured terminal, then build the first-run board check:

```powershell
cd examples\esp_idf\00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the ESP32-P4 serial port. After the board check succeeds,
choose an example from the tables below. Configure the LCD and any required
Wi-Fi credentials with `idf.py menuconfig` before building; the component
manager resolves dependencies during configuration. The repository ignores
local `build/`, `managed_components/`, `sdkconfig` and `dependencies.lock` files.

The `>=5.5` requirement is a manifest constraint, not a guarantee that every
example works with every newer release. Dependencies can impose additional
constraints.

## 🧪 ESP-IDF Examples

| Example | Purpose |
| --- | --- |
| [00_board_check](examples/esp_idf/00_board_check) | Chip, Flash, PSRAM and heap information, with a periodic heartbeat |
| [01_gpio_input](examples/esp_idf/01_gpio_input) | Display GPIO input states on the LCD |
| [02_i2c_console](examples/esp_idf/02_i2c_console) | Interactive I2C scan and register read/write console |
| [03_sdmmc](examples/esp_idf/03_sdmmc) | microSD file write, read and rename tests |
| [04_mipi_dsi](examples/esp_idf/04_mipi_dsi) | LCD output test; currently displays white, with an inactive color-bar branch |
| [05_mipi_csi](examples/esp_idf/05_mipi_csi) | Camera preview on the LCD using PPA processing |
| [06_wifi](examples/esp_idf/06_wifi) | Wi-Fi station connection and gateway ping |
| [07_i2s_codec](examples/esp_idf/07_i2s_codec) | ES8311 embedded audio playback or microphone loopback |
| [08_usb_host_msc](examples/esp_idf/08_usb_host_msc) | USB mass-storage file operations and sequential read/write speed tests |

See each example's README and configuration for wiring and operation.

## 🧪 Arduino Examples

The local board package is documented for **Arduino-ESP32 3.3.11**. Follow the
[Arduino setup instructions](examples/Arduino/README.md) to install the
`waveshare_esp32_p4_wifi6_db` variant and board entry, then select
`Waveshare ESP32-P4-WIFI6-DB`. The board entry defaults to 32 MB Flash, PSRAM enabled
and the `default_32MB` partition scheme. Install each sketch's required libraries
as described in its README.

| Sketch | Purpose |
| --- | --- |
| [board_check](examples/Arduino/board_check) | Chip and memory information |
| [gpio](examples/Arduino/gpio) | Interactive GPIO testing over serial |
| [i2c](examples/Arduino/i2c) | I2C1 address scan |
| [i2s](examples/Arduino/i2s) | ES8311 48 kHz mono microphone-to-speaker loopback |
| [sdmmc](examples/Arduino/sdmmc) | 4-bit microSD file read/write |
| [mipi_dsi](examples/Arduino/mipi_dsi) | LCD color bars and backlight testing |
| [mipi_csi](examples/Arduino/mipi_csi) | OV5647 camera preview on the LCD |

Arduino `mipi_dsi` and `mipi_csi` default to **10.1-inch A JD9365**, 800 × 1280,
RGB565, two DSI lanes at 1500 Mbps/lane. `mipi_dsi` selects the panel in
`esp_panel_drivers_conf.h`; `mipi_csi` selects JD9365 in `lcd_panel.h` and the
10.1-inch A profile in `esp_lcd_jd9365.h`. These settings are separate from the
ESP-IDF BSP Kconfig.

## 📡 ESP32-P4 and ESP32-C5 Wireless

ESP32-P4 does not contain an integrated radio. Wi-Fi and Bluetooth are provided
by the ESP32-C5 coprocessor over SDIO using ESP-Hosted. ESP32-C5 requires
compatible ESP-Hosted slave firmware, and flashing an ESP32-P4 example does not
update the C5. Keep the slave firmware compatible with the `esp_hosted` and
`esp_wifi_remote` versions declared by each wireless project before changing or
reflashing the coprocessor.

## 📦 Firmware

The [`firmware/`](firmware/) directory keeps two firmware surfaces:

- [`firmware/brookesia/`](firmware/brookesia/README.md) is the ESP-Brookesia
  board integration application: an integrated GUI with calculator, drawing,
  audio, camera, video, settings, Xiaozhi and GPIO applications.
- [`firmware/factory-firmware/`](firmware/factory-firmware/) holds prebuilt
  ESP32-P4 images for the 5, 7, 8 and 10.1-inch panels and an ESP32-C5 slave
  image.

To build Brookesia:

```powershell
cd firmware\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

The bundled factory images are separate artifacts; their presence does not
establish compatibility with freshly resolved host dependencies.

## 🗂️ Repository Layout

| Path | Purpose |
| --- | --- |
| [`components/esp32_p4_wifi6_db/`](components/esp32_p4_wifi6_db/) | ESP-IDF BSP, headers, Kconfig and component manifest |
| [`examples/esp_idf/`](examples/esp_idf/) | First-party ESP-IDF peripheral examples |
| [`examples/Arduino/`](examples/Arduino/README.md) | Arduino sketches and local board definitions |
| [`firmware/brookesia/`](firmware/brookesia/README.md) | ESP-Brookesia board integration application |
| [`firmware/factory-firmware/`](firmware/factory-firmware/) | Prebuilt ESP32-P4 and ESP32-C5 images |
| [`hardware/`](hardware/) | Board schematic |
| [`docs/`](docs/) | Hardware reference (pinout, GPIO allocation, usage notes) |
| [`asserts/`](asserts/) | Product images used by repository documentation |

## 📚 Documentation

- [Product Page](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)
- [Official Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)
- [Hardware Reference](docs/en/hw-reference/user-guide-esp32-p4-wifi6-db.md)
- [BSP README](components/esp32_p4_wifi6_db/README.md)
- [BSP API Reference](components/esp32_p4_wifi6_db/API.md)
- [BSP Changelog](components/esp32_p4_wifi6_db/CHANGELOG.md)
- [Arduino Examples](examples/Arduino/README.md)
- [Brookesia Firmware](firmware/brookesia/README.md)
- [Schematic](hardware/ESP32-P4-WIFI6-DB.pdf)

## ✅ Validation Scope

This overview reflects repository source and default configuration. It does not
certify a successful build or full peripheral validation of the current revision.
Build, flash and runtime results must be checked separately for the selected
example, framework version, panel and board.

## 🤝 Support and Contributions

Contributions and reproducible issue reports are welcome. Include the board
version, attached panel, example path, ESP-IDF or Arduino-ESP32 version,
reproduction steps, expected behavior, actual behavior, and relevant serial logs.

- [Open an Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/issues/new)

## 📄 License

This repository is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).

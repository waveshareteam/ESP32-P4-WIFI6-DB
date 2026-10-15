<div align="center">
  <h1>ESP32-P4-WIFI6-DB</h1>
  <p><strong>ESP32-P4 development board with an ESP32-C5 Wi-Fi 6 coprocessor and MIPI-DSI / MIPI-CSI interfaces</strong></p>
  <p>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-DB"></a>
  </p>
  <p>
    <a href="README_CN.md">简体中文</a> ·
    <a href="https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm">🌐 Product Page</a> ·
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-DB">📚 Wiki</a> ·
    <a href="firmware/">📦 Firmware</a> ·
    <a href="examples/esp_idf/">🧩 ESP-IDF Examples</a> ·
    <a href="examples/Arduino/">🔧 Arduino Examples</a>
  </p>
  <img src="asserts/ESP32-P4-WIFI6-DB.jpg" alt="Waveshare ESP32-P4-WIFI6-DB" width="720">
</div>

---

## ✨ Overview

This repository provides the ESP-IDF BSP, ESP-IDF and Arduino examples, ESP-Brookesia firmware source, factory firmware, and schematic for the Waveshare ESP32-P4-WIFI6-DB development board.

The board uses ESP32-P4 as its main processor and connects an ESP32-C5 wireless coprocessor through ESP-Hosted over SDIO. It provides MIPI-DSI display, MIPI-CSI camera, ES8311 audio, microSD, USB Host, SPIFFS and expansion-header GPIO. The display and camera are external modules; see the [product page](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm) for ordering information.

## 🖥️ Hardware Overview

| Feature | Device / interface |
| --- | --- |
| Main processor | ESP32-P4, dual-core RISC-V HP system up to 360 MHz, with a low-power core |
| Wireless | ESP32-C5 coprocessor over SDIO using ESP-Hosted, providing Wi-Fi 6 and Bluetooth |
| Storage | 32 MB Flash; PSRAM enabled in the Arduino board definition |
| Display | Two-lane MIPI-DSI; supports external Waveshare 5, 7, 8 and 10.1-inch panels |
| Touch | GT911 capacitive touch controller, polled over the shared I2C bus |
| Camera | MIPI-CSI; camera examples configure OV5647 |
| Audio | ES8311 codec, one analog microphone input and a speaker amplifier |
| Storage and expansion | microSD (4-bit SDMMC), USB Host, SPIFFS and expansion-header GPIO |
| Board support | Local ESP-IDF BSP component `esp32_p4_wifi6_db`, version `0.0.1` |
| Schematic | [hardware/ESP32-P4-WIFI6-DB.pdf](hardware/ESP32-P4-WIFI6-DB.pdf) |

### ESP-IDF LCD Selection

| Panel | Controller | Resolution |
| --- | --- | --- |
| [Waveshare 5-DSI-TOUCH-A](https://www.waveshare.net/shop/5-DSI-TOUCH-A.htm) | HX8394, 5-inch | 720 × 1280 |
| [Waveshare 7-DSI-TOUCH-A](https://www.waveshare.net/shop/7-DSI-TOUCH-A.htm) | ILI9881C, 7-inch | 720 × 1280 |
| [Waveshare 8-DSI-TOUCH-A](https://www.waveshare.net/shop/8-DSI-TOUCH-A.htm) | JD9365, 8-inch | 800 × 1280 |
| [Waveshare 10.1-DSI-TOUCH-A](https://www.waveshare.net/shop/10.1-DSI-TOUCH-A.htm) | JD9365, 10.1-inch | 800 × 1280 |

Before building, make sure the selected panel matches the connected display.

## 🚀 Quick Start

Install ESP-IDF (the BSP manifest requires **>=5.5**) and open its configured terminal, then build the board check example:

```powershell
cd examples\esp_idf\00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the board's serial port. After the board check succeeds, choose an example from the table below; configure the LCD profile to match the connected panel before building display examples. See the [Arduino setup instructions](examples/Arduino/README.md) to install the Arduino board package.

The `>=5.5` requirement is a manifest constraint, not a guarantee that every example works with every newer release. Dependencies can impose additional constraints.

## 🧪 ESP-IDF Examples

| Example | Purpose |
| --- | --- |
| [00_board_check](examples/esp_idf/00_board_check) | Chip, Flash, PSRAM and serial-port information |
| [01_gpio_input](examples/esp_idf/01_gpio_input) | Display GPIO input states on the LCD |
| [02_i2c_console](examples/esp_idf/02_i2c_console) | Shared I2C bus scan and interactive diagnostics |
| [03_sdmmc](examples/esp_idf/03_sdmmc) | microSD 4-bit SDMMC read/write test |
| [04_mipi_dsi](examples/esp_idf/04_mipi_dsi) | MIPI-DSI LCD output and backlight test |
| [05_mipi_csi](examples/esp_idf/05_mipi_csi) | Camera preview on the LCD |
| [06_wifi](examples/esp_idf/06_wifi) | Wi-Fi connection through ESP32-C5 / ESP-Hosted |
| [07_i2s_codec](examples/esp_idf/07_i2s_codec) | ES8311 audio playback, capture and loopback |
| [08_usb_host_msc](examples/esp_idf/08_usb_host_msc) | USB Host mass-storage test |
| [09_board_manager](examples/esp_idf/09_board_manager) | ESP Board Manager bring-up console and test cases |

See each example's README and configuration for wiring and operation.

## 🧪 Arduino Examples

The local board package is documented for Arduino-ESP32 **3.3.11**. Follow the [Arduino setup instructions](examples/Arduino/README.md) to install the variant and board entry, then select `Waveshare ESP32-P4-WIFI6-DB`. The board defaults to 32 MB Flash and the `default_32MB` partition scheme. See each example's README for its configuration.

| Sketch | Purpose |
| --- | --- |
| [board_check](examples/Arduino/board_check) | Chip and memory information |
| [gpio](examples/Arduino/gpio) | Interactive GPIO testing over serial |
| [i2c](examples/Arduino/i2c) | I2C1 address scan |
| [i2s](examples/Arduino/i2s) | ES8311 single-microphone to single-speaker audio loopback |
| [sdmmc](examples/Arduino/sdmmc) | 4-bit microSD file read/write |
| [mipi_dsi](examples/Arduino/mipi_dsi) | LCD color bars and backlight testing |
| [mipi_csi](examples/Arduino/mipi_csi) | OV5647 camera preview on the LCD |

Arduino `mipi_dsi` and `mipi_csi` default to the 10.1-inch JD9365 panel, 800 × 1280, RGB565 and two DSI lanes. For other LCDs, select the option that matches the connected panel in the example configuration. I2C1 is shared by multiple peripherals, so initialize it once per program.

## 📡 ESP32-P4 and ESP32-C5 Wireless

ESP32-P4 does not contain an integrated radio. Wi-Fi and Bluetooth are provided by the ESP32-C5 coprocessor over SDIO using ESP-Hosted. ESP32-C5 requires compatible slave firmware; flashing an ESP32-P4 example does not update the C5. Before changing or reflashing the coprocessor, check that its firmware is compatible with the ESP-Hosted and remote Wi-Fi component versions used by the P4 project.

## 📦 Firmware

[`firmware/brookesia/`](firmware/brookesia/) contains the ESP-Brookesia application firmware source, including the GUI, settings, music player, video player, camera and other application components.

[`firmware/factory-firmware/`](firmware/factory-firmware/) provides prebuilt firmware for the ESP32-P4 main processor and ESP32-C5 wireless coprocessor. They target different chips and use different flashing addresses; check the corresponding instructions and firmware versions before flashing. Their presence does not establish compatibility with freshly resolved dependencies.

## 🗂️ Repository Layout

| Path | Purpose |
| --- | --- |
| [`components/waveshare_bsp/esp32_p4_wifi6_db/`](components/waveshare_bsp/esp32_p4_wifi6_db/) | ESP-IDF BSP, headers, Kconfig and component manifest |
| [`components/waveshare_bmgr/`](components/waveshare_bmgr/) | ESP Board Manager board definition and LCD amendments |
| [`examples/esp_idf/`](examples/esp_idf/) | ESP-IDF peripheral and feature examples |
| [`examples/Arduino/`](examples/Arduino/) | Arduino examples and local board definitions |
| [`firmware/brookesia/`](firmware/brookesia/) | ESP-Brookesia application firmware source |
| [`firmware/factory-firmware/`](firmware/factory-firmware/) | Prebuilt ESP32-P4 and ESP32-C5 firmware |
| [`hardware/`](hardware/) | Board schematic |
| [`docs/`](docs/) | Hardware reference and project documentation |
| [`asserts/`](asserts/) | Product images used by repository documentation |

## 📚 Documentation

- [Product Page](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)
- [Official Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)
- [BSP README](components/waveshare_bsp/esp32_p4_wifi6_db/README.md)
- [BSP API Reference](components/waveshare_bsp/esp32_p4_wifi6_db/API.md)
- [Arduino Examples](examples/Arduino/README.md)
- [Brookesia Firmware](firmware/brookesia/README.md)
- [Schematic](hardware/ESP32-P4-WIFI6-DB.pdf)

## 🤝 Support and Contributions

Contributions and reproducible issue reports are welcome. Include the board version, attached panel, example path, ESP-IDF or Arduino-ESP32 version, reproduction steps, expected behavior, actual behavior and relevant serial logs. Do not include logs containing Wi-Fi passwords or API keys.

- [Open an Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/issues/new)

## 📄 License

This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE). Some components and example files have separate license or copyright notices; follow those notices when using or redistributing those files.

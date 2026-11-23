<div align="center">
  <h1>ESP32-P4-WIFI6-DB</h1>
  <p><strong>ESP32-P4 development board with an ESP32-C5 Wi-Fi 6 coprocessor and MIPI-DSI / MIPI-CSI interfaces</strong></p>
  <p><strong>English</strong> | <a href="README_ZH.md">简体中文</a></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-P4-WIFI6-DB"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-DB"></a>
  </p>
  <p>
    <a href="https://www.waveshare.com/esp32-p4-wifi6-db.htm">🛒 Product</a> &middot;
    <a href="firmware/">📦 Firmware</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/arduino/examples/">🔧 Arduino</a> &middot;
    <a href="https://www.waveshare.com/wiki/ESP32-P4-WIFI6-DB">📖 Documentation</a>
  </p>
  <p><img src="docs/images/ESP32-P4-WIFI6-DB.jpg" alt="ESP32-P4-WIFI6-DB development board" width="500"></p>
</div>

---

## ✨ Overview

**ESP32-P4-WIFI6-DB** is a development board built around the ESP32-P4 processor and an ESP32-C5 Wi-Fi 6 / Bluetooth LE wireless coprocessor over SDIO. It integrates 2-lane MIPI-DSI display and GT911 touch, MIPI-CSI camera, ES8311 audio, microSD, USB Host, and expansion GPIO.

This repository provides ESP-IDF examples, Arduino examples with a local board definition, the ESP-Brookesia factory firmware source, prebuilt factory binaries, and hardware schematics. Run [00_board_check](examples/esp-idf/00_board_check/) first to verify building, flashing, and serial monitoring.

| Feature | Device / interface |
| --- | --- |
| MCU | ESP32-P4 (dual-core RISC-V HP + LP core) |
| Wireless | ESP32-C5 wireless coprocessor (Wi-Fi 6 & BLE, 4-bit SDIO) |
| Memory | On-board 32 MB Flash and 32 MB PSRAM |
| Display | 2-lane MIPI-DSI interface with I2C backlight control (supports [5-DSI-TOUCH-A](https://www.waveshare.com/5-DSI-TOUCH-A.htm), [7-DSI-TOUCH-A](https://www.waveshare.com/7-DSI-TOUCH-A.htm), [8-DSI-TOUCH-A](https://www.waveshare.com/8-DSI-TOUCH-A.htm), [10.1-DSI-TOUCH-A](https://www.waveshare.com/10.1-DSI-TOUCH-A.htm)) |
| Touch | GT911 capacitive touch (I2C interface) |
| Camera | On-board MIPI-CSI interface supporting OV5647 ([RPi Camera (B)](https://www.waveshare.com/rpi-camera-b.htm)) camera module |
| Audio | ES8311 audio codec (I2C + I2S) with speaker amplifier and analog microphone input |
| Storage | On-board microSD slot over 4-bit SDMMC |
| USB | USB Host (USB-C OTG) |
| Expansion | Expansion GPIO header |

Before building display examples, make sure the selected panel matches the connected display (configured under `Board Support Package` in `idf.py menuconfig` for ESP-IDF, or in the sketch for Arduino).

### ESP32-P4 and ESP32-C5 Wireless

ESP32-P4 does not contain an integrated radio. Wi-Fi 6 and Bluetooth are provided by the onboard ESP32-C5 coprocessor over SDIO using ESP-Hosted. The board ships with compatible slave firmware pre-flashed on the ESP32-C5, and flashing an ESP32-P4 example does not overwrite it. A prebuilt slave binary is included at [`firmware/ESP32-C5/esp32-c5-slave-260623.bin`](firmware/ESP32-C5/esp32-c5-slave-260623.bin); if you update the ESP-Hosted component in your P4 project, build a matching slave firmware from the official [espressif/esp-hosted-mcu](https://github.com/espressif/esp-hosted-mcu) repository.

## 📦 Quick Start

The fastest way to try an example is a ready-to-flash package from the
[latest release](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/releases/latest).

1. Download the `*-combined.zip` package for the example and framework version you need.
2. Extract the archive and install esptool with `python -m pip install esptool`.
3. Connect the board over USB.
4. Run `flash_combined.bat COMx` on Windows or `./flash_combined.sh /dev/ttyACM0` on Linux.
5. Reset the board if it does not restart automatically.

Factory firmware (restores the out-of-box demo): download the matching panel file whose name contains `Factory` from the [`firmware`](firmware/) folder.

## 🧪 Examples

### ESP-IDF

| Example | Main feature |
| --- | --- |
| [00_board_check](examples/esp-idf/00_board_check/) | Chip, Flash, PSRAM, and serial-port check |
| [01_gpio_input](examples/esp-idf/01_gpio_input/) | Display GPIO input states on the LCD |
| [02_i2c_console](examples/esp-idf/02_i2c_console/) | Shared I2C bus scan and interactive diagnostics |
| [03_sdmmc](examples/esp-idf/03_sdmmc/) | microSD 4-bit SDMMC read/write test |
| [04_mipi_dsi](examples/esp-idf/04_mipi_dsi/) | MIPI-DSI LCD output and backlight test |
| [05_mipi_csi](examples/esp-idf/05_mipi_csi/) | OV5647 MIPI-CSI camera preview on the LCD |
| [06_wifi](examples/esp-idf/06_wifi/) | Wi-Fi connection through ESP32-C5 / ESP-Hosted |
| [07_i2s_codec](examples/esp-idf/07_i2s_codec/) | ES8311 audio playback, capture, and loopback |
| [08_usb_host_msc](examples/esp-idf/08_usb_host_msc/) | USB Host mass-storage test |
| [09_board_manager](examples/esp-idf/09_board_manager/) | ESP Board Manager bring-up console and test cases |

### Arduino

| Example | Main feature |
| --- | --- |
| [board_check](examples/arduino/examples/board_check/) | Chip, Flash, PSRAM, and serial-port check |
| [gpio](examples/arduino/examples/gpio/) | Expansion-header GPIO input monitor |
| [i2c](examples/arduino/examples/i2c/) | Shared I2C bus scan |
| [i2s](examples/arduino/examples/i2s/) | ES8311 audio playback and microphone test |
| [mipi_csi](examples/arduino/examples/mipi_csi/) | OV5647 MIPI-CSI camera preview on the LCD |
| [mipi_dsi](examples/arduino/examples/mipi_dsi/) | MIPI-DSI LCD and touch test |
| [sdmmc](examples/arduino/examples/sdmmc/) | microSD 4-bit SDMMC read/write test |

Board files for Arduino-ESP32 `3.3.11` are provided in [`examples/arduino/esp32`](examples/arduino/esp32/).

## 🛠️ Development Environment

| Framework | Version | Examples |
| --- | --- | ---: |
| ESP-IDF | `v5.5.6`, `v6.1.0` | 10 |
| Arduino-ESP32 | `3.3.11` | 7 |

## 📚 Resources

- [Schematic](hardware/schematic/)
- [Product page](https://www.waveshare.com/esp32-p4-wifi6-db.htm)
- [Wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-DB)

## 🤝 Support

- Example or repository problem: [open an issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/issues/new/choose).
- Product usage, hardware or order questions: [Waveshare support](https://service.waveshare.com/).

## 📄 License

Apache License 2.0. See [LICENSE](LICENSE).

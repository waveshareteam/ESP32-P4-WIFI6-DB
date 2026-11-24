# i2c

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Scans the shared I2C bus (`Wire1`) and prints all detected 7-bit device addresses.

## Hardware Required

- ESP32-P4-WIFI6-DB development board

## How to Run

1. Install the `esp32` board package (Arduino-ESP32 `3.3.11`) and apply the board files in `examples/arduino/esp32/` if needed.
2. Select the board and the **Tools** menu options that match `.github/product.json`:

   | Tools menu | Value |
   | --- | --- |
   | Board | ESP32P4 Dev Module (or Waveshare ESP32-P4-WIFI6-DB) |
   | Flash Size | 32MB (256Mb) |
   | PSRAM | Enabled |

3. Open the `.ino` file and upload.

## Expected Result

Serial monitor lists detected I2C addresses on `Wire1`.

## Troubleshooting

None yet.

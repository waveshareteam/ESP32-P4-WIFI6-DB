# mipi_dsi

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the MIPI-DSI display and GT911 touch controller using `ESP32_Display_Panel`.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Waveshare MIPI-DSI touch display (5, 7, 8, or 10.1-inch)

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

The display renders test graphics and responds to touch coordinates.

## Troubleshooting

Install `ESP32_Display_Panel`, `ESP32_IO_Expander`, and `esp-lib-utils`, and select the matching panel type in the sketch.

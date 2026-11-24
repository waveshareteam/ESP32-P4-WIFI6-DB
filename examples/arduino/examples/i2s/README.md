# i2s

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the ES8311 audio codec over `Wire1` and `I2S` for audio playback and microphone testing.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Speaker connected to the onboard speaker header

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

The speaker plays test audio and serial monitor logs codec status.

## Troubleshooting

None yet.

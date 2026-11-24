# 07_i2s_codec

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Demonstrates ES8311 audio codec playback, analog microphone capture, and audio loopback over I2S.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Speaker connected to the onboard speaker header

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

The speaker plays audio output and logs codec initialization status over the serial monitor.

## Troubleshooting

None yet.

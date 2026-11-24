# Factory Firmware (ESP-Brookesia)

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the ESP-Brookesia launcher with Drawpanel, SpecAnalyzer, MusicPlayer, Camera, VideoPlayer, Settings, and GpioMonitor.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Waveshare MIPI-DSI touch display (5, 7, 8, or 10.1-inch)

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

The display boots into the ESP-Brookesia home screen with interactive board-test applications.

## Troubleshooting

Select the matching panel type (`CONFIG_BSP_LCD_TYPE_*`) in `idf.py menuconfig` before building.

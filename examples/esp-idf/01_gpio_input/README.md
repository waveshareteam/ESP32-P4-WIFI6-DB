# 01_gpio_input

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Reads expansion-header GPIO input levels and displays their real-time states on the MIPI-DSI LCD using LVGL.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Waveshare MIPI-DSI touch display (5, 7, 8, or 10.1-inch)

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

The LCD renders a GPIO status grid that updates when expansion pins are pulled high or low.

## Troubleshooting

Select the matching panel model in `idf.py menuconfig` under `Board Support Package` before building.

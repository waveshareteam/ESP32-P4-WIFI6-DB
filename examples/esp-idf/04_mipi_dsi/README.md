# 04_mipi_dsi

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the MIPI-DSI LCD panel and backlight, then renders test graphics with LVGL.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Waveshare MIPI-DSI touch display (5, 7, 8, or 10.1-inch)

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

The display turns on its backlight and renders the LVGL test pattern.

## Troubleshooting

Select the matching panel model in `idf.py menuconfig` under `Board Support Package` before building.

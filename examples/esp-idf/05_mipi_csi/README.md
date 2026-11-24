# 05_mipi_csi

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Captures video from an OV5647 MIPI-CSI camera and previews the live stream on the MIPI-DSI LCD.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- OV5647 MIPI-CSI camera module and Waveshare MIPI-DSI display

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Live camera video appears on the MIPI-DSI screen.

## Troubleshooting

Confirm the FPC cable orientation for both camera and display before powering on.

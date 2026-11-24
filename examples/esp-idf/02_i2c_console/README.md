# 02_i2c_console

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Provides an interactive serial console to scan the shared I2C bus and probe connected devices.

## Hardware Required

- ESP32-P4-WIFI6-DB development board

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial console lists detected I2C device addresses (such as ES8311 and GT911 when a touch panel is attached).

## Troubleshooting

None yet.

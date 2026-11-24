# 00_board_check

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Prints chip model, Flash size, PSRAM size, and serial-port information to verify the basic bring-up environment.

## Hardware Required

- ESP32-P4-WIFI6-DB development board

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial monitor outputs chip revision, CPU frequency, 32 MB Flash, and PSRAM size.

## Troubleshooting

None yet.

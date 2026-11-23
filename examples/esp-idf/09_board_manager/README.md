# 09_board_manager

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Demonstrates ESP Board Manager initialization and peripheral tests using the managed `waveshare/waveshare_boards` package.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- Optional Waveshare MIPI-DSI display, microSD card, or OV5647 camera depending on selected test cases

## How to Run

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_db
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial console initializes board peripherals and runs the selected Board Manager test cases.

## Troubleshooting

Run `idf.py bmgr -b esp32_p4_wifi6_db` before `idf.py build` so `gen_bmgr_codes` is generated.

# 06_wifi

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Connects to a Wi-Fi access point through the onboard ESP32-C5 coprocessor using ESP-Hosted over SDIO.

## Hardware Required

- ESP32-P4-WIFI6-DB development board

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial monitor logs ESP-Hosted initialization, Wi-Fi association, and the assigned IP address.

## Troubleshooting

Configure the target Wi-Fi SSID and password in `idf.py menuconfig` under `Example Configuration`, and ensure the ESP32-C5 slave runs matching ESP-Hosted firmware.

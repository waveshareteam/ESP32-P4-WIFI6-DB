# 08_usb_host_msc

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Acts as a USB Host to mount a connected USB flash drive and perform file read/write tests.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- USB flash drive and USB-C OTG adapter

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial monitor detects the attached MSC device, mounts `/usb`, and reads/writes a test file.

## Troubleshooting

Connect the USB flash drive to the USB-C OTG port rather than the UART debug port.

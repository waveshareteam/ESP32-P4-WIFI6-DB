# 03_sdmmc

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Mounts a microSD card over 4-bit SDMMC and runs file read/write verification.

## Hardware Required

- ESP32-P4-WIFI6-DB development board
- FAT32-formatted microSD card

## How to Run

```bash
idf.py set-target esp32p4 build flash monitor
```

## Expected Result

Serial monitor prints card info, creates a test file, reads back its contents, and unmounts the card.

## Troubleshooting

Ensure the microSD card is inserted before resetting the board.

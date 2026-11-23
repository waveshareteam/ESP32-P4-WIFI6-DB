# sdmmc

[中文版本](./README_CN.md)

## Purpose

Initializes the onboard microSD card over the 4-bit SDMMC interface, prints the
card and filesystem capacity, and writes and reads back a test file. The sketch
turns on card power by itself; no extra GPIO handling is required.

## Usage

1. Insert a microSD card into the board.
2. Select `Waveshare ESP32-P4-WIFI6-DB` in the Arduino IDE.
3. Open `sdmmc.ino`, compile and upload.
4. Open the serial monitor at `115200` baud.
5. Read the card information and the write/read result for
   `/arduino_test.txt`.

## Board wiring

- SDMMC CLK/CMD: GPIO43/GPIO44.
- SDMMC D0-D3: GPIO39/GPIO40/GPIO41/GPIO42.
- Card power control: GPIO45, active low.

## Expected output

```text
Starting SDMMC test
SD card detected: ...
Card size: ... MB
Filesystem total: ... MB
Writing /arduino_test.txt
Reading /arduino_test.txt
ESP32-P4 SDMMC test
SDMMC test completed successfully
```

If mounting fails, first confirm that the card is inserted and formatted in a
recognizable filesystem, then check whether another example is holding the
SDMMC pins. The test file is created in the card's root directory and can be
deleted manually afterwards.

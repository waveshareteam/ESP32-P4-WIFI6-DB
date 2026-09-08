# board_check

[中文版本](./README_CN.md)

## Purpose

Reads and prints the board's chip model, chip revision, CPU information, Flash
size, PSRAM size and free heap. The sketch does not touch the LCD, camera,
audio, SDMMC or ESP-Hosted, so no additional peripherals need to be connected.

## Usage

1. Select `Waveshare ESP32-P4-WIFI6-DB` in the Arduino IDE.
2. Open `board_check.ino`, compile and upload.
3. Open the serial monitor at `115200` baud.
4. Read the board information output.

## Expected output

The sketch prints a block like the following once at startup. The actual CPU
frequency and memory values depend on the board and on the Arduino board menu
configuration:

```text
Waveshare ESP32-P4-WIFI6-DB
Chip model: ESP32-P4
Chip revision: ...
CPU cores: ...
CPU frequency: ... MHz
Flash size: ... MB
PSRAM size: ... MB
Free heap: ... bytes
```

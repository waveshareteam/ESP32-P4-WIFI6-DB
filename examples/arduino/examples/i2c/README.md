# i2c

[中文版本](./README_CN.md)

## Purpose

Scans the board's default I2C1 bus and prints the 7-bit I2C addresses that
respond. This board uses SDA GPIO7 and SCL GPIO8 at 400 kHz by default. The
scanner only lists addresses that return an ACK; finding no device does not by
itself indicate a fault.

## Usage

1. Connect the I2C devices you want to detect to the board's I2C bus.
2. Select `Waveshare ESP32-P4-WIFI6-DB` in the Arduino IDE.
3. Open `i2c.ino`, compile and upload.
4. Open the serial monitor at `115200` baud.
5. Read the scan result; the sketch rescans every 5 seconds.

## Notes

- This bus is shared with onboard devices such as the ES8311, the LCD
  backlight, the GT911 and the camera SCCB. Do not run another example that
  also initializes I2C1 at the same time.
- The printed addresses are the 7-bit addresses used by Arduino `Wire1`, not
  the 8-bit control bytes that include the read/write bit.

## Expected output

```text
Scanning I2C bus...
Device found at 0x...
Scan complete: ... device(s) found
```

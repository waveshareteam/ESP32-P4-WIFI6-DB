# ESP32-P4-WIFI6-DB Firmware

[English](README.md) | [简体中文](README_ZH.md)

Select the ESP32-P4 factory firmware for the connected display. Each file is a merged image flashed at `0x0`; the date in its name identifies the version.

| File | Purpose |
| --- | --- |
| [ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin) | Factory firmware for the 10.1-DSI-TOUCH-A display |
| [ESP32-P4-WIFI6-DB-5-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-5-DSI-TOUCH-A_Factory_20260804.bin) | Factory firmware for the 5-DSI-TOUCH-A display |
| [ESP32-P4-WIFI6-DB-7-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-7-DSI-TOUCH-A_Factory_20260804.bin) | Factory firmware for the 7-DSI-TOUCH-A display |
| [ESP32-P4-WIFI6-DB-8-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-8-DSI-TOUCH-A_Factory_20260804.bin) | Factory firmware for the 8-DSI-TOUCH-A display |

## Flashing

Run the commands from this directory. The example uses the 10.1-DSI-TOUCH-A image. Replace `<PORT>` with the ESP32-P4 serial port and select the file listed for your display. Erasing removes the existing data from ESP32-P4 flash.

```bash
python -m pip install esptool
python -m esptool --chip esp32p4 --port <PORT> erase-flash
python -m esptool --chip esp32p4 --port <PORT> write-flash 0x0 ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin
```

The ESP32-C5 wireless coprocessor firmware is available as [esp32-c5-slave-260623.bin](ESP32-C5/esp32-c5-slave-260623.bin). See [examples/factory/README.md](../examples/factory/README.md) for the ESP-Brookesia factory firmware source.

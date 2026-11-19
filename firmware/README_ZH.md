# ESP32-P4-WIFI6-DB 固件

[English](README.md) | [简体中文](README_ZH.md)

按连接的屏幕选择对应的 ESP32-P4 出厂固件。以下文件均为从 `0x0` 烧录的合并镜像，文件名中的日期表示版本。

| 文件 | 用途 |
| --- | --- |
| [ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin) | 10.1-DSI-TOUCH-A 屏幕出厂固件 |
| [ESP32-P4-WIFI6-DB-5-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-5-DSI-TOUCH-A_Factory_20260804.bin) | 5-DSI-TOUCH-A 屏幕出厂固件 |
| [ESP32-P4-WIFI6-DB-7-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-7-DSI-TOUCH-A_Factory_20260804.bin) | 7-DSI-TOUCH-A 屏幕出厂固件 |
| [ESP32-P4-WIFI6-DB-8-DSI-TOUCH-A_Factory_20260804.bin](ESP32-P4-WIFI6-DB-8-DSI-TOUCH-A_Factory_20260804.bin) | 8-DSI-TOUCH-A 屏幕出厂固件 |

## 烧录

在本目录执行，以下命令以 10.1-DSI-TOUCH-A 为例。将 `<PORT>` 替换为 ESP32-P4 的串口，其他屏幕使用表中对应的文件名。擦除会清除 ESP32-P4 Flash 中的已有数据。

```bash
python -m pip install esptool
python -m esptool --chip esp32p4 --port <PORT> erase-flash
python -m esptool --chip esp32p4 --port <PORT> write-flash 0x0 ESP32-P4-WIFI6-DB-10.1-DSI-TOUCH-A_Factory_20260804.bin
```

ESP32-C5 无线协处理器的固件位于 [esp32-c5-slave-260623.bin](ESP32-C5/esp32-c5-slave-260623.bin)。ESP-Brookesia 示例见 [brookesia/README_CN.md](brookesia/README_CN.md)。

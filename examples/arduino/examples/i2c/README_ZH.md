# i2c

[English](README.md) | [简体中文](README_ZH.md)

## 功能

扫描共享 I2C 总线（`Wire1`）并打印所有检测到的 7 位设备地址。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板

## 怎么运行

1. 在开发板管理器中安装 `esp32`（Arduino-ESP32 `3.3.11`），必要时按 `examples/arduino/esp32/` 说明安装板级文件。
2. 选择开发板，并按 `.github/product.json` 设置「工具」菜单：

   | 工具菜单 | 取值 |
   | --- | --- |
   | 开发板 | ESP32P4 Dev Module（或 Waveshare ESP32-P4-WIFI6-DB） |
   | Flash Size | 32MB (256Mb) |
   | PSRAM | Enabled |

3. 打开 `.ino` 文件并上传。

## 预期现象

串口监视器输出 `Wire1` 总线上扫描到的 I2C 设备地址。

## 常见问题

暂无。

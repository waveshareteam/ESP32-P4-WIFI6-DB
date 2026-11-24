# gpio

[English](README.md) | [简体中文](README_ZH.md)

## 功能

监测扩展排针上的 GPIO 输入电平并在串口打印状态变化。

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

当外部改变引脚电平时，串口监视器实时输出对应 GPIO 状态。

## 常见问题

暂无。

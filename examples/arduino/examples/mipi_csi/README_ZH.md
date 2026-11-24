# mipi_csi

[English](README.md) | [简体中文](README_ZH.md)

## 功能

采集 OV5647 MIPI-CSI 摄像头画面并在 MIPI-DSI 屏幕上显示。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- OV5647 MIPI-CSI 摄像头模组与微雪 MIPI-DSI 屏幕

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

MIPI-DSI 屏幕上实时显示摄像头预览画面。

## 常见问题

上传前请在代码中选择与实物匹配的屏幕配置。

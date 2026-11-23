# mipi_dsi

[English](README.md) | [简体中文](README_ZH.md)

## 功能

使用 `ESP32_Display_Panel` 初始化 MIPI-DSI 屏幕与 GT911 触摸控制器。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 微雪 MIPI-DSI 触摸屏（5 / 7 / 8 / 10.1 寸）

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

屏幕显示测试图案并响应触摸坐标。

## 常见问题

需安装 `ESP32_Display_Panel`、`ESP32_IO_Expander` 和 `esp-lib-utils`，并在代码中选择与实物匹配的屏幕型号。

# sdmmc

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 4 位 SDMMC 挂载 microSD 卡并执行文件读写测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- FAT32 格式的 microSD 卡

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

串口监视器打印存储卡容量及文件读写校验结果。

## 常见问题

上电前请确保已插入 microSD 卡。

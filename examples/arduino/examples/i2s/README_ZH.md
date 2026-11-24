# i2s

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 `Wire1` 与 `I2S` 初始化 ES8311 音频编解码器，进行音频播放与麦克风测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 接在板载扬声器接口上的喇叭

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

喇叭输出测试音频，串口监视器打印编解码器状态。

## 常见问题

暂无。

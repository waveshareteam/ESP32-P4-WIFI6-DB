<div align="center">
  <h1>ESP32-P4-WIFI6-DB</h1>
  <p><strong>搭载 ESP32-C5 Wi-Fi 6 协处理器及 MIPI-DSI / MIPI-CSI 接口的 ESP32-P4 开发板</strong></p>
  <p><a href="README.md">English</a> | <strong>简体中文</strong></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/actions/workflows/build.yml"><img alt="构建" src="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/releases/latest"><img alt="最新版本" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-P4-WIFI6-DB"></a>
    <a href="LICENSE"><img alt="许可证" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-DB"></a>
  </p>
  <p>
    <a href="https://www.waveshare.com/esp32-p4-wifi6-db.htm">🛒 产品</a> &middot;
    <a href="firmware/">📦 固件</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/arduino/examples/">🔧 Arduino</a> &middot;
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-DB/">📖 文档</a>
  </p>
  <p><img src="docs/images/ESP32-P4-WIFI6-DB.jpg" alt="ESP32-P4-WIFI6-DB 开发板" width="500"></p>
</div>

---

## ✨ 概述

Waveshare ESP32-P4-WIFI6-DB 是一款以 ESP32-P4 为主控、通过 SDIO 连接 ESP32-C5 无线协处理器的多媒体开发板。本仓库提供 ESP-IDF 和 Arduino 示例、开源出厂固件源码、实测出厂固件以及硬件原理图。

| 功能 | 器件或接口 |
| --- | --- |
| MCU | ESP32-P4（双核 RISC-V 最高 360 MHz + 低功耗核）、ESP32-C5（通过 SDIO 提供 Wi-Fi 6 / 蓝牙） |
| 存储器 | 32 MB Flash，32 MB PSRAM |
| 显示 | 2 通道 MIPI-DSI（支持外接 5 / 7 / 8 / 10.1 寸微雪 DSI 触摸屏） |
| 触摸 | GT911 电容触摸控制器（I2C） |
| 摄像头 | MIPI-CSI（OV5647） |
| 音频 | ES8311 编解码器、模拟麦克风输入与扬声器功放 |
| 存储与扩展 | microSD（4 位 SDMMC）、USB Host、扩展排针 GPIO |

### 支持的屏幕

| 面板 | 控制器 | 分辨率 |
| --- | --- | --- |
| [Waveshare 5-DSI-TOUCH-A](https://www.waveshare.net/shop/5-DSI-TOUCH-A.htm) | HX8394，5 英寸 | 720 × 1280 |
| [Waveshare 7-DSI-TOUCH-A](https://www.waveshare.net/shop/7-DSI-TOUCH-A.htm) | ILI9881C，7 英寸 | 720 × 1280 |
| [Waveshare 8-DSI-TOUCH-A](https://www.waveshare.net/shop/8-DSI-TOUCH-A.htm) | JD9365，8 英寸 | 800 × 1280 |
| [Waveshare 10.1-DSI-TOUCH-A](https://www.waveshare.net/shop/10.1-DSI-TOUCH-A.htm) | JD9365，10.1 英寸 | 800 × 1280 |

构建显示相关示例前，请确认所选面板与实际连接的屏幕型号匹配。

### ESP32-P4 与 ESP32-C5 无线

ESP32-P4 本身不集成射频，Wi-Fi 6 和蓝牙由板载 ESP32-C5 协处理器通过 SDIO 上的 ESP-Hosted 提供。ESP32-C5 需要兼容的从机固件；烧录 ESP32-P4 示例不会更新 C5。修改或重新烧录协处理器前，请确认从机固件与 P4 工程使用的 ESP-Hosted 和 `esp_wifi_remote` 组件版本兼容。预编译的 ESP32-C5 从机固件位于 [`firmware/ESP32-C5/esp32-c5-slave-260623.bin`](firmware/ESP32-C5/esp32-c5-slave-260623.bin)（说明见 [`firmware/README_ZH.md`](firmware/README_ZH.md)）。

## 📦 快速开始

体验示例最快的方法是使用[最新版本](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/releases/latest)中打包好的可烧录固件。

1. 下载所需示例和框架版本对应的 `*-combined.zip`。
2. 解压后通过 `python -m pip install esptool` 安装 esptool。
3. 通过 USB 连接开发板。
4. Windows 运行 `flash_combined.bat COMx`，Linux 运行 `./flash_combined.sh /dev/ttyACM0`。
5. 如果开发板未自动重启，请手动复位。

出厂固件（恢复开箱演示）：在 [`firmware`](firmware/) 目录下载对应屏幕型号且文件名带 `Factory` 的文件。

## 🧪 示例

### ESP-IDF

| 示例 | 主要功能 |
| --- | --- |
| [00_board_check](examples/esp-idf/00_board_check/) | 芯片、Flash、PSRAM 与串口基础检测 |
| [01_gpio_input](examples/esp-idf/01_gpio_input/) | 在屏幕上实时显示 GPIO 输入状态 |
| [02_i2c_console](examples/esp-idf/02_i2c_console/) | 共享 I2C 总线扫描与交互诊断 |
| [03_sdmmc](examples/esp-idf/03_sdmmc/) | microSD 4 位 SDMMC 读写测试 |
| [04_mipi_dsi](examples/esp-idf/04_mipi_dsi/) | MIPI-DSI 屏幕显示与背光测试 |
| [05_mipi_csi](examples/esp-idf/05_mipi_csi/) | OV5647 MIPI-CSI 摄像头画面预览 |
| [06_wifi](examples/esp-idf/06_wifi/) | 通过 ESP32-C5 / ESP-Hosted 连接 Wi-Fi |
| [07_i2s_codec](examples/esp-idf/07_i2s_codec/) | ES8311 音频播放、录音与回环测试 |
| [08_usb_host_msc](examples/esp-idf/08_usb_host_msc/) | USB Host 大容量存储设备读写测试 |
| [09_board_manager](examples/esp-idf/09_board_manager/) | ESP Board Manager 板级初始化与外设测试 |

### Arduino

| 示例 | 主要功能 |
| --- | --- |
| [board_check](examples/arduino/examples/board_check/) | 芯片、Flash、PSRAM 与串口基础检测 |
| [gpio](examples/arduino/examples/gpio/) | 扩展排针 GPIO 输入状态监测 |
| [i2c](examples/arduino/examples/i2c/) | 共享 I2C 总线扫描 |
| [i2s](examples/arduino/examples/i2s/) | ES8311 音频播放与麦克风测试 |
| [mipi_csi](examples/arduino/examples/mipi_csi/) | OV5647 MIPI-CSI 摄像头画面预览 |
| [mipi_dsi](examples/arduino/examples/mipi_dsi/) | MIPI-DSI 屏幕显示与触摸测试 |
| [sdmmc](examples/arduino/examples/sdmmc/) | microSD 4 位 SDMMC 读写测试 |

Arduino-ESP32 `3.3.11` 的板级支持文件位于 [`examples/arduino/esp32`](examples/arduino/esp32/)。

## 🛠️ 开发环境

| 开发框架 | 版本 | 示例数 |
| --- | --- | ---: |
| ESP-IDF | `v5.5.6`、`v6.1.0` | 10 |
| Arduino-ESP32 | `3.3.11` | 7 |

## 📚 资料

- [原理图](hardware/schematic/)
- [产品页](https://www.waveshare.com/esp32-p4-wifi6-db.htm)
- [Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB/)

## 🤝 支持

- 产品使用、硬件或订单问题：[微雪技术支持](https://docs.waveshare.net/ESP32-P4-WIFI6-DB/Technical-Support/)。

## 📄 许可证

Apache License 2.0，详见 [LICENSE](LICENSE)。

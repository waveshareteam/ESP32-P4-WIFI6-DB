<div align="center">
  <h1>ESP32-P4-WIFI6-DB</h1>
  <p><strong>搭载 ESP32-C5 Wi-Fi 6 协处理器、提供 MIPI-DSI / MIPI-CSI 接口的 ESP32-P4 开发板</strong></p>
  <p>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-P4-WIFI6-DB"></a>
  </p>
  <p>
    <a href="README.md">English</a> ·
    <a href="https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm">🛒 产品页面</a> ·
    <a href="https://docs.waveshare.net/ESP32-P4-WIFI6-DB">📚 Wiki</a> ·
    <a href="components/esp32_p4_wifi6_db/">🔌 BSP</a> ·
    <a href="examples/esp_idf/">🧩 ESP-IDF 示例</a> ·
    <a href="examples/Arduino/">🔧 Arduino 示例</a> ·
    <a href="firmware/">📦 固件</a>
  </p>
  <img src="asserts/ESP32-P4-WIFI6-DB.jpg" alt="Waveshare ESP32-P4-WIFI6-DB" width="720">
</div>

---

## ✨ 概述

本仓库提供 Waveshare ESP32-P4-WIFI6-DB 开发板的板级支持包（BSP）、
ESP-IDF 和 Arduino 示例、ESP-Brookesia 固件源码、出厂固件及原理图。

开发板将 ESP32-P4 的多媒体能力与通过 SDIO 上的 ESP-Hosted 连接的 ESP32-C5
无线协处理器结合，提供双通道 MIPI-DSI（支持 5、7、8、10.1 英寸面板）、
MIPI-CSI 摄像头接口、ES8311 音频编解码器、microSD、USB 和扩展 GPIO。
购买信息见[产品页面](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)。

## 🖥️ 硬件概览

| 功能 | 器件 / 接口 |
| --- | --- |
| 主处理器 | ESP32-P4（`esp32p4` 目标） |
| 无线 | ESP32-C5 协处理器，通过 SDIO 上的 ESP-Hosted 提供 Wi-Fi 6 和蓝牙 |
| 显示 | 双通道 MIPI-DSI；可选 5 英寸 HX8394、7 英寸 ILI9881C、8 英寸 JD9365 或 10.1 英寸 JD9365 面板 |
| 触摸 | GT911 电容触摸控制器，通过共享 I2C 总线轮询 |
| 摄像头 | MIPI-CSI；摄像头示例配置 OV5647 |
| 音频 | ES8311 编解码器，单路模拟麦克风输入和扬声器功放 |
| 存储与扩展 | microSD（4-bit SDMMC）、USB Host、SPIFFS、扩展 GPIO |
| 板级支持 | 本地组件 [`esp32_p4_wifi6_db`](components/esp32_p4_wifi6_db/) 0.0.1 |
| 原理图 | [hardware/ESP32-P4-WIFI6-DB.pdf](hardware/ESP32-P4-WIFI6-DB.pdf) |

完整引脚定义、40 针排针对照和逐个 GPIO 的分配见
[硬件参考](docs/zh_CN/hw-reference/user-guide-esp32-p4-wifi6-db.md)。

### BSP 引脚分配

| 资源 | BSP 配置 |
| --- | --- |
| I2C | SDA GPIO7、SCL GPIO8；ES8311、GT911、LCD 背光和摄像头 SCCB 共用 |
| 音频 | ES8311，单路模拟麦克风输入；I2S MCLK/BCLK/WS/DOUT/DIN 为 GPIO13/12/10/9/11；功放使能 GPIO53 |
| microSD | 4-bit SDMMC；CLK/CMD/D0/D1/D2/D3 为 GPIO43/44/39/40/41/42；卡电源使能 GPIO45，低有效；SDMMC IO 供电使用 LDO 通道 4 |
| 显示 | 双通道 MIPI-DSI；LCD 复位 GPIO 未使用；I2C 背光地址 `0x45`，亮度寄存器 `0x96` |
| 触摸 | GT911 轮询；复位和中断 GPIO 未使用 |
| 摄像头 | MIPI-CSI 采集；摄像头示例配置 OV5647 |
| 存储 / USB | SPIFFS 和 USB Host 支持 |
| RTC | BSP 不提供外部 RTC 支持（`BSP_CAPS_RTC=0`） |

实现细节见 [BSP README](components/esp32_p4_wifi6_db/README_CN.md) 和
[BSP API 参考](components/esp32_p4_wifi6_db/API_CN.md)。

### ESP-IDF LCD 选择

| BSP 选项 | 面板控制器 | 分辨率 |
| --- | --- | --- |
| `CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A` | HX8394，5 英寸 | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A` | ILI9881C，7 英寸 | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A` | JD9365，8 英寸 | 800 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A` | JD9365，10.1 英寸 | 800 × 1280 |

[BSP Kconfig](components/esp32_p4_wifi6_db/Kconfig) 默认选择 **10.1 英寸 JD9365**；
[Brookesia 默认配置](firmware/brookesia/sdkconfig.defaults) 也选择
**10.1 英寸 JD9365**。构建前应选择实际连接的面板。默认配置不会覆盖已有的
`sdkconfig`；判断实际生效选项时，应检查工程配置和生成的 `build/config/sdkconfig.h`。

## 🚀 快速开始

安装 ESP-IDF（BSP 和 Brookesia 的组件清单要求 **>=5.5**）并打开已配置环境的终端，
先构建开机自检示例：

```powershell
cd examples\esp_idf\00_board_check
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为 ESP32-P4 的实际串口。自检通过后，从下方表格中选择示例。
构建前通过 `idf.py menuconfig` 配置 LCD 及所需的 Wi-Fi 凭据；组件管理器会在
配置期间解析依赖。仓库忽略本地 `build/`、`managed_components/`、`sdkconfig`
和 `dependencies.lock` 文件。

`>=5.5` 是组件清单的版本约束，不代表所有示例已在所有更新版本上验证；
依赖组件还可能施加额外约束。

## 🧪 ESP-IDF 示例

| 示例 | 用途 |
| --- | --- |
| [00_board_check](examples/esp_idf/00_board_check) | 输出芯片、Flash、PSRAM、堆内存信息和周期心跳 |
| [01_gpio_input](examples/esp_idf/01_gpio_input) | 在 LCD 上显示 GPIO 输入状态 |
| [02_i2c_console](examples/esp_idf/02_i2c_console) | 交互式 I2C 扫描及寄存器读写控制台 |
| [03_sdmmc](examples/esp_idf/03_sdmmc) | microSD 文件写入、读取和重命名测试 |
| [04_mipi_dsi](examples/esp_idf/04_mipi_dsi) | LCD 输出测试；当前显示纯白屏，彩条分支未启用 |
| [05_mipi_csi](examples/esp_idf/05_mipi_csi) | 使用 PPA 处理并在 LCD 上预览摄像头画面 |
| [06_wifi](examples/esp_idf/06_wifi) | Wi-Fi STA 联网和网关 ping |
| [07_i2s_codec](examples/esp_idf/07_i2s_codec) | ES8311 内置音频播放或麦克风回放 |
| [08_usb_host_msc](examples/esp_idf/08_usb_host_msc) | USB 大容量存储文件操作和顺序读写速度测试 |

接线和操作方法请查看各示例的 README 及配置。

## 🧪 Arduino 示例

本地板卡包的文档面向 **Arduino-ESP32 3.3.11**。按
[Arduino 配置说明](examples/Arduino/README_CN.md) 安装
`waveshare_esp32_p4_wifi6_db` variant 和板卡条目，然后选择
`Waveshare ESP32-P4-WIFI6-DB`。板卡条目默认使用 32 MB Flash、启用 PSRAM，
分区方案为 `default_32MB`。各示例所需的库见对应 README。

| 示例 | 用途 |
| --- | --- |
| [board_check](examples/Arduino/board_check) | 芯片和内存信息 |
| [gpio](examples/Arduino/gpio) | 串口交互式 GPIO 测试 |
| [i2c](examples/Arduino/i2c) | I2C1 地址扫描 |
| [i2s](examples/Arduino/i2s) | ES8311 48 kHz 单声道麦克风到扬声器回环 |
| [sdmmc](examples/Arduino/sdmmc) | 4-bit microSD 文件读写 |
| [mipi_dsi](examples/Arduino/mipi_dsi) | LCD 彩条和背光测试 |
| [mipi_csi](examples/Arduino/mipi_csi) | OV5647 摄像头 LCD 预览 |

Arduino `mipi_dsi` 和 `mipi_csi` 默认使用 **10.1 英寸 A 型 JD9365**，
800×1280、RGB565、双 DSI lane、1500 Mbps/lane。`mipi_dsi` 在
`esp_panel_drivers_conf.h` 中选择面板；`mipi_csi` 在 `lcd_panel.h` 中选择
JD9365，在 `esp_lcd_jd9365.h` 中选择 10.1 英寸 A 型配置。
这些示例配置独立于 ESP-IDF BSP 的 Kconfig。

## 📡 ESP32-P4 与 ESP32-C5 无线

ESP32-P4 本身不集成射频。Wi-Fi 和蓝牙由 ESP32-C5 协处理器通过 SDIO 上的
ESP-Hosted 提供。ESP32-C5 需要兼容的 ESP-Hosted 从机固件；烧录 ESP32-P4 示例
不会更新 C5。修改或重新烧录协处理器前，请确保从机固件与各无线工程声明的
`esp_hosted` 和 `esp_wifi_remote` 版本兼容。

## 📦 固件

[`firmware/`](firmware/) 目录包含两类固件：

- [`firmware/brookesia/`](firmware/brookesia/README_CN.md) 是 ESP-Brookesia
  板级综合应用：包含计算器、画板、音频、摄像头、视频、设置、Xiaozhi 和 GPIO
  应用的综合界面。
- [`firmware/factory-firmware/`](firmware/factory-firmware/) 提供 5、7、8、10.1
  英寸面板的 ESP32-P4 出厂固件，以及 ESP32-C5 从机固件。

构建 Brookesia：

```powershell
cd firmware\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

仓库中的出厂固件是独立产物，不能仅凭其存在就认定它与重新解析的主机依赖兼容。

## 🗂️ 仓库目录

| 路径 | 内容 |
| --- | --- |
| [`components/esp32_p4_wifi6_db/`](components/esp32_p4_wifi6_db/) | ESP-IDF BSP、头文件、Kconfig 和组件清单 |
| [`examples/esp_idf/`](examples/esp_idf/) | ESP-IDF 外设示例 |
| [`examples/Arduino/`](examples/Arduino/README_CN.md) | Arduino 示例及本地板卡定义 |
| [`firmware/brookesia/`](firmware/brookesia/README_CN.md) | ESP-Brookesia 板级综合应用 |
| [`firmware/factory-firmware/`](firmware/factory-firmware/) | ESP32-P4 和 ESP32-C5 出厂固件 |
| [`hardware/`](hardware/) | 开发板原理图 |
| [`docs/`](docs/) | 硬件参考（引脚定义、GPIO 分配、使用注意） |
| [`asserts/`](asserts/) | 文档使用的产品图片 |

## 📚 文档

- [产品页面](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)
- [官方 Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)
- [硬件参考](docs/zh_CN/hw-reference/user-guide-esp32-p4-wifi6-db.md)
- [BSP README](components/esp32_p4_wifi6_db/README_CN.md)
- [BSP API 参考](components/esp32_p4_wifi6_db/API_CN.md)
- [BSP 更新日志](components/esp32_p4_wifi6_db/CHANGELOG.md)
- [Arduino 示例](examples/Arduino/README_CN.md)
- [Brookesia 固件](firmware/brookesia/README_CN.md)
- [原理图](hardware/ESP32-P4-WIFI6-DB.pdf)

## ✅ 验证范围

本概览依据仓库源码和默认配置整理，不代表当前版本已经编译成功或完成全部外设验证。
应针对所选示例、框架版本、面板和开发板分别确认编译、烧录及运行结果。

## 🤝 支持与贡献

欢迎提交贡献和可复现的问题报告。请附上开发板版本、所接面板、示例路径、
ESP-IDF 或 Arduino-ESP32 版本、复现步骤、预期行为、实际行为和相关串口日志。

- [提交 Issue](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/issues/new)

## 📄 许可证

本仓库采用 Apache License 2.0 许可，见 [LICENSE](LICENSE)。

# ESP32-P4-WIFI6-DB

[English](README.md) | 简体中文

本仓库提供 Waveshare ESP32-P4-WIFI6-DB 开发板的板级支持包（BSP）、
ESP-IDF 和 Arduino 示例、出厂固件及原理图。

## 仓库目录

| 路径 | 内容 |
| --- | --- |
| [firmware/esp32_p4_wifi6_db](firmware/esp32_p4_wifi6_db) | ESP-IDF BSP、头文件、Kconfig 和组件清单 |
| [examples/esp_idf](examples/esp_idf) | 外设示例和 Brookesia 应用 |
| [examples/Arduino](examples/Arduino/README_CN.md) | Arduino 示例及本地板卡定义 |
| [firmware/factory-firmware](firmware/factory-firmware) | 5、7、8、10.1 英寸面板的 ESP32-P4 固件，以及 ESP32-C5 从机固件 |
| [hardware/ESP32-P4-WIFI6-DB.pdf](hardware/ESP32-P4-WIFI6-DB.pdf) | 开发板原理图 |

实现细节见 [BSP README](firmware/esp32_p4_wifi6_db/README_CN.md) 和
[BSP API 参考](firmware/esp32_p4_wifi6_db/API_CN.md)。

## 板级支持

工程以 ESP32-P4 为目标，通过 SDIO 使用 ESP-Hosted 连接 ESP32-C5 无线协处理器。
显示和摄像头示例需要连接对应外设。

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

### ESP-IDF LCD 选择

| BSP 选项 | 面板控制器 | 分辨率 |
| --- | --- | --- |
| `CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A` | HX8394，5 英寸 | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A` | ILI9881C，7 英寸 | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A` | JD9365，8 英寸 | 800 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A` | JD9365，10.1 英寸 | 800 × 1280 |

[BSP Kconfig](firmware/esp32_p4_wifi6_db/Kconfig) 默认选择 **10.1 英寸 JD9365**；
[Brookesia 默认配置](examples/esp_idf/brookesia/sdkconfig.defaults) 选择
**8 英寸 JD9365**。构建前应选择实际连接的面板。默认配置不会覆盖已有的
`sdkconfig`；判断实际生效选项时，应检查工程配置和生成的 `build/config/sdkconfig.h`。

## ESP-IDF 示例

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
| [brookesia](examples/esp_idf/brookesia) | 包含计算器、画板、音频、摄像头、视频、设置、Xiaozhi 和 GPIO 应用的综合界面 |

接线和操作方法请查看各示例的 README 及配置。

### 构建和运行

安装 ESP-IDF 并打开已配置环境的终端。BSP 和 Brookesia 的组件清单要求
**ESP-IDF >=5.5**。这是组件清单的版本约束，不代表所有示例已在所有更新
版本上验证；依赖组件还可能施加额外约束。

例如，从仓库根目录执行：

```powershell
cd examples\esp_idf\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为 ESP32-P4 的实际串口。使用外设示例时，改为进入对应目录。
构建前配置 LCD 及所需的 Wi-Fi 凭据。组件管理器会在配置期间解析依赖。
仓库忽略本地 `build/`、`managed_components/`、`sdkconfig` 和 `dependencies.lock` 文件。

ESP32-C5 需要兼容的 ESP-Hosted 从机固件；烧录 ESP32-P4 示例不会更新 C5。
仓库中的出厂固件是独立产物，不能仅凭其存在就认定它与重新解析的主机依赖兼容。

## Arduino 示例

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

## 验证范围

本概览依据仓库源码和默认配置整理，不代表当前版本已经编译成功或完成全部外设验证。
应针对所选示例、框架版本、面板和开发板分别确认编译、烧录及运行结果。

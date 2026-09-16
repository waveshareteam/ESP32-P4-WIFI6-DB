# ESP32-P4-WIFI6-DB

[English](../../en/hw-reference/user-guide-esp32-p4-wifi6-db.md)

ESP32-P4-WIFI6-DB 是一款以 ESP32-P4NRW32X 为主处理器（双核高性能 RISC-V + 单核低功耗核，
封装内堆叠 32 MB PSRAM）、搭配 ESP32-C5-MINI-1 Wi-Fi 6 / Bluetooth 5（LE）协处理器
（通过 SDIO 连接）的开发板。板载双 lane MIPI-DSI 触摸屏接口、双 lane MIPI-CSI 摄像头接口、
ES8311 音频编解码器、microSD、USB 2.0 High-Speed OTG，以及 40 针扩展排针。

![ESP32-P4-WIFI6-DB](../../../asserts/ESP32-P4-WIFI6-DB.jpg)

> **说明**
>
> 本页内容整理自三个来源：板级支持包（BSP）
> [`components/waveshare_bsp/esp32_p4_wifi6_db`](../../../components/waveshare_bsp/esp32_p4_wifi6_db/)、
> 原理图 [`hardware/ESP32-P4-WIFI6-DB.pdf`](../../../hardware/ESP32-P4-WIFI6-DB.pdf)
> 以及[官方 Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)。
> GPIO 分配已在 BSP 与原理图网表之间交叉核对。机械/外观信息（板卡外形、接口具体型号）
> 仅见于 Wiki，本页不再重复。

## 板载资源

- **主处理器**：ESP32-P4NRW32X —— 双核高性能 RISC-V + 单核低功耗核，封装内堆叠 32 MB PSRAM。
- **Flash**：32 MB 外置 NOR Flash，QIO 模式（示例 `sdkconfig.defaults` 配置为
  `CONFIG_ESPTOOLPY_FLASHSIZE_32MB`）。
- **PSRAM**：32 MB 封装内。示例默认 200 MHz（`CONFIG_SPIRAM_SPEED_200M`）；
  Brookesia 使用 Hex 模式 250 MHz。
- **无线协处理器**：ESP32-C5-MINI-1（2.4 / 5 GHz Wi-Fi 6 + Bluetooth 5 LE），通过 4-bit
  SDIO 连接，由应用层 `esp_hosted` / `esp_wifi_remote` 组件驱动；BSP 本身不配置 SDIO 链路。
  板上另有 4 针排针 `H4`（BOOT、U0RXD、U0TXD、GND）引出 ESP32-C5 的下载串口，
  用于重新烧录从机固件。
- **USB 转串口**：CH343P，接 USB Type-C 口；提供供电、固件下载和 UART0（GPIO37/GPIO38）
  串口监视。
- **USB OTG**：4 针连接器，直连 ESP32-P4 内置 USB 2.0 High-Speed PHY（DM/DP）。
  BSP 将其作为 USB Host 使用（`bsp_usb_host_start()`）。
- **显示**：双 lane MIPI-DSI，24 针 FPC 座，屏幕型号通过 Kconfig 选择 ——
  HX8394（5 英寸，720×1280，700 Mbps/lane）、ILI9881C（7 英寸，720×1280，1000 Mbps/lane）、
  JD9365（8 / 10.1 英寸，800×1280，1500 Mbps/lane，默认）。无硬件 LCD 复位引脚，
  背光通过 I2C 控制。
- **触摸**：GT911 电容触摸控制器，接在共享 I2C 总线上，采用轮询方式（复位/中断引脚未接）。
  驱动会依次尝试 `0x5D` 和 `0x14` 两个地址。
- **音频**：ES8311 编解码器，板载麦克风（单路模拟输入）、3.5 mm 耳机座，以及由板载功放
  驱动的扬声器接口（功放使能 GPIO53）。
- **存储**：microSD，4-bit SDMMC（slot 0），卡电源由 GPIO45 控制的 P-MOS 开关；
  SDMMC IO 电压来自片上 LDO 通道 4。
- **摄像头**：双 lane MIPI-CSI，24 针 FPC 座，通过 `esp_video` 使用。仓库示例配置为
  OV5647（RAW8 800×1280 @ 50 fps）。摄像头 XCLK/RESET 未由 BSP 驱动（`GPIO_NUM_NC`）。
- **扩展排针**：40 针（2×20）排针，采用 Raspberry Pi Pico 引脚布局；共引出 27 路 GPIO，
  其中 25 路由 `bsp_get_header_gpios()` 枚举（见下文）。
- **按键 / 指示灯**：BOOT 按键接 GPIO35，RESET 按键接 `CHIP_PU`，电源指示灯。
  BSP 未注册按键（`BSP_CAPS_BUTTONS = 0`）。
- **RTC**：GPIO0/GPIO1 接 32.768 kHz 晶振，供 ESP32-P4 RTC 慢时钟使用；无外部 RTC 芯片
  （`BSP_CAPS_RTC = 0`）。
- **ESP-IDF target**：`esp32p4`，要求 ESP-IDF >= 5.5。

## 外设速查

| 模块 | 器件 / 功能 | 接口 | 地址 / 参数 | GPIO / 信号 |
| --- | --- | --- | --- | --- |
| LCD | MIPI-DSI 显示屏 | MIPI-DSI | 2-lane；HX8394 / ILI9881C / JD9365，Kconfig 可选；RGB565（默认）或 RGB888 | 无 LCD 复位引脚（`GPIO_NUM_NC`）；背光通过共享 I2C 控制 |
| 触摸 | GT911 电容触摸 | I2C | 7-bit 地址 `0x5D` 或 `0x14`（均会尝试） | SDA=GPIO7，SCL=GPIO8；RST/INT 未接 |
| LCD 背光 | 背光控制器 | I2C | 7-bit 地址 `0x45`；亮度寄存器 `0x96`，数据 `0..255` | SDA=GPIO7，SCL=GPIO8 |
| 摄像头 | MIPI-CSI 摄像头接口 | MIPI-CSI + SCCB | 2-lane；示例配置 OV5647，其他 `esp_video` 传感器可在 Kconfig 中选择 | SCCB 复用 GPIO7/GPIO8；XCLK/RESET 未由 BSP 驱动 |
| 音频 | ES8311 Codec | I2C + I2S | I2C 7-bit 地址 `0x18`（`ES8311_CODEC_DEFAULT_ADDR`）；默认 48 kHz、16-bit、单声道、全双工 | I2C：SDA=GPIO7，SCL=GPIO8；MCLK=GPIO13，SCLK=GPIO12，WS=GPIO10，DOUT=GPIO9，DIN=GPIO11 |
| 功放控制 | 板载扬声器功放使能 | GPIO | 高电平有效 | GPIO53 |
| TF 卡 | SDMMC 4-bit | SDMMC | 4-bit 模式，slot 0；IO 供电来自 LDO 通道 4 | CLK=GPIO43，CMD=GPIO44，D0=GPIO39，D1=GPIO40，D2=GPIO41，D3=GPIO42；电源控制=GPIO45（低有效） |
| Wi-Fi/BT 协处理器 | ESP32-C5-MINI-1，ESP-Hosted 链路 | SDIO | 4-bit；六条线均有 51 kΩ 上拉 | CLK=GPIO18，CMD=GPIO19，D0=GPIO14，D1=GPIO15，D2=GPIO16，D3=GPIO17；CHIP_PU=GPIO54（低有效）；IO2 握手=GPIO6 |
| USB OTG | USB 2.0 High-Speed Host | USB 2.0 HS | 芯片内置 USB 2.0 PHY | D+/D- 直连 ESP32-P4 DM/DP（4 针连接器） |
| USB 1.1 | 全速 USB PHY | USB 1.1 FS | BSP 未使用 | D-=GPIO24，D+=GPIO25（引至排针 19/20 脚） |
| USB 转串口 | CH343P | UART0 | 下载和串口监视 | TXD=GPIO37，RXD=GPIO38（USB Type-C） |
| 按键 | BOOT / RESET | GPIO | BOOT 为 strapping 引脚 | BOOT=GPIO35；RESET=`CHIP_PU` |

## 引脚定义

### 扩展排针

40 针排针采用 Raspberry Pi Pico 的引脚排列，下表给出每个引脚对应的 ESP32-P4 信号
（原理图位号 `U10`）。

| 引脚 | 信号 | 引脚 | 信号 |
| ---: | --- | ---: | --- |
| 1 | GPIO52 | 40 | VBUS（USB 5 V） |
| 2 | GPIO51 | 39 | VSYS（5 V） |
| 3 | GND | 38 | GND |
| 4 | GPIO31 | 37 | 3V3_EN |
| 5 | GPIO30 | 36 | 3V3 输出 |
| 6 | GPIO29 | 35 | GPIO20 |
| 7 | GPIO28 | 34 | GPIO21 |
| 8 | GND | 33 | GND |
| 9 | GPIO50 | 32 | GPIO22 |
| 10 | GPIO49 | 31 | GPIO23 |
| 11 | GPIO5 | 30 | RUN（ESP32-P4 EN） |
| 12 | GPIO4 | 29 | GPIO26 |
| 13 | GND | 28 | GND |
| 14 | GPIO3 | 27 | GPIO27 |
| 15 | GPIO2 | 26 | GPIO32 |
| 16 | GPIO8（I2C SCL） | 25 | GPIO33 |
| 17 | GPIO7（I2C SDA） | 24 | GPIO46 |
| 18 | GND | 23 | GND |
| 19 | GPIO24（USB1 D-） | 22 | GPIO47 |
| 20 | GPIO25（USB1 D+） | 21 | GPIO48 |

`bsp_get_header_gpios()` 返回下方 BSP 只读数组（25 项）。GPIO7/GPIO8 虽引到排针上，
但因承载共享 I2C 总线而未纳入该数组。

```text
52, 51, 31, 30, 29, 28, 50, 49, 5, 4, 3, 2, 24,
25, 20, 21, 22, 23, 26, 27, 32, 33, 46, 47, 48
```

GPIO24/GPIO25 同时是 ESP32-P4 全速 USB 1.1 PHY 的数据线；应用未启用该 PHY 时可作为
普通 GPIO 使用。

### 摄像头与显示接口

两个 FPC 座的 2/3 脚均引出共享 I2C 总线（GPIO7/GPIO8），分别供摄像头 SCCB 接口，
以及显示屏上的 GT911 触摸控制器和背光控制器使用。

| 接口 | 类型 | ESP32-P4 信号 |
| --- | --- | --- |
| `J1`（24 针） | MIPI-CSI | CSI_CLK_P/N、CSI_D0_P/N、CSI_D1_P/N、I2C SDA/SCL、3.3 V、GND |
| `J2`（24 针） | MIPI-DSI | DSI_CLK_P/N、DSI_D0_P/N、DSI_D1_P/N、I2C SDA/SCL、3.3 V、GND |

## GPIO 完整分配

下表列出全部 GPIO（0～54）及其在板上的连接。"排针 N 脚" 指上文的 40 针扩展排针。

| GPIO | 信号名 | 连接到 | 备注 |
| --- | --- | --- | --- |
| GPIO0 | XTAL_32K_N | 32.768 kHz 晶振（`Y2`），经 0 Ω | RTC 慢时钟；挪作他用前需拆除 0 Ω 电阻 |
| GPIO1 | XTAL_32K_P | 32.768 kHz 晶振（`Y2`），经 0 Ω | RTC 慢时钟；挪作他用前需拆除 0 Ω 电阻 |
| GPIO2 | GPIO2 | 排针 15 脚 | `bsp_get_header_gpios()` |
| GPIO3 | GPIO3 | 排针 14 脚 | `bsp_get_header_gpios()` |
| GPIO4 | GPIO4 | 排针 12 脚 | `bsp_get_header_gpios()` |
| GPIO5 | GPIO5 | 排针 11 脚 | `bsp_get_header_gpios()` |
| GPIO6 | C5_IO2 | ESP32-C5 IO2，经 0 Ω（`R15`） | 保留给协处理器（ESP-Hosted 握手 / 唤醒）；未引到排针 |
| GPIO7 | I2C SDA | ES8311、GT911、LCD 背光、摄像头 SCCB、排针 17 脚 | 共享 I2C 数据线；未纳入排针 GPIO 数组 |
| GPIO8 | I2C SCL | ES8311、GT911、LCD 背光、摄像头 SCCB、排针 16 脚 | 共享 I2C 时钟线；未纳入排针 GPIO 数组 |
| GPIO9 | I2S DOUT | ES8311 DSDIN | 音频播放数据，ESP32-P4 → codec |
| GPIO10 | I2S WS | ES8311 LRCK | 音频帧同步 |
| GPIO11 | I2S DIN | ES8311 ASDOUT | 音频采集数据，codec → ESP32-P4 |
| GPIO12 | I2S SCLK | ES8311 SCLK | 音频位时钟 |
| GPIO13 | I2S MCLK | ES8311 MCLK | 音频主时钟 |
| GPIO14 | SDIO D0 | ESP32-C5 IO8 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO15 | SDIO D1 | ESP32-C5 IO7 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO16 | SDIO D2 | ESP32-C5 IO14 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO17 | SDIO D3 | ESP32-C5 IO13 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO18 | SDIO CLK | ESP32-C5 IO9 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO19 | SDIO CMD | ESP32-C5 IO10 | Hosted Wi-Fi/BT 链路；51 kΩ 上拉 |
| GPIO20 | GPIO20 | 排针 35 脚 | `bsp_get_header_gpios()` |
| GPIO21 | GPIO21 | 排针 34 脚 | `bsp_get_header_gpios()` |
| GPIO22 | GPIO22 | 排针 32 脚 | `bsp_get_header_gpios()` |
| GPIO23 | GPIO23 | 排针 31 脚 | `bsp_get_header_gpios()` |
| GPIO24 | GPIO24 / USB1 D- | 排针 19 脚 | `bsp_get_header_gpios()`；启用全速 USB PHY 时为其数据线 |
| GPIO25 | GPIO25 / USB1 D+ | 排针 20 脚 | `bsp_get_header_gpios()`；启用全速 USB PHY 时为其数据线 |
| GPIO26 | GPIO26 | 排针 29 脚 | `bsp_get_header_gpios()` |
| GPIO27 | GPIO27 | 排针 27 脚 | `bsp_get_header_gpios()` |
| GPIO28 | GPIO28 | 排针 7 脚 | `bsp_get_header_gpios()` |
| GPIO29 | GPIO29 | 排针 6 脚 | `bsp_get_header_gpios()` |
| GPIO30 | GPIO30 | 排针 5 脚 | `bsp_get_header_gpios()` |
| GPIO31 | GPIO31 | 排针 4 脚 | `bsp_get_header_gpios()` |
| GPIO32 | GPIO32 | 排针 26 脚 | `bsp_get_header_gpios()` |
| GPIO33 | GPIO33 | 排针 25 脚 | `bsp_get_header_gpios()` |
| GPIO34 | — | 仅测试点 `TP1` | Strapping 引脚；未引到任何连接器 |
| GPIO35 | BOOT | BOOT 按键（`Key1`） | Strapping 引脚；上电时按住进入下载模式 |
| GPIO36 | — | 测试点 `TP2`，3.3 V 上拉（`R36`） | Strapping 引脚；未引到任何连接器 |
| GPIO37 | UART0 TXD | CH343P | 固件下载和串口监视器；不建议作为普通 GPIO 使用 |
| GPIO38 | UART0 RXD | CH343P | 固件下载和串口监视器；不建议作为普通 GPIO 使用 |
| GPIO39 | SD D0 | microSD 卡 | SDMMC 4-bit 数据线；51 kΩ 上拉 |
| GPIO40 | SD D1 | microSD 卡 | SDMMC 4-bit 数据线；51 kΩ 上拉 |
| GPIO41 | SD D2 | microSD 卡 | SDMMC 4-bit 数据线；51 kΩ 上拉 |
| GPIO42 | SD D3 | microSD 卡 | SDMMC 4-bit 数据线；51 kΩ 上拉 |
| GPIO43 | SD CLK | microSD 卡 | SDMMC 时钟线 |
| GPIO44 | SD CMD | microSD 卡 | SDMMC 命令线；51 kΩ 上拉 |
| GPIO45 | SD_VDD_EN | microSD 电源开关（`Q1`，P-MOS） | 低电平有效；未引到排针 |
| GPIO46 | GPIO46 | 排针 24 脚 | `bsp_get_header_gpios()` |
| GPIO47 | GPIO47 | 排针 22 脚 | `bsp_get_header_gpios()` |
| GPIO48 | GPIO48 | 排针 21 脚 | `bsp_get_header_gpios()` |
| GPIO49 | GPIO49 | 排针 10 脚 | `bsp_get_header_gpios()` |
| GPIO50 | GPIO50 | 排针 9 脚 | `bsp_get_header_gpios()` |
| GPIO51 | GPIO51 | 排针 2 脚 | `bsp_get_header_gpios()` |
| GPIO52 | GPIO52 | 排针 1 脚 | `bsp_get_header_gpios()` |
| GPIO53 | PA_CTRL | 扬声器功放使能 | 高电平有效；未引到排针 |
| GPIO54 | C5_CHIP_PU | ESP32-C5 EN，经 0 Ω（`R26`） | 低电平有效，复位 Wi-Fi/BT 协处理器 |

## 使用注意

- GPIO7/GPIO8 为板载共享 I2C 总线，接入 ES8311（`0x18`）、GT911（`0x5D`/`0x14`）、
  LCD 背光控制器（`0x45`）和摄像头 SCCB 接口，并同时引到排针 16/17 脚；外接其他 I2C
  设备时需确认地址不冲突。
- GPIO45 控制 microSD 电源，GPIO53 控制音频功放，GPIO54 复位 ESP32-C5，GPIO6 接
  ESP32-C5 IO2。这些引脚均未引到排针；对应外设工作时不要单独驱动它们。
- GPIO37/GPIO38 是经 CH343P 的 UART0 下载串口（TXD/RXD），用于烧录固件和串口监视器，
  不建议作为普通 GPIO 使用。
- GPIO34～GPIO38 是 ESP32-P4 的 strapping 引脚。GPIO35 为 BOOT 按键，GPIO36 有 3.3 V 上拉，
  GPIO37/GPIO38 为 UART0；GPIO34 和 GPIO36 仅引到测试点。在上电或复位阶段改变这些引脚
  电平前，请先查阅 ESP32-P4 技术规格书。
- GPIO0/GPIO1 经 0 Ω 电阻（`R27`/`R31`）接板上 32.768 kHz 晶振。BSP 未配置 RTC 慢时钟源。
  若要挪作他用，需要先拆除对应的 0 Ω 电阻。
- GPIO24/GPIO25 同时是 ESP32-P4 全速 USB 1.1 PHY 的数据线，并引到排针 19/20 脚；
  应用未启用该 PHY 时仍为普通 GPIO。
- GPIO14～GPIO19 为 ESP32-C5 的 SDIO 链路。烧录 ESP32-P4 示例不会更新 ESP32-C5；
  请确保从机固件与各无线工程声明的 `esp_hosted` / `esp_wifi_remote` 版本兼容。
  预编译的 ESP32-C5 固件位于
  [`firmware/factory-firmware/ESP32-C5`](../../../firmware/factory-firmware/ESP32-C5/)。
- LCD 复位、GT911 复位和中断信号均未接线，由显示/触摸驱动通过软件流程处理（触摸采用轮询）。
- `bsp_sdcard_mount()` 会在 SDMMC 初始化前对卡电源做一次断电（100 ms）再上电（等待 200 ms）
  的循环，并启用内部上拉；板上 CMD 和 D0～D3 另有 51 kΩ 外部上拉。
- 扩展排针 GPIO 数组（`bsp_get_header_gpios()`）是软件层面的枚举，供 `01_gpio_input`
  等示例使用；它有意排除了 GPIO7/GPIO8。

## 产品尺寸

板卡外形及安装孔位见[官方 Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)，
本仓库不再重复。

## 相关文档

- [BSP README](../../../components/waveshare_bsp/esp32_p4_wifi6_db/README_CN.md)
- [BSP API 参考](../../../components/waveshare_bsp/esp32_p4_wifi6_db/API_CN.md)
- [BSP 更新日志](../../../components/waveshare_bsp/esp32_p4_wifi6_db/CHANGELOG.md)
- [原理图](../../../hardware/ESP32-P4-WIFI6-DB.pdf)
- [官方 Wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)
- [产品页面](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)

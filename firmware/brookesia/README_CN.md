# ESP-Brookesia 板级综合测试

[English Version](./README.md)

本示例在 ESP32-P4-WIFI6-DB 开发板上运行 ESP-Brookesia Phone UI，并把多项
板级测试应用集中到一个启动器中。

这是一个集成示例。程序会先初始化本地 BSP、显示与触摸、ES8311 音频和
SPIFFS，再安装界面应用。Settings 应用会初始化 ESP32-C5 Hosted Wi-Fi 和
SNTP 控制路径，Wi-Fi 会在 WLAN 页面请求时启动。本示例不会初始化
PCF85063A RTC，状态栏时钟使用系统时间。

## 启动流程

`app_main()` 依次执行：

1. 把 `factory` 应用分区设置为下次启动分区。
2. 初始化 NVS 并挂载 SPIFFS。
3. 初始化 ES8311 音频路径。
4. 以 RGB565 和 `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL` 启动选中的
   800x1280 JD9365 显示。
5. 启动 ESP-Brookesia Phone 系统并安装应用。
6. 应用安装完成后打开背光。
7. 每秒从系统时间读取时间并刷新状态栏时钟。
8. 根据 Wi-Fi 断开/获取 IP 事件更新图标，并每 5 秒输出 SRAM/PSRAM
   使用情况。

## 已安装应用

启动器会安装：

- Drawpanel
- SpecAnalyzer
- MusicPlayer
- Camera
- VideoPlayer
- Settings
- GpioMonitor

应用安装成功只说明软件初始化执行到了该应用；每条外设路径仍需单独做运行和硬件验证。

## 硬件和配置

| 功能 | 当前工程配置 |
| --- | --- |
| 显示/触摸 | JD9365 800x1280 MIPI-DSI、GT911；当前默认选择 10.1 英寸 A 型变体 |
| 显示缓冲 | RGB565、3 个 MIPI-DPI framebuffer |
| RTC | 本示例未初始化 RTC，状态栏使用系统时间 |
| 音频 | ES8311，通过本地 BSP 和 `bsp_extra` |
| 摄像头 | OV5647 MIPI-CSI，RAW8 800x1280 50 FPS |
| 无线 | ESP32-C5，通过 ESP-Hosted / Wi-Fi Remote |
| Flash/PSRAM 默认值 | 16 MB Flash、250 MHz HEX PSRAM |
| 控制台波特率 | 2,000,000 |
| 分区 | 9 MB `factory`、5 MB `storage` SPIFFS |

仓库中的 `sdkconfig.defaults` 选择了
`CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A`。重新构建前请在 `menuconfig`
中选择与实际连接面板匹配的选项。

开发板原理图见
[hardware/ESP32-P4-WIFI6-DB.pdf](../../hardware/ESP32-P4-WIFI6-DB.pdf)。

全新配置时，请在 `menuconfig` 中确认：

```text
CONFIG_SLAVE_IDF_TARGET_ESP32C5=y
CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
```

ESP32-C5 需要单独烧录兼容的 ESP-Hosted 协处理器固件；烧录本工程只会更新 ESP32-P4。

## 构建和烧录

使用 ESP-IDF 5.5 或更新版本；当前固件已在 ESP-IDF v6.0.1 下烧录验证。

```powershell
cd firmware\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为 ESP32-P4 的实际串口号；monitor 会从 `sdkconfig` 读取控制台波特率。

`components/bsp_extra/idf_component.yml` 通过以下相对路径引用本地 BSP：

```text
../esp32_p4_wifi6_db
```

构建过程还会生成并烧录 `storage` SPIFFS 镜像。仓库没有跟踪
`spiffs/music` 下的 MP3 文件；需要 MusicPlayer 播放时，只加入允许再分发的媒体文件并重新构建镜像。

## 预期启动日志

源码会输出类似：

```text
Switch to partition factory
SPIFFS mount successfully
Display ESP-Brookesia phone demo
```

GUI 启动后应每 5 秒输出一次内存信息。固件已在 ESP-IDF v6.0.1 下烧录验证；
触摸、Wi-Fi、摄像头画面、音频、视频播放和 GPIO 行为仍应按功能分别记录。

## 排障

- 组件解析找不到 BSP 时，先检查上述相对路径，不要直接删除本地覆盖。
- ESP-Hosted 失败时，检查 ESP32-C5 固件、目标选择、传输引脚和复位 GPIO。
- 启动器显示前失败时，检查 JD9365 复位、背光、MIPI 供电/通道配置、
  PSRAM 和 LVGL 任务内存。
- 时钟不更新时，请注意本示例使用系统时间且不会初始化 PCF85063A RTC；
  依赖时钟前，请先从外部设置系统时间，或补充 RTC/SNTP 初始化路径。
- Camera 打开但没有画面时，分别验证 OV5647 供电、MIPI-CSI 连线和 RAW8
  格式。
- MusicPlayer 没有内容时，加入可再分发的音乐文件并重新构建 SPIFFS 镜像。

ESP-Brookesia 框架说明见
[brookesia_core](components/brookesia_core/README_CN.md)。

## 验证边界

本文已按当前源码和配置做核对。固件已在 ESP-IDF v6.0.1 下烧录验证；这不代表
同一次验证已经覆盖所有已安装应用或所有外设路径。

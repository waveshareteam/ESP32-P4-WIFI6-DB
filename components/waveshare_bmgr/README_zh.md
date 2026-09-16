# ESP32-P4 Board Manager 配置

[English](README.md)

本组件为 Waveshare **ESP32-P4-WIFI6-DB** 开发板提供
[ESP Board Manager](https://github.com/espressif/esp-board-manager) 板卡定义
和 MIPI-DSI LCD amend 配置。板卡信息、外设引脚、设备参数以及 LCD 驱动初始化
参数均通过 YAML、`sdkconfig.defaults.board` 和板卡专用 factory hook 组织。

## 内容

### 板卡定义

| 目录 | 说明 |
| --- | --- |
| [`esp32_p4_wifi6_db`](esp32_p4_wifi6_db/README_zh.md) | ESP32-P4-WIFI6-DB |

板卡目录通常包含：

- `board_info.yaml`：板卡名称、芯片和厂商信息；
- `board_devices.yaml`：LCD、背光、触摸、音频、存储和摄像头设备；
- `board_peripherals.yaml`：I2C、I2S、GPIO、LDO 和 MIPI-DSI 配置；
- `setup_device.c`：板卡专用的 LCD 和触摸 factory hook；
- `lcd_brightness.c`：I2C 背光控制器的 `custom` 设备驱动；
- `sdkconfig.defaults.board`：板卡默认 Kconfig 配置。

### LCD amend

`amends/` 中的每个目录对应一块可替换默认 JD9365 10.1 寸屏幕的可选 Waveshare
DSI 触摸屏。amend 会同时更新 DSI 参数、LCD DPI 时序、触摸分辨率以及
`waveshare_lcd` factory 选择。

| Amend | 驱动 | 分辨率 | DSI lane | 速率 |
| --- | --- | ---: | ---: | ---: |
| [`lcd_5_dsi_touch_a`](amends/lcd_5_dsi_touch_a) | HX8394 | 720x1280 | 2 | 700 Mbps |
| [`lcd_7_dsi_touch_a`](amends/lcd_7_dsi_touch_a) | ILI9881C | 720x1280 | 2 | 1000 Mbps |
| [`lcd_8_dsi_touch_a`](amends/lcd_8_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 Mbps |
| [`lcd_10_1_dsi_touch_a`](amends/lcd_10_1_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 Mbps |

这与 [`waveshare_bsp` 版本](../waveshare_bsp/esp32_p4_wifi6_db/README_CN.md) 支持的四种屏幕
选项一致。

## 使用方式

`idf.py bmgr` 在 ESP-IDF 工程目录下执行。本板卡包没有发布到组件注册表，需要用
`-c` 指定本目录的路径才能被发现，然后选择板卡：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr
```

如果需要使用默认 10.1 寸屏幕以外的其他 DSI 屏幕，加上 `-a` 指向对应的 amend
目录：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr -a <repo>/components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

[`examples/esp_idf/09_board_manager`](../../examples/esp_idf/09_board_manager/README_zh.md)
是一个按此方式配置好的完整工程。也可以参考 [`amends/README_zh.md`](amends/README_zh.md)
和板卡说明
[`esp32_p4_wifi6_db/README_zh.md`](esp32_p4_wifi6_db/README_zh.md)。

## LCD 配置

`Kconfig` 中包含上游完整的 Waveshare 屏幕选项列表（与其他 Waveshare
ESP32-P4 开发板共用），但本仓库仅为上表四款屏幕提供了对应的 amend 目录。
`Kconfig` 提供以下 LCD 选项：

- 默认从选中板卡的 Board Manager YAML 读取 LCD 配置；
- 通过 amend 选择具体的 LCD 驱动和初始化时序；
- 支持 RGB565 和 RGB888；
- MIPI-DSI DPI frame buffer 数量可设置为 1 到 3，默认值为 3。

`lcd_panel_factory.c` 实现各 LCD 驱动的 factory override，
`include/waveshare_lcd.h` 提供 Board Manager 集成所需的公共头文件引用。

## 注意事项

- 板卡目录中的 GPIO、外设和显示时序应以目标硬件版本及其 Board Manager 配置为准。
- amend 只应应用到兼容的基础板卡；不同屏幕的分辨率、驱动和 DSI 速率不能混用。
- 本仓库只提供配置和驱动集成代码；实际工程仍需准备对应的 ESP-IDF、Board Manager
  及 LCD 驱动依赖。

# ESP32-P4-WIFI6-DB 板卡定义

[English](README.md)

本目录提供 Waveshare `ESP32-P4-WIFI6-DB` 的 ESP Board Manager 板卡配置。
板卡芯片为 `esp32p4`，配置版本为 `1.0.0`。

## 默认配置

- LCD：JD9365 MIPI-DSI，800x1280，RGB565，2-lane，1500 Mbps；
- 触摸：GT911，触摸范围 800x1280，I2C 地址候选为 `0xBA` 和 `0x28`；
- 音频：ES8311，I2C 地址 `0x30`，I2S port 0，采样率 48 kHz；
- 摄像头：CSI 摄像头资源，`sdkconfig.defaults.board` 选择 OV5647，
  RAW8 800x1280 50 fps；
- 存储：SDMMC 4-bit，挂载点 `/sdcard`，默认频率为 `SDMMC_FREQ_HIGHSPEED`；
- MIPI 电源：LDO channel 3，输出 2500 mV；
- LCD 背光：共享 I2C 总线上的 `custom` 设备 `lcd_brightness`，地址 `0x45`，
  寄存器 `0x96`，启动时亮度 100%（`CONFIG_BSP_LCD_BRIGHTNESS_USE_I2C=y`）；
- LCD 默认使用 3 个 DPI frame buffer，amend 默认关闭。

## 外设映射

| 外设 | 配置 |
| --- | --- |
| I2C | port 1，SDA GPIO7，SCL GPIO8 |
| I2S | port 0，MCLK/BCLK/WS/DOUT/DIN 为 GPIO13/12/10/9/11 |
| 功放控制 | GPIO53 |
| SDMMC | CLK/CMD/D0-D3 为 GPIO43/44/39/40/41/42 |
| SD 卡电源 | GPIO45 |
| MIPI-DSI | bus 0，2 lanes，1500 Mbps |

Board Manager 没有对应设备/外设类型、因此未建模的部分：ESP32-C5 Hosted Wi-Fi 的
SDIO 接线（CLK GPIO18、CMD GPIO19、D0-D3 GPIO14-17、复位 GPIO54）。该链路由应用通过
`esp_hosted` / `esp_wifi_remote` 直接配置。

## 文件说明

- `board_info.yaml`：板卡基本信息；
- `board_devices.yaml`：音频、LCD、背光、触摸、摄像头、SD 卡和 SD 卡电源控制设备；
- `board_peripherals.yaml`：总线、GPIO、LDO 和 MIPI-DSI 配置；
- `setup_device.c`：LCD 和 GT911 touch factory hook；
- `lcd_brightness.c` / `lcd_brightness.h`：I2C 背光控制器的 `custom` 设备驱动，
  提供 `custom_lcd_brightness_set()`；
- `sdkconfig.defaults.board`：默认 Flash、PSRAM、LCD、背光和 OV5647 配置。

## 使用

在 ESP-IDF 工程目录下执行，并用 `-c` 指向本仓库的 `components/waveshare_bmgr`
目录（板卡包未发布到组件注册表，不会被自动发现）：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr
```

如需使用其他 Waveshare DSI 触摸屏，加上 `-a` 指向 `amends/` 下对应的 LCD amend。
可用的屏幕配置见 [`../amends/README_zh.md`](../amends/README_zh.md)，完整工程见
[`examples/esp_idf/09_board_manager`](../../../examples/esp_idf/09_board_manager/README_zh.md)。

LCD、触摸和摄像头参数应与实际硬件版本保持一致；本 README 仅描述当前仓库
中的 Board Manager 配置，不替代硬件验证。

# 出厂固件源码（ESP-Brookesia）

[English](README.md) | [简体中文](README_ZH.md)

## 功能

运行 ESP-Brookesia 桌面，集成画板、频谱分析、音乐播放、摄像头、视频播放、设置和 GPIO 监测等板测应用。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 微雪 MIPI-DSI 触摸屏（5 / 7 / 8 / 10.1 寸）

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

屏幕启动进入 ESP-Brookesia 桌面，可点击运行各项硬件测试应用。

## 常见问题

编译前请在 `idf.py menuconfig` 中选择与实物匹配的屏幕型号（`CONFIG_BSP_LCD_TYPE_*`）。

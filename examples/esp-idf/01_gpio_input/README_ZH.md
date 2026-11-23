# 01_gpio_input

[English](README.md) | [简体中文](README_ZH.md)

## 功能

读取扩展排针上的 GPIO 输入电平，并通过 LVGL 在 MIPI-DSI 屏幕上实时显示各引脚状态。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 微雪 MIPI-DSI 触摸屏（5 / 7 / 8 / 10.1 寸）

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

屏幕显示 GPIO 状态列表，外部拉高或拉低对应扩展引脚时界面状态同步更新。

## 常见问题

编译前请在 `idf.py menuconfig` 的 `Board Support Package` 中选择与实物匹配的屏幕型号。

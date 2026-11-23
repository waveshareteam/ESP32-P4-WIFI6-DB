# 04_mipi_dsi

[English](README.md) | [简体中文](README_ZH.md)

## 功能

初始化 MIPI-DSI 屏幕与背光控制，并通过 LVGL 显示测试界面。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 微雪 MIPI-DSI 触摸屏（5 / 7 / 8 / 10.1 寸）

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

屏幕背光点亮并正常显示 LVGL 测试画面。

## 常见问题

编译前请在 `idf.py menuconfig` 的 `Board Support Package` 中选择与实物匹配的屏幕型号。

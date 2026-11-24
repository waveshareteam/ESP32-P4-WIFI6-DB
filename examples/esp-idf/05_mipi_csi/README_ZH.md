# 05_mipi_csi

[English](README.md) | [简体中文](README_ZH.md)

## 功能

采集 OV5647 MIPI-CSI 摄像头画面，并在 MIPI-DSI 屏幕上实时预览。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- OV5647 MIPI-CSI 摄像头模组与微雪 MIPI-DSI 屏幕

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

MIPI-DSI 屏幕上实时显示摄像头采集画面。

## 常见问题

上电前请确认摄像头和屏幕排线方向插接正确。

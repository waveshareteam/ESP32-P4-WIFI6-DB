# 09_board_manager

[English](README.md) | [简体中文](README_ZH.md)

## 功能

演示使用托管组件 `waveshare/waveshare_boards` 通过 ESP Board Manager 完成板级初始化与外设测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 按所选测试项选配微雪 MIPI-DSI 屏幕、microSD 卡或 OV5647 摄像头

## 怎么运行

```bash
python -m pip install --upgrade esp-bmgr-assist
idf.py bmgr -b esp32_p4_wifi6_db
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口控制台完成板级外设初始化并输出对应测试项的运行结果。

## 常见问题

执行 `idf.py build` 前须先运行 `idf.py bmgr -b esp32_p4_wifi6_db` 生成板级代码。

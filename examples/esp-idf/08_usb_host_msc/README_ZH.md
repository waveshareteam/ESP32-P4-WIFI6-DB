# 08_usb_host_msc

[English](README.md) | [简体中文](README_ZH.md)

## 功能

作为 USB Host 挂载外接 U 盘并执行文件读写测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- U 盘及 USB-C OTG 转接线

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口监视器识别到 U 盘挂载至 `/usb` 并输出文件读写测试结果。

## 常见问题

请将 U 盘接入 USB-C OTG 接口，而非串口调试口。

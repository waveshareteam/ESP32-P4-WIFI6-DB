# 00_board_check

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在串口打印芯片型号、Flash 容量、PSRAM 容量和基础运行信息，用于验证开发环境与板卡连接。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口监视器输出芯片版本、主频、32 MB Flash 及 PSRAM 容量信息。

## 常见问题

暂无。

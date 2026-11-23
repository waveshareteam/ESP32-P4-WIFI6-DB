# 02_i2c_console

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供交互式串口控制台，用于扫描共享 I2C 总线并探测已挂载的从设备地址。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口控制台输出扫描到的 I2C 设备地址（如 ES8311，接有触摸屏时可检测到 GT911）。

## 常见问题

暂无。

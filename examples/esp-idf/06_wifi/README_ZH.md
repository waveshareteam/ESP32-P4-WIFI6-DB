# 06_wifi

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过板载 ESP32-C5 协处理器及 ESP-Hosted（SDIO）连接指定的 Wi-Fi 路由器。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口监视器输出 ESP-Hosted 初始化、Wi-Fi 连接成功及获取到的 IP 地址。

## 常见问题

编译前请在 `idf.py menuconfig` 的 `Example Configuration` 中配置 Wi-Fi 名称和密码，并确保 ESP32-C5 协处理器已烧录匹配的 ESP-Hosted 从机固件。

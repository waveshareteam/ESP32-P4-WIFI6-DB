# 03_sdmmc

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 4 位 SDMMC 接口挂载 microSD 卡并执行文件读写测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- FAT32 格式的 microSD 卡

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

串口监视器打印卡容量信息，完成测试文件写入、回读校验并卸载。

## 常见问题

请确保在复位开发板前已插入 microSD 卡。

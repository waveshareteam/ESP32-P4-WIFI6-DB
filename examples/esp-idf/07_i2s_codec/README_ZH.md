# 07_i2s_codec

[English](README.md) | [简体中文](README_ZH.md)

## 功能

演示基于 I2S 的 ES8311 音频编解码器播放、模拟麦克风录音与回环测试。

## 硬件需求

- ESP32-P4-WIFI6-DB 开发板
- 接在板载扬声器接口上的喇叭

## 怎么运行

```bash
idf.py set-target esp32p4 build flash monitor
```

## 预期现象

喇叭播放测试音频，串口监视器打印编解码器初始化与运行状态。

## 常见问题

暂无。

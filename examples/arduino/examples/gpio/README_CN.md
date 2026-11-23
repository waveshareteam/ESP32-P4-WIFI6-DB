# GPIO 控制台示例

[English Version](./README.md)

本示例使用 Arduino-ESP32 的 `Console` 库，通过串口监视器交互式地配置和测试 GPIO。
它基于 Arduino 核心的 `ConsoleGPIO` 示例改写。

## 前置条件

- 在 Arduino IDE 中选择 `Waveshare ESP32-P4-WIFI6-DB`。
- 使用 USB 串口，并把串口监视器波特率设置为 `115200`。
- 确认 Arduino-ESP32 的 `Console` 库可用，它随 Arduino 核心一起安装。

## 使用方法

上传完成后打开串口监视器，把行结束方式设为 `Newline`，输入 `help` 可以列出所有
命令。

```text
gpio read <pin>
gpio write <pin> <0|1>
gpio mode <pin> <in|out|in_pu|in_pd>
```

示例：

```text
gpio mode 2 out
gpio write 2 0
gpio write 2 1
gpio mode 4 in_pu
gpio read 4
```

## 板级说明

- 本板 variant 没有定义板载 `LED_BUILTIN`；本示例把 GPIO2 和 GPIO3 当作普通排针
  GPIO 处理。
- 板载外设对应的示例或功能正在使用时，不要占用它们保留的 GPIO，包括 MIPI 显示、
  触摸控制器、摄像头、SDMMC、ESP-Hosted SDIO、音频 codec 以及 RS485/RS232
  收发器。
- 先执行 `gpio mode` 再执行 `gpio write`。对配置为输入的引脚写值不是有效的输出
  测试。

## 预期输出

```text
gpio> gpio mode 2 out
GPIO 2 mode set to out
gpio> gpio read 2
GPIO 2 = 0 (LOW)
```

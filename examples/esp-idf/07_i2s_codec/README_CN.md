# ES8311 I2S Codec

[English Version](./README.md)

本示例通过 I2S 持续播放 PCM，或采集麦克风数据后回放，使用本地 ESP32-P4 BSP 提供的 ES8311 播放和单麦克风录音设备。

## 硬件要求

- 默认 BSP/ES8311 路径使用 ESP32-P4-WIFI6-DB 开发板。
- 播放需要扬声器或耳机。
- Echo 模式需要麦克风输入。

## 构建和烧录

```powershell
cd examples\esp_idf\07_i2s_codec
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

将 `COMx` 替换为实际串口号。

## 配置

在 **Example Configuration** 中：

- 选择 `music`，循环播放内置的 `main/canon.pcm`。
- 选择 `echo`，读取麦克风 PCM 并写回输出。
- 通过 **Voice volume** 设置 0～100 的输出音量。
- 通过 **Microphone gain** 设置麦克风增益：0～42 dB，步进 6 dB，默认 24 dB。

Kconfig 默认选择 `music`。两种模式都使用 BSP，不再提供 BSP 开关。
Codec 辅助组件使用 16 kHz、16 bit、双声道 PCM，麦克风增益使用 menuconfig 中的选择。
`bsp_board_extra.h` 中的 `CODEC_DEFAULT_*` 定义 Codec 默认参数；
`example_config.h` 只定义 Echo 缓冲区大小，单位为字节。

BSP 将 ES8311 的 `no_dac_ref` 设为 `true`。双声道采集时，右声道保持空，
不会填入 DAC 输出参考信号。

当前 ESP32-P4 I2C/I2S 引脚：

| 信号 | GPIO |
| --- | ---: |
| I2C SDA | 7 |
| I2C SCL | 8 |
| I2S MCLK | 13 |
| I2S BCLK | 12 |
| I2S LRCK | 10 |
| I2S DOUT → ES8311 DSDIN | 9 |
| ES8311 ASDOUT → I2S DIN | 11 |

本地 BSP 将 GPIO53 分配为功放使能。

## 预期行为

- 初始化成功后打印 `codec init success`。
- Music 模式会持续打印写入的 PCM 字节数。
- Echo 模式先打印 `[echo] Echo start`，然后持续采集并回放 PCM。

初始化日志只能证明软件路径已经打开，不能证明左右声道存在有效麦克风数据，也不能证明模拟输出已经有声。

## 排障

- 检查 Codec 供电、MCLK/BCLK/LRCK 波形、I2C 控制、I2S 接线和当前扬声器/耳机路由。
- Echo 模式先分别统计左右声道 PCM 的 peak 或 RMS，再判断 Codec 或模拟硬件故障。
- 使用 [02_i2c_console](../02_i2c_console/) 确认 GPIO7/GPIO8
  上的目标 Codec 能正常应答。

本文已按源码完成静态核对；音频播放和录音仍为待实测。

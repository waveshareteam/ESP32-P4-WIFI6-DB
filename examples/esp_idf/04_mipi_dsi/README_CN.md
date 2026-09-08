# 显示色条

[English Version](./README.md)

在受支持的 LCD 面板上显示色条。这是本仓库中最简单的 ESP-IDF 显示 bring-up 示例。

## 难度

中级。

## 硬件要求

- ESP32-P4-WIFI6-DB 开发板。
- 连接到显示接口的 BSP 支持的 MIPI-DSI 屏幕，并在 `menuconfig` 中选择
  匹配的 5、7、8 或 10.1 英寸型号。

该示例使用本地 `esp32_p4_wifi6_db` BSP 组件。它是本板很合适的第一个显示测试，因为应用不会创建 LVGL UI；它只初始化面板，并请求 DPI 面板驱动生成硬件垂直色条图案。

## 构建和烧录

```powershell
cd examples\esp_idf\06_mipi_dsi_test
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

## 预期行为

串口日志应显示：

```text
Initialize LCD device
Show color bar pattern drawn by hardware
```

LCD 应点亮背光并显示垂直色条。

## 排障

- 确认显示面板型号和接口。
- 如果面板路径需要 PSRAM，确认 PSRAM 已启用。
- 如果日志正常但背光不亮，检查 GPIO7/GPIO8 上的共享 I2C 总线，以及
  `0x45` 设备的 `0x96` 寄存器写入。
- 本板没有 LCD 复位 GPIO；请检查 DSI 供电、lane 配置、FPC 方向和面板初始化。
- 在运行 [07_mipi_csi_test](../07_mipi_csi_test/) 前先运行此示例，把面板
  bring-up 与摄像头处理分开测试。

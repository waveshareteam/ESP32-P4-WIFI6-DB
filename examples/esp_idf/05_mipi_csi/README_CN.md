| 支持目标 | ESP32-P4 |
| -------- | -------- |

[English Version](./README.md)

# Video LCD Display

该示例基于 [esp_video](https://github.com/espressif/esp-video-components/tree/master/esp_video) 组件，演示如何把摄像头图像显示到 LCD 屏幕上。应用会初始化本地 ESP32-P4-WIFI6-DB BSP 显示，打开 MIPI-CSI video device，使用 PPA 进行缩放/旋转/镜像处理，并通过 LVGL adapter dummy-draw 路径把每帧 blit 到 LCD frame buffer。

## ESP-IDF 要求

- 该示例支持 ESP-IDF v5.5 及更高版本。
- 工程依赖 `esp_video` 和本地 `esp32_p4_wifi6_db` BSP。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**先 [构建第一个工程](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，熟悉 ESP-IDF 并确保环境正确。

### 前置条件

* ESP32-P4-WIFI6-DB 开发板。
* 在 `menuconfig` 中选择 BSP 支持的 5、7、8 或 10.1 英寸 MIPI-DSI
  屏幕。LCD 没有复位 GPIO；背光通过共享 I2C 总线的 `0x45` 地址、
  `0x96` 寄存器控制。
* GT911 触摸与其他设备共享 GPIO7/GPIO8 I2C；`bsp_display_start()` 会在复位/中断脚均为 NC 的配置下注册触摸。
* 一个受 `esp_video` 支持的 MIPI-CSI 摄像头传感器。默认 `sdkconfig.defaults` 选择 OV5647，MIPI RAW8 `800 x 1280`，50 FPS。
* 用于供电和烧录的 USB-C 线。
* 将 LCD FPC 接到 MIPI-DSI，将 Camera FPC 接到 MIPI-CSI，再连接 USB-C
  进行供电、烧录和串口监视。

### 配置工程

运行 `idf.py menuconfig`，配置 BSP 显示、摄像头传感器和视频流水线选项。

MIPI-CSI 摄像头默认 ESP32-P4 SCCB/I2C 引脚：

| 信号 | 默认 GPIO |
| --- | --- |
| SCL | GPIO8 |
| SDA | GPIO7 |

在 `Espressif Camera Sensors` 配置菜单中，选择与你的硬件匹配的摄像头传感器。当前默认值为：

```text
Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] OV5647  ---->
            Default format select for MIPI  --->
                (X) RAW8 800x1280 50fps, MIPI input
```

如果使用 SC2336 或其他传感器，请把传感器选择和输出格式改成与摄像头模块匹配。

### 构建和烧录

构建工程并烧录到开发板，然后运行监视工具查看串口输出（将 `PORT` 替换为开发板串口名）：

```bash
cd examples/esp_idf/07_mipi_csi_test
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

输入 `Ctrl-]` 退出串口监视器。

完整配置和使用 ESP-IDF 构建工程的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

### 预期行为

显示背光点亮，GT911 注册成功，LCD 显示实时摄像头图像。日志会打印 video driver 版本、设备名、总线信息，以及检测到的帧宽高。帧分配在 PSRAM 中，并复制到 `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3` 配置的三重 LCD frame buffer。

### 排障

- 先运行 [06_mipi_dsi_test](../06_mipi_dsi_test/) 验证 LCD 路径。
- 如果出现 `video cam open failed`，检查摄像头 FPC 方向、传感器供电、SCCB/I2C 引脚和已选传感器型号。
- 确认 PSRAM 已启用且稳定；摄像头 buffer 会从 PSRAM 分配。
- 如果图像裁剪、镜像或旋转不正确，请调整传感器输出格式或 `main/main.c` 中的 PPA 操作。

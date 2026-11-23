# GPIO 输入输出监控

[English Version](./README.md)

这个示例使用 LVGL 在屏幕上以自适应网格显示选定 GPIO 的实时电平。

调用 `bsp_display_start_with_config()` 时，BSP 会在 GPIO7/GPIO8 共享 I2C
总线上初始化 GT911，并注册为 LVGL 输入设备。触摸复位和中断脚未连接，
LVGL adapter 通过 I2C 轮询触摸数据。

## 使用方式

例程通过 `bsp_get_header_gpios()` 从当前 BSP 自动获取扩展排针上的 GPIO 列表，不再在 `main.c` 中硬编码板型宏和 GPIO 数组。

ESP32-P4-WIFI6-DB BSP 当前按以下顺序提供排针 GPIO：
`52, 51, 31, 30, 29, 28, 50, 49, 5, 4, 3, 2, 24, 25, 20, 21, 22, 23,
26, 27, 32, 33, 46, 47, 48`。

在 `gpio_monitor_config_t` 中设置初始模式：

```c
.initial_mode = GPIO_MODE_INPUT,
```

顶部的 `lv_switch` 用于切换运行模式：关闭时为 `GPIO_MODE_INPUT`，打开时为 `GPIO_MODE_OUTPUT`。所有监控 GPIO 同时切换。输入模式下卡片显示 `GPIOx IN:0/1`，输出模式下显示 `GPIOx OUT:0/1`；输出模式下点击卡片可以翻转输出电平。高电平使用绿色卡片，低电平使用深色卡片；GPIO 数量较多时，网格支持上下滚动。

## 界面布局

- 列数根据 GPIO 总数和屏幕宽度自动选择。
- 卡片宽度由网格列数和屏幕宽度决定。
- 卡片高度由行数和屏幕高度决定；数量过多时保持最小高度并滚动。
- 高电平使用绿色卡片，低电平使用深色卡片。

## 接口

`main/gpio_monitor.h` 提供：

- `gpio_monitor_init()`：根据用户传入的 GPIO 配置完成 GPIO 初始化。应在 BSP 显示启动前调用。
- `gpio_monitor_start_ui()`：在 BSP 显示启动后创建 LVGL 界面。
- `gpio_monitor_set_output_enabled()`：通过代码切换所有 GPIO 的 `GPIO_MODE_INPUT`/`GPIO_MODE_OUTPUT`；界面顶部开关使用的就是这个接口。
- `gpio_monitor_set_level()`：给已经配置为输出的 GPIO 设置电平。

当前示例默认以 `GPIO_MODE_INPUT` 初始化。切换到输出模式前，请确认这些扩展
排针上没有连接会与输出电平冲突的外部电路。

# GPIO Input/Output Monitor

[中文版本](./README_CN.md)

This example uses LVGL to show the live level of selected GPIOs in a responsive grid.

The BSP initializes the GT911 touch controller on the shared GPIO7/GPIO8 I2C
bus when `bsp_display_start_with_config()` is called. Touch reset and interrupt
are not connected; the LVGL adapter polls touch data over I2C.

The example obtains the expansion-header GPIO list from the active BSP through `bsp_get_header_gpios()`. The top `lv_switch` changes all monitored GPIOs between `GPIO_MODE_INPUT` and `GPIO_MODE_OUTPUT`; output-mode cards can be clicked to toggle their output level.

Call `gpio_monitor_init()` before starting the BSP display, then call `gpio_monitor_start_ui()` after the display is ready. The grid calculates its columns and card sizes from the GPIO count and the active display resolution. If the list is larger than the screen, the grid can be scrolled vertically.

The ESP32-P4-WIFI6-DB BSP exposes GPIOs `52, 51, 31, 30, 29, 28, 50, 49,
5, 4, 3, 2, 24, 25, 20, 21, 22, 23, 26, 27, 32, 33, 46, 47, 48`, in that
order.

The example currently starts in `GPIO_MODE_INPUT`. Confirm that nothing
external is connected to these expansion pins before switching them to output
mode.

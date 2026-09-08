idf_component_register(
    SRCS src/gpio_monitor.c
    INCLUDE_DIRS include
    REQUIRES esp_driver_gpio lvgl__lvgl)

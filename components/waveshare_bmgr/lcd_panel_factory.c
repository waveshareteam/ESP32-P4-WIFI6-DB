/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "waveshare_lcd.h"

#if CONFIG_BSP_LCD_TYPE_AMENDS

#if CONFIG_BSP_LCD_TYPE_800_800_3_4_INCH || \
    CONFIG_BSP_LCD_TYPE_720_720_4_INCH || \
    CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A || \
    CONFIG_BSP_LCD_TYPE_720_1280_9_INCH_B || \
    CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A || \
    CONFIG_BSP_LCD_TYPE_720_1280_10_1_INCH_B
#include "esp_lcd_jd9365.h"
#elif CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A
#include "esp_lcd_ili9881c.h"
#elif CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A
#include "esp_lcd_hx8394.h"
#elif CONFIG_BSP_LCD_TYPE_480_1920_8_8_INCH_A
#include "esp_lcd_ota7290b.h"
#elif CONFIG_BSP_LCD_TYPE_1024_600_7_INCH_C
#include "esp_lcd_ek79007.h"
#elif CONFIG_BSP_LCD_TYPE_480_800_4_INCH_A || CONFIG_BSP_LCD_TYPE_480_800_4_3_INCH_A
#include "esp_lcd_st7701.h"
#endif

static const char *TAG = "WAVESHARE_DISPLAY_DSI";

#if CONFIG_BSP_LCD_TYPE_800_800_3_4_INCH || CONFIG_BSP_LCD_TYPE_720_720_4_INCH || CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A || \
    CONFIG_BSP_LCD_TYPE_720_1280_9_INCH_B || CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A || CONFIG_BSP_LCD_TYPE_720_1280_10_1_INCH_B
esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BSP_LCD_TYPE_720_1280_9_INCH_B || CONFIG_BSP_LCD_TYPE_720_1280_10_1_INCH_B
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_720_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_720_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
#elif CONFIG_BSP_LCD_TYPE_720_720_4_INCH
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_720_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_720_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
#elif CONFIG_BSP_LCD_TYPE_800_800_3_4_INCH
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_800_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_800_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
#else
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
#endif

    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;

    jd9365_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
            .lane_num = 2,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_jd9365(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#if CONFIG_BSP_LCD_TYPE_480_800_4_INCH_A || CONFIG_BSP_LCD_TYPE_480_800_4_3_INCH_A
/* These two profiles are the panel-specific sequences used by the reference
 * Waveshare BSP. The ST7701 driver's built-in defaults are not interchangeable
 * between the 4-inch and 4.3-inch panels. */
#if CONFIG_BSP_LCD_TYPE_480_800_4_3_INCH_A
static const st7701_lcd_init_cmd_t st7701_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x17, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x94, 0x0E, 0x10, 0x05, 0x0B, 0x09, 0x08, 0x26, 0x04, 0x52, 0x10, 0x69, 0x6B, 0x69}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0xD2, 0x98, 0x0C, 0x92, 0x07, 0x09, 0x08, 0x07, 0x25, 0x02, 0x0E, 0x0C, 0x6E, 0x78, 0x55}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x4E}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};
#else
static const st7701_lcd_init_cmd_t st7701_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0C, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x01, 0x07}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0xCD, 0x18, 0x1F, 0x0F, 0x13, 0x08, 0x09, 0x08, 0x08, 0x24, 0x03, 0x10, 0x0E, 0x21, 0x24, 0x0B}, 16, 0},
    {0xB1, (uint8_t[]){0xC3, 0x0F, 0x18, 0x0B, 0x0F, 0x05, 0x09, 0x09, 0x08, 0x24, 0x06, 0x13, 0x13, 0x28, 0x2D, 0x15}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x3F}, 1, 0},
    {0xB2, (uint8_t[]){0x82}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x45}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x3E}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 100},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20}, 11, 0},
    {0xE2, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x00}, 4, 0},
    {0xE4, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE5, (uint8_t[]){0x04, 0x34, 0x9A, 0xA0, 0x06, 0x34, 0x9A, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x00}, 4, 0},
    {0xE7, (uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE8, (uint8_t[]){0x05, 0x34, 0x9A, 0xA0, 0x07, 0x34, 0x9A, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 200},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};
#endif
#endif

#if CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A
esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = ILI9881C_720_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = ILI9881C_720_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    ili9881c_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
            .lane_num = 2,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_ili9881c(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#if CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A
esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = HX8394_720_1280_PANEL_30HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = HX8394_720_1280_PANEL_30HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    hx8394_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
            .lane_num = 2,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_hx8394(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#if CONFIG_BSP_LCD_TYPE_480_1920_8_8_INCH_A
esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = OTA7290B_480_1920_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = OTA7290B_480_1920_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    ota7290b_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_ota7290b(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#if CONFIG_BSP_LCD_TYPE_1024_600_7_INCH_C
esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle, dev_display_lcd_config_t *lcd_cfg, dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
#endif
#else
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif
#endif

    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
            .lane_num = 2,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_ek79007(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#if CONFIG_BSP_LCD_TYPE_480_800_4_INCH_A || CONFIG_BSP_LCD_TYPE_480_800_4_3_INCH_A
esp_err_t lcd_dsi_panel_factory_entry_t(
    esp_lcd_dsi_bus_handle_t dsi_handle,
    dev_display_lcd_config_t *lcd_cfg,
    dev_display_lcd_handles_t *lcd_handles)
{
    ESP_LOGI(TAG, "Strong LCD factory override is active (waveshare_lcd)");

    if (dsi_handle == NULL || lcd_cfg == NULL || lcd_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_lcd_dpi_panel_config_t dpi_config = lcd_cfg->sub_cfg.dsi.dpi_config;
    dpi_config.dpi_clock_freq_mhz = 30;
    dpi_config.video_timing.h_size = 480;
    dpi_config.video_timing.v_size = 800;
    dpi_config.video_timing.hsync_back_porch = 42;
    dpi_config.video_timing.hsync_pulse_width = 12;
    dpi_config.video_timing.hsync_front_porch = 42;
    dpi_config.video_timing.vsync_back_porch = 2;
    dpi_config.video_timing.vsync_pulse_width = 8;
    dpi_config.video_timing.vsync_front_porch = 60;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB888;
#else
    dpi_config.in_color_format = LCD_COLOR_FMT_RGB565;
#endif
#else
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    dpi_config.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB888;
#else
    dpi_config.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
#endif
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;
    st7701_vendor_config_t vendor_config = {
        .init_cmds = st7701_init_cmds,
        .init_cmds_size = sizeof(st7701_init_cmds) / sizeof(st7701_init_cmds[0]),
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &dpi_config,
        },
        .flags = {
            .use_mipi_interface = 1,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .bits_per_pixel = 24,
#else
        .bits_per_pixel = 16,
#endif
        .data_endian = lcd_cfg->data_endian,
        .flags = {
            .reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        },
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_st7701(
        lcd_handles->io_handle,
        &panel_config,
        &lcd_handles->panel_handle);
}
#endif

#endif

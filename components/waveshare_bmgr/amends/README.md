# ESP32-P4-WIFI6-DB LCD Amends

[中文](README_zh.md)

Each directory below is a complete Board Manager amend for one Waveshare DSI
display, applicable to the [`esp32_p4_wifi6_db`](../esp32_p4_wifi6_db)
board. The amend updates the DSI bus bitrate and lane count, LCD DPI timing,
touch resolution, and `waveshare_lcd` factory selection together.

Select an amend from the ESP-IDF project directory, together with the `-c`
path that makes this board package discoverable:

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr -a <repo>/components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

| Amend | Panel | Resolution | Lanes | Bitrate (Mbps) |
| --- | --- | ---: | ---: | ---: |
| [`lcd_5_dsi_touch_a`](lcd_5_dsi_touch_a) | HX8394 | 720x1280 | 2 | 700 |
| [`lcd_7_dsi_touch_a`](lcd_7_dsi_touch_a) | ILI9881C | 720x1280 | 2 | 1000 |
| [`lcd_8_dsi_touch_a`](lcd_8_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 |
| [`lcd_10_1_dsi_touch_a`](lcd_10_1_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 (board default) |

Each profile is defined by the `panel.yaml`, `board_amend.yaml`, and
`sdkconfig.defaults.board` files in its directory. `Kconfig` and
`lcd_panel_factory.c` in the parent `waveshare_bmgr` component support additional
Waveshare panel types shared with other boards, but only the four amends above
are provided for this board in this repository. Add further amends only after
their driver and bus parameters are confirmed against actual hardware.

# ESP32-P4-WIFI6-DB LCD Amend

[English](README.md)

下面每个目录都是一块 Waveshare DSI 显示屏的完整 Board Manager amend，适用于
[`esp32_p4_wifi6_db`](../esp32_p4_wifi6_db) 板卡。amend 会
同时更新 DSI 总线速率和 lane 数量、LCD DPI 时序、触摸分辨率以及
`waveshare_lcd` factory 选择。

在 ESP-IDF 工程目录下选择一个 amend，并用 `-c` 指定本板卡包的路径使其可被发现：

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c <repo>/components/waveshare_bmgr -a <repo>/components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

| Amend | 面板 | 分辨率 | Lanes | 速率（Mbps） |
| --- | --- | ---: | ---: | ---: |
| [`lcd_5_dsi_touch_a`](lcd_5_dsi_touch_a) | HX8394 | 720x1280 | 2 | 700 |
| [`lcd_7_dsi_touch_a`](lcd_7_dsi_touch_a) | ILI9881C | 720x1280 | 2 | 1000 |
| [`lcd_8_dsi_touch_a`](lcd_8_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500 |
| [`lcd_10_1_dsi_touch_a`](lcd_10_1_dsi_touch_a) | JD9365 | 800x1280 | 2 | 1500（板卡默认） |

每个配置由所在目录中的 `panel.yaml`、`board_amend.yaml` 和
`sdkconfig.defaults.board` 定义。上级 `waveshare_bmgr` 组件的 `Kconfig` 和
`lcd_panel_factory.c` 还支持与其他板卡共用的更多 Waveshare 屏幕型号，但本仓库
针对该板卡只提供了以上四种 amend。只有在驱动和总线参数经过实际硬件确认后，
才应继续添加新的 amend。

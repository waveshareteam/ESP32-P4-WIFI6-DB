# Display Color Bar

[中文版本](./README_CN.md)

Display color bars on a supported LCD panel. This is the simplest ESP-IDF
display bring-up example in the repository.

## Difficulty

Intermediate.

## Hardware Required

- ESP32-P4-WIFI6-DB board.
- One BSP-supported MIPI-DSI panel connected to the display interface. Select
  the matching 5, 7, 8, or 10.1-inch panel in `menuconfig`.

This example uses the local `esp32_p4_wifi6_db` BSP component. It is a good
first display test for this board because the application does
not create an LVGL UI; it only initializes the panel and asks the DPI panel
driver to generate a hardware vertical color-bar pattern.

## Build and Flash

```powershell
cd examples\esp_idf\04_mipi_dsi
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p PORT flash monitor
```

## Expected Behavior

The serial log should show:

```text
Initialize LCD device
Show color bar pattern drawn by hardware
```

The LCD should turn on its backlight and show vertical color bars.

## Troubleshooting

- Confirm the display panel model and interface.
- Confirm PSRAM is enabled when the panel path needs it.
- Check the shared I2C bus on GPIO7/GPIO8 and I2C device `0x45`, register
  `0x96`, if logs look correct but the backlight stays dark.
- This board has no LCD reset GPIO; verify DSI power, lane configuration, FPC
  orientation, and panel initialization instead.
- Run this example before [05_mipi_csi](../05_mipi_csi/) so panel
  bring-up is tested separately from camera processing.

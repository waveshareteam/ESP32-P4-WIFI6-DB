# mipi_csi

[中文版本](./README_CN.md)

## Purpose

Initializes the OV5647 MIPI-CSI camera and the MIPI-DSI LCD of the
ESP32-P4-WIFI6-DB and shows the live camera image on the LCD. The LCD entry
point supports JD9365, HX8394 and ILI9881C. `lcd_panel.h` currently selects
JD9365 by default, and `esp_lcd_jd9365.h` defaults to the 10.1-inch A panel
(800×1280, RGB565, two DSI lanes, 1500 Mbps/lane).

## Prerequisites

- Use Arduino-ESP32 `3.3.11` and its matching prebuilt ESP32-P4 libraries.
- `ESP_Video` must have the MIPI-CSI video device enabled:
  `CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE=y`. The repository's `ci.yml`
  declares this requirement; a custom Arduino-ESP32 build must keep the same
  setting.
- Connect the OV5647 camera and the actual LCD panel, and confirm the camera
  power supply, the MIPI cable and the shared I2C/SCCB wiring.

## Usage

1. Connect the OV5647 camera and the actual LCD panel to the board.
2. Use Arduino-ESP32 `3.3.11` and select `Waveshare ESP32-P4-WIFI6-DB`.
3. Select the attached panel in `lcd_panel.h`: `LCD_PANEL_TYPE` can be set to
   `lcd_panel_type_jd9365`, `lcd_panel_type_hx8394` or
   `lcd_panel_type_ili9881c`. Keep the defaults `lcd_panel_type_jd9365` and
   `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A=1` in `esp_lcd_jd9365.h`.
4. Open `mipi_csi.ino`, compile and upload.
5. Open the serial monitor at `115200` baud.

## LCD and I2C

The local Arduino-ESP32 version is `3.3.11`, whose prebuilt ESP32-P4 package is
based on ESP-IDF `v5.5.5`. `ESP32_Display_Panel 1.0.4` uses the legacy I2C
driver while `ESP_Video 3.3.11` uses the new `i2c_master` driver. When both
enter the same program, the following can be triggered even if no LCD I2C
function is called:

```text
CONFLICT! driver_ng is not allowed to be used with this old driver
```

This example therefore does not use `ESP32_Display_Panel`. It maintains a local
`lcd_panel.cpp/.h` plus three panel driver files and calls the native `esp_lcd`
MIPI-DSI API directly. The board's LCD has no reset GPIO; the backlight is
controlled through device `0x45`, register `0x96`, on the shared I2C bus
GPIO7/GPIO8.

The camera and the backlight share the new `i2c_master` bus handle managed by
Arduino `Wire1`; the legacy I2C API is not used. This requires no changes to
Arduino-ESP32 or to the official library sources. If an official library later
provides an implementation compatible with both I2C drivers, switching back to
a generic LCD library can be reconsidered.

## Camera library versions and logs

This example uses `ESP_Video 3.3.11` as bundled with Arduino-ESP32 `3.3.11`.
The underlying prebuilt component versions are `esp_video 2.3.0`,
`esp_cam_sensor 2.3.0` and `esp_sccb_intf 0.0.8`. Together they handle MIPI-CSI
initialization, SCCB communication, sensor driver registration and video device
access.

`esp_video` currently probes each compiled-in sensor driver in turn during
initialization. You may therefore see probe failure logs for drivers such as
`imx500`, `os04c10` or `ov2710`; this is only the auto-detection process. The
camera used by this example has started only once the OV5647 `PID=0x5647`
detection log appears and `OV5647 direct display started` is printed.

## Board parameters

- I2C/SCCB: SDA=`GPIO7`, SCL=`GPIO8`, I2C controller `1`.
- LCD: JD9365 is 800×1280, 2 lanes, 1500 Mbps/lane; HX8394 is 720×1280,
  2 lanes, 700 Mbps/lane; ILI9881C is 720×1280, 2 lanes, 1000 Mbps/lane.
- LCD: all three panels use RGB565 and PHY LDO3/2500 mV; the current frame
  buffer count is 3.
- LCD: no dedicated reset GPIO; the backlight is I2C `0x45`, register `0x96`.
- Camera: OV5647 MIPI-CSI; XCLK and reset pins are NC and are managed by
  `ESP_Video`.

## Runtime logs

During initialization you may see probe failure logs from other built-in sensor
drivers. These come from the auto-detection process and do not necessarily
indicate an OV5647 failure. The key log for a successful start is:

```text
OV5647 direct display started
```

If that log is missing, check the I2C/SCCB bus, the LCD initialization, the
MIPI-CSI configuration and the camera capture device, in that order. This
example does not use `ESP32_Display_Panel`; it builds the new `i2c_master` bus
through `Wire1` and then shares the bus handle with the camera and the LCD
backlight.

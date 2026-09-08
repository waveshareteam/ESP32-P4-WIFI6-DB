| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

[中文版本](./README_CN.md)

# Video LCD Display

This example is based on the [esp_video](https://github.com/espressif/esp-video-components/tree/master/esp_video) component and demonstrates how to display images from the camera on an LCD screen. The application initializes the local ESP32-P4-WIFI6-DB BSP display, opens the MIPI-CSI video device, uses PPA scale/rotate/mirror processing, and blits each frame into the LCD frame buffer through the LVGL adapter dummy-draw path.

## ESP-IDF Required

- This example supports ESP-IDF v5.5 and later.
- The project depends on `esp_video` and the local `esp32_p4_wifi6_db` BSP.
- Please follow the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to set up the development environment. **We highly recommend** you [Build Your First Project](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#build-your-first-project) to get familiar with ESP-IDF and make sure the environment is set up correctly.

### Prerequisites

* An ESP32-P4-WIFI6-DB board.
* A BSP-supported 5, 7, 8, or 10.1-inch MIPI-DSI panel selected in
  `menuconfig`. There is no LCD reset GPIO; backlight is controlled on the
  shared I2C bus at address `0x45`, register `0x96`.
* The GT911 touch controller shares GPIO7/GPIO8 I2C and is registered by `bsp_display_start()` with reset/interrupt set to NC.
* A MIPI-CSI camera sensor supported by `esp_video`. The default `sdkconfig.defaults` selects OV5647, MIPI RAW8 `800 x 1280` at 50 FPS.
* A USB-C cable for power supply and programming.
* Connect the LCD FPC to MIPI-DSI and the camera FPC to MIPI-CSI, then connect
  USB-C for power, flashing, and the serial monitor.

### Configure the Project

Run `idf.py menuconfig` and configure the BSP display, camera sensor, and video pipeline options.

Default ESP32-P4 SCCB/I2C pins for the MIPI-CSI camera:

| Signal | Default GPIO |
| --- | --- |
| SCL | GPIO8 |
| SDA | GPIO7 |

In the `Espressif Camera Sensors` configuration menu, select the camera sensor that matches your hardware. The current defaults are:

```
Component config  --->
    Espressif Camera Sensors Configurations  --->
        [*] OV5647  ---->
            Default format select for MIPI  --->
                (X) RAW8 800x1280 50fps, MIPI input
```

If you use SC2336 or another sensor, update the sensor selection and output format to match the camera module.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```bash
cd examples/esp_idf/07_mipi_csi_test
idf.py set-target esp32p4
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

### Expected Behavior

The display backlight turns on, the GT911 is registered, and the LCD shows the live camera image. The log prints the video driver version, device name, bus information, and the detected frame width and height. Frames are allocated in PSRAM and copied into the triple LCD frame buffer configured by `CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3`.

### Troubleshooting

- Run [06_mipi_dsi_test](../06_mipi_dsi_test/) first to verify the LCD path.
- Check camera FPC orientation, sensor power, SCCB/I2C pins, and selected sensor model if `video cam open failed` appears.
- Confirm PSRAM is enabled and stable; camera buffers are allocated from PSRAM.
- If the image is cropped, mirrored, or rotated incorrectly, adjust the sensor output format or the PPA operation in `main/main.c`.

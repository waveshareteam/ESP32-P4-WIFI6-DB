# ESP32-P4-WIFI6-DB

English | [简体中文](README_CN.md)

Board support package (BSP), ESP-IDF and Arduino examples, factory firmware,
and schematic for the Waveshare ESP32-P4-WIFI6-DB development board.

## Repository layout

| Path | Contents |
| --- | --- |
| [firmware/esp32_p4_wifi6_db](firmware/esp32_p4_wifi6_db) | ESP-IDF BSP, headers, Kconfig and component manifest |
| [examples/esp_idf](examples/esp_idf) | Peripheral examples and the Brookesia application |
| [examples/Arduino](examples/Arduino/README.md) | Arduino sketches and local board definitions |
| [firmware/factory-firmware](firmware/factory-firmware) | ESP32-P4 images for 5, 7, 8 and 10.1-inch panels, and an ESP32-C5 slave image |
| [hardware/ESP32-P4-WIFI6-DB.pdf](hardware/ESP32-P4-WIFI6-DB.pdf) | Board schematic |

See the [BSP README](firmware/esp32_p4_wifi6_db/README.md) and
[BSP API reference](firmware/esp32_p4_wifi6_db/API.md) for implementation details.

## Board support

The project targets ESP32-P4 and uses an ESP32-C5 wireless coprocessor through
ESP-Hosted over SDIO. Display and camera examples require the corresponding
external peripherals.

| Resource | BSP configuration |
| --- | --- |
| I2C | SDA GPIO7, SCL GPIO8; shared by ES8311, GT911, LCD backlight and camera SCCB |
| Audio | ES8311 with one analog microphone input; I2S MCLK/BCLK/WS/DOUT/DIN on GPIO13/12/10/9/11; amplifier enable GPIO53 |
| microSD | 4-bit SDMMC; CLK/CMD/D0/D1/D2/D3 on GPIO43/44/39/40/41/42; card power enable GPIO45, active low; SDMMC IO power uses LDO channel 4 |
| Display | Two-lane MIPI-DSI; LCD reset GPIO unused; I2C backlight address `0x45`, brightness register `0x96` |
| Touch | GT911 polling; reset and interrupt GPIOs unused |
| Camera | MIPI-CSI capture; camera examples configure OV5647 |
| Storage / USB | SPIFFS and USB Host support |
| RTC | No external RTC support in the BSP (`BSP_CAPS_RTC=0`) |

### ESP-IDF LCD selection

| BSP option | Panel controller | Resolution |
| --- | --- | --- |
| `CONFIG_BSP_LCD_TYPE_720_1280_5_INCH_A` | HX8394, 5-inch | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_720_1280_7_INCH_A` | ILI9881C, 7-inch | 720 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A` | JD9365, 8-inch | 800 × 1280 |
| `CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A` | JD9365, 10.1-inch | 800 × 1280 |

The [BSP Kconfig](firmware/esp32_p4_wifi6_db/Kconfig) defaults to **10.1-inch JD9365**.
[Brookesia defaults](examples/esp_idf/brookesia/sdkconfig.defaults) select
**8-inch JD9365**. Select the attached panel before building. Defaults do not
replace an existing `sdkconfig`; check the project's configuration and generated
`build/config/sdkconfig.h` when determining the effective selection.

## ESP-IDF examples

| Example | Purpose |
| --- | --- |
| [00_board_check](examples/esp_idf/00_board_check) | Chip, Flash, PSRAM and heap information, with a periodic heartbeat |
| [01_gpio_input](examples/esp_idf/01_gpio_input) | Display GPIO input states on the LCD |
| [02_i2c_console](examples/esp_idf/02_i2c_console) | Interactive I2C scan and register read/write console |
| [03_sdmmc](examples/esp_idf/03_sdmmc) | microSD file write, read and rename tests |
| [04_mipi_dsi](examples/esp_idf/04_mipi_dsi) | LCD output test; currently displays white, with an inactive color-bar branch |
| [05_mipi_csi](examples/esp_idf/05_mipi_csi) | Camera preview on the LCD using PPA processing |
| [06_wifi](examples/esp_idf/06_wifi) | Wi-Fi station connection and gateway ping |
| [07_i2s_codec](examples/esp_idf/07_i2s_codec) | ES8311 embedded audio playback or microphone loopback |
| [08_usb_host_msc](examples/esp_idf/08_usb_host_msc) | USB mass-storage file operations and sequential read/write speed tests |
| [brookesia](examples/esp_idf/brookesia) | Integrated GUI with calculator, drawing, audio, camera, video, settings, Xiaozhi and GPIO applications |

See each example's README and configuration for wiring and operation.

### Build and run

Install ESP-IDF and open its configured terminal. The BSP and Brookesia manifests
require **ESP-IDF >=5.5**. These are manifest constraints, not a guarantee that
every example works with every newer release. Dependencies can impose additional
constraints.

From the repository root, for example:

```powershell
cd examples\esp_idf\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the ESP32-P4 serial port. To use a peripheral example, enter
its directory instead. Configure the LCD and any required Wi-Fi credentials
before building. The component manager resolves dependencies during configuration.
The repository ignores local `build/`, `managed_components/`, `sdkconfig` and
`dependencies.lock` files.

ESP32-C5 requires compatible ESP-Hosted slave firmware. Flashing an ESP32-P4
example does not update the C5. The bundled factory images are separate artifacts;
their presence does not establish compatibility with freshly resolved host dependencies.

## Arduino examples

The local board package is documented for **Arduino-ESP32 3.3.11**. Follow the
[Arduino setup instructions](examples/Arduino/README.md) to install the
`waveshare_esp32_p4_wifi6_db` variant and board entry, then select
`Waveshare ESP32-P4-WIFI6-DB`. The board entry defaults to 32 MB Flash, PSRAM enabled
and the `default_32MB` partition scheme. Install each sketch's required libraries
as described in its README.

| Sketch | Purpose |
| --- | --- |
| [board_check](examples/Arduino/board_check) | Chip and memory information |
| [gpio](examples/Arduino/gpio) | Interactive GPIO testing over serial |
| [i2c](examples/Arduino/i2c) | I2C1 address scan |
| [i2s](examples/Arduino/i2s) | ES8311 48 kHz mono microphone-to-speaker loopback |
| [sdmmc](examples/Arduino/sdmmc) | 4-bit microSD file read/write |
| [mipi_dsi](examples/Arduino/mipi_dsi) | LCD color bars and backlight testing |
| [mipi_csi](examples/Arduino/mipi_csi) | OV5647 camera preview on the LCD |

Arduino `mipi_dsi` and `mipi_csi` default to **10.1-inch A JD9365**, 800 × 1280,
RGB565, two DSI lanes at 1500 Mbps/lane. `mipi_dsi` selects the panel in
`esp_panel_drivers_conf.h`; `mipi_csi` selects JD9365 in `lcd_panel.h` and the
10.1-inch A profile in `esp_lcd_jd9365.h`. These settings are separate from the
ESP-IDF BSP Kconfig.

## Validation scope

This overview reflects repository source and default configuration. It does not
certify a successful build or full peripheral validation of the current revision.
Build, flash and runtime results must be checked separately for the selected
example, framework version, panel and board.

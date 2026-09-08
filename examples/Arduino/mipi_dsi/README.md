# mipi_dsi

[中文版本](./README_CN.md)

This example initializes a MIPI-DSI panel with the project's local LCD drivers,
cycles through horizontal and vertical color bars, and verifies the LCD
backlight through the I2C backlight controller. It does not use the camera,
LVGL or GT911 touch.

## Usage

1. Use Arduino-ESP32 `3.3.11` and select the board
   `Waveshare ESP32-P4-WIFI6-DB`.
2. Install or confirm the following Arduino libraries and versions:

   - `ESP32_Display_Panel` `1.0.4`
   - `ESP32_IO_Expander` `1.1.1`
   - `esp-lib-utils` `0.2.3`

   The latter two are dependencies of `ESP32_Display_Panel`. Arduino-ESP32
   `3.3.11` already ships the ESP-IDF `5.5.5` low-level headers and libraries
   used by this example.

3. Open `esp_panel_drivers_conf.h` and enable exactly one local LCD driver:

   - `ESP_PANEL_DRIVERS_LCD_ENABLE_JD9365_LOCAL`
   - `ESP_PANEL_DRIVERS_LCD_ENABLE_HX8394_LOCAL`
   - `ESP_PANEL_DRIVERS_LCD_ENABLE_ILI9881C_LOCAL`

4. If you select JD9365, choose the panel size in the same configuration file.
   The 10.1-inch A panel is selected by default:

   ```cpp
   #define CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A (1)
   #define CONFIG_BSP_LCD_TYPE_800_1280_8_INCH_A    (0)
   #define CONFIG_BSP_LCD_TYPE_720_1280_10_1_INCH_B (0)
   ```

   For other panels, disable the A-type default and enable exactly one matching
   entry. The 10.1-inch B panel is 720×1280 and differs from the A type; the
   attached panel must match the register table and the DSI parameters.
5. Compile, upload, and open the serial monitor at `115200` baud. After start
   the example cycles MIPI-DSI color bars to verify the LCD, the DSI timing and
   the backlight.

The current default configuration is the 10.1-inch A JD9365: 800×1280, RGB565,
two lanes, 1500 Mbps/lane.

If you select HX8394 or ILI9881C, the example uses the fixed resolution and DSI
timing defined in the corresponding driver. The JD9365 panel-size macros only
take effect when JD9365 is selected.

## Why the legacy I2C API is used

The board's backlight controller is at address `0x45` on the I2C1 bus
(GPIO7/GPIO8). The example uses the legacy I2C API from `driver/i2c.h` and
calls `i2c_param_config()`, `i2c_driver_install()` and
`i2c_master_write_to_device()` directly to control the backlight.

Backlight initialization sets the operating mode through register `0x95` and
the brightness through register `0x96`.

This keeps the example consistent with the existing Waveshare examples and with
the older drivers on the same I2C1 bus, and avoids the duplicate initialization
or bus-ownership conflicts that would come from mixing in Arduino `Wire1` or a
second I2C bus management scheme. It is not because `ESP32_Display_Panel` is
outdated; switching to the new I2C API would also require redesigning how I2C1
is initialized and shared between devices.

## Verified versions and dependencies

| Type | Library or component | Current version | Purpose |
| --- | --- | --- | --- |
| Arduino core | Arduino-ESP32 | `3.3.11` | Arduino framework, ESP32-P4 board support |
| ESP-IDF | ESP-IDF bundled with Arduino-ESP32 | `5.5.5` | MIPI-DSI, I2C, FreeRTOS and low-level LCD APIs |
| Direct use | ESP32_Display_Panel | `1.0.4` | `BusDSI`, `LCD` interfaces and DSI panel lifecycle |
| Library dependency | ESP32_IO_Expander | `1.1.1` | Dependency of `ESP32_Display_Panel`; this example creates no IO expander instance |
| Library dependency | esp-lib-utils | `0.2.3` | Shared utility dependency of `ESP32_Display_Panel` and `ESP32_IO_Expander` |

The HX8394, ILI9881C and JD9365 controller implementations are project-local
drivers under this example's `src/drivers/lcd`; they do not correspond to a
separately installed LCD driver library version.

This example does not use LVGL, ESP_Video or the GT911 touch driver, and does
not depend on any source file in the `mipi_csi` directory.

## Troubleshooting

- If there is still no image after `LCD: start complete`, first check that the
  LCD controller, resolution and DSI lane rate match the attached panel.
- If backlight initialization fails, confirm that device `0x45` on I2C1
  responds and that the PHY LDO channel 3 supply meets the panel's
  requirements.
- Do not run the `i2c` or `mipi_csi` examples at the same time; they claim
  ownership of the shared I2C1 bus and initialize it differently.

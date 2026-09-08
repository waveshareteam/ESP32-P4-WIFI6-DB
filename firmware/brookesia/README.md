# ESP-Brookesia Board Test

[中文版本](./README_CN.md)

Run an ESP-Brookesia Phone UI on the ESP32-P4-WIFI6-DB board and expose the
board test applications from one launcher.

This is an integration example. It initializes the local BSP, display and
touch, ES8311 audio, and SPIFFS before it installs the UI applications. The
Settings app initializes the ESP32-C5-hosted Wi-Fi and SNTP control path; Wi-Fi
starts when the WLAN page requests it. This example does not initialize a
PCF85063A RTC; the status-bar clock uses system time.

## Startup Flow

`app_main()` performs these steps:

1. Select the `factory` application partition as the next boot partition.
2. Initialize NVS and mount SPIFFS.
3. Initialize the ES8311 audio path.
4. Start the selected 800x1280 JD9365 display with RGB565 and
   `ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL`.
5. Start the ESP-Brookesia Phone system and install the applications.
6. Turn on the backlight after application installation.
7. Refresh the status-bar clock from system time once per second.
8. Update the Wi-Fi icon on disconnect/IP events and print SRAM/PSRAM usage
   every five seconds.

## Installed Applications

The launcher installs:

- Drawpanel
- SpecAnalyzer
- MusicPlayer
- Camera
- VideoPlayer
- Settings
- GpioMonitor

Successful installation only proves that the software initialization reached
that application. Each peripheral path still needs its own runtime and
hardware test.

## Hardware and Configuration

| Function | Current project configuration |
| --- | --- |
| Display/touch | JD9365 800x1280 MIPI-DSI, GT911; current defaults select the 10.1-inch A variant |
| Display buffers | RGB565, 3 MIPI-DPI frame buffers |
| RTC | Not initialized by this example; the status bar uses system time |
| Audio | ES8311 through the local BSP and `bsp_extra` |
| Camera | OV5647 MIPI-CSI, RAW8 800x1280 at 50 FPS |
| Wireless | ESP32-C5 through ESP-Hosted / Wi-Fi Remote |
| Flash/PSRAM defaults | 16 MB flash, HEX PSRAM at 250 MHz |
| Console baud | 2,000,000 |
| Partitions | 9 MB `factory`, 5 MB `storage` SPIFFS |

The checked-in `sdkconfig.defaults` selects
`CONFIG_BSP_LCD_TYPE_800_1280_10_1_INCH_A`. Select the matching panel in
`menuconfig` before rebuilding.

The board schematic is available at
[hardware/ESP32-P4-WIFI6-DB.pdf](../../hardware/ESP32-P4-WIFI6-DB.pdf).

For a clean configuration, verify these Hosted selections in `menuconfig`:

```text
CONFIG_SLAVE_IDF_TARGET_ESP32C5=y
CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
```

The ESP32-C5 must be flashed separately with compatible ESP-Hosted
co-processor firmware. Flashing this project updates only ESP32-P4.

## Build and Flash

Use ESP-IDF 5.5 or newer. The current firmware was flashed and verified with
ESP-IDF v6.0.1.

```powershell
cd firmware\brookesia
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the ESP32-P4 serial port. The monitor baud is read from
`sdkconfig`.

`components/bsp_extra/idf_component.yml` resolves the BSP from:

```text
../esp32_p4_wifi6_db
```

The build also creates and flashes the `storage` SPIFFS image. No MP3 file is
tracked under `spiffs/music`; add only media that can be redistributed before
expecting MusicPlayer playback.

## Expected Startup Log

The source emits messages equivalent to:

```text
Switch to partition factory
SPIFFS mount successfully
Display ESP-Brookesia phone demo
```

After the GUI starts, memory information should be printed every five seconds.
The firmware has been flashed and verified with ESP-IDF v6.0.1; record
feature-specific observations separately for touch, Wi-Fi, camera frames,
audio, video playback, and GPIO behavior.

## Troubleshooting

- If component resolution cannot find the BSP, verify the relative path above
  before removing the local override.
- If ESP-Hosted fails, check the ESP32-C5 firmware, target selection, transport
  pins, and reset GPIO.
- If the display fails before the launcher appears, check JD9365 reset,
  backlight, MIPI power/lane configuration, PSRAM, and LVGL task allocation.
- If the clock does not update, remember that this example uses system time and
  does not initialize a PCF85063A RTC. Set the system time externally or add an
  RTC/SNTP initialization path before relying on the clock.
- If Camera opens without frames, validate OV5647 power, MIPI-CSI wiring, and
  the selected RAW8 format separately.
- If MusicPlayer has no content, populate the SPIFFS music directory with
  redistributable files and rebuild the partition image.

For ESP-Brookesia framework details, see
[brookesia_core](components/brookesia_core/README.md).

## Validation Boundary

This README was checked against the current source and configuration. The
firmware was flashed and verified with ESP-IDF v6.0.1; this does not claim that
every installed application or peripheral path was exercised in the same run.

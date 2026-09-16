| Supported Targets | ESP32-P4 |
| ----------------- | -------- |

# Board Manager Test Application

[中文](README_zh.md)

An interactive console for bringing up the ESP32-P4-WIFI6-DB board
through [ESP Board Manager](https://github.com/espressif/esp-board-manager)
and running functional test cases (LCD, touch, audio, camera, SD card,
GPIO, ...) on real hardware. The board definition lives in
[`components/waveshare_bmgr`](../../../components/waveshare_bmgr).

This project is adapted from the official `test_apps` project shipped with
Espressif's [esp-board-manager](https://github.com/espressif/esp-board-manager)
component (v0.7.x, `managed_components/espressif__esp_board_manager/test_apps`
after the first build), trimmed to the ESP32-P4 target and pointed at the
Waveshare board package. The test framework, device/peripheral test sources,
partition table, and `sdkconfig.defaults*` keep Espressif's original
`LicenseRef-Espressif-Modified-MIT` headers; the full text is in
[`LICENSE`](LICENSE) in this directory, and is separate from the Apache-2.0
license that covers the rest of this repository.

## 1. Board Manager setup

`idf.py bmgr` is provided by the `esp_board_manager` component, not by
ESP-IDF. Install `esp-bmgr-assist` once in the ESP-IDF Python environment
(activate it with `export.ps1` / `export.sh` first); it registers the `bmgr`
command automatically for every project:

```bash
pip install esp-bmgr-assist
```

Check that it works:

```bash
idf.py bmgr -l -c ../../../components/waveshare_bmgr
```

`esp32_p4_wifi6_db` should appear in the list.

## 2. Build and flash

Run all commands from this directory.

```bash
idf.py set-target esp32p4
```

Generate the board configuration. `-c` points at the boards package in this
repository (it is not on the component registry, so it is not discovered
automatically):

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c ../../../components/waveshare_bmgr
```

The default display is the 10.1" DSI panel. For another Waveshare DSI touch
display add `-a` with the matching amend directory
(`lcd_5_dsi_touch_a`, `lcd_7_dsi_touch_a`, `lcd_8_dsi_touch_a`,
`lcd_10_1_dsi_touch_a`):

```bash
idf.py bmgr -b esp32_p4_wifi6_db -c ../../../components/waveshare_bmgr -a ../../../components/waveshare_bmgr/amends/lcd_7_dsi_touch_a
```

Then build, flash and open the monitor:

```bash
idf.py build flash monitor
```

Notes:

- `bmgr` writes `components/gen_bmgr_codes/` (git-ignored) and deletes
  `build/CMakeCache.txt`, so the next `build` reconfigures automatically.
  Re-run `bmgr` whenever you switch the display amend or edit the board
  YAML; `idf.py bmgr -x` clears the generated configuration.
- `set-target` can be run before `bmgr`, but `build` needs `bmgr` to have
  completed at least once.
- The console runs on UART0 (the USB download port). If the board is
  connected only through the USB-Serial/JTAG port, switch the console with
  `idf.py menuconfig` → *Component config → ESP System Settings → Channel
  for console output*.

## 3. Using the console

After boot the prompt appears on the monitor. Type `help` for the full
command list. The two commands you need are `bmgr` and `case`:

```
bmgr init                  # initialize every device and peripheral of the board
bmgr init lcd_touch        # initialize a single device/peripheral by name
bmgr info                  # print board name / chip / version
bmgr print                 # dump all peripheral and device configuration
bmgr status                # show which devices are currently initialized
bmgr deinit                # release everything (or `bmgr deinit <name>`)

case list                  # list test cases available for this board
case list --group lcd      # list cases of one group
case run lcd.color.red     # run one test case
case run-all --group periph  # run every case in a group
case run-all               # run every non-manual case
case run-all --include-manual  # also run cases that need user interaction
```

Typical session:

```
bmgr init
case list
case run lcd.pattern.checker
case run periph.i2c_probe
bmgr deinit
```

Cases are grouped as `audio`, `button`, `camera`, `custom`, `fs`,
`gpio_expander`, `knob`, `lcd`, `led`, and `periph`. Only the cases whose
devices/peripherals the board actually declares are compiled in, so
`case list` shows exactly what can run on this board.

Add `--summary` (human readable) or `--json` (machine readable) to a `case`
command to print pass/fail metrics at the end of the run.

The test cases look devices up by their conventional Board Manager names.
This board's only `custom` device is `lcd_brightness` (the I2C backlight
controller), and `sdkconfig.defaults` already points `custom.basic` at it
(`CONFIG_BMGR_TEST_NAME_CUSTOM`). The other names can be overridden under
*Board Manager Test App → Device/Peripheral name overrides* in
`idf.py menuconfig`.

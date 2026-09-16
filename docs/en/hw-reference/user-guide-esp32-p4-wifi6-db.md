# ESP32-P4-WIFI6-DB

[中文版本](../../zh_CN/hw-reference/user-guide-esp32-p4-wifi6-db.md)

ESP32-P4-WIFI6-DB is a development board built around the ESP32-P4NRW32X
application processor (dual-core high-performance RISC-V + a low-power core,
with 32 MB in-package PSRAM), paired with an ESP32-C5-MINI-1 Wi-Fi 6 /
Bluetooth 5 (LE) coprocessor over SDIO. The board provides a two-lane
MIPI-DSI touch-panel interface, a two-lane MIPI-CSI camera interface, an
ES8311 audio codec, microSD, USB 2.0 High-Speed OTG, and a 40-pin expansion
header.

![ESP32-P4-WIFI6-DB](../../../asserts/ESP32-P4-WIFI6-DB.jpg)

> **Note**
>
> This page is compiled from three sources: the board support package (BSP)
> in [`components/waveshare_bsp/esp32_p4_wifi6_db`](../../../components/waveshare_bsp/esp32_p4_wifi6_db/),
> the schematic in [`hardware/ESP32-P4-WIFI6-DB.pdf`](../../../hardware/ESP32-P4-WIFI6-DB.pdf),
> and the [official wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB).
> GPIO assignments are cross-checked between the BSP and the schematic
> netlist. Mechanical details (board outline, connector part numbers) are only
> available from the wiki and are not reproduced here.

## Onboard Resources

- **Main processor**: ESP32-P4NRW32X — dual-core RISC-V HP system + low-power
  core, 32 MB stacked PSRAM.
- **Flash**: 32 MB external NOR Flash, QIO (`CONFIG_ESPTOOLPY_FLASHSIZE_32MB`
  in the example `sdkconfig.defaults`).
- **PSRAM**: 32 MB in-package. Examples default to 200 MHz
  (`CONFIG_SPIRAM_SPEED_200M`); Brookesia selects hex mode at 250 MHz.
- **Wireless coprocessor**: ESP32-C5-MINI-1 (2.4 / 5 GHz Wi-Fi 6 + Bluetooth
  5 LE) over 4-bit SDIO, driven by the application's `esp_hosted` /
  `esp_wifi_remote` components. The BSP itself does not configure the SDIO
  link. A 4-pin header (`H4`: BOOT, U0RXD, U0TXD, GND) exposes the ESP32-C5
  download port for reflashing the slave firmware.
- **USB-UART**: CH343P bridge on the USB Type-C port; provides power, firmware
  download and the serial monitor on UART0 (GPIO37/GPIO38).
- **USB OTG**: 4-pin connector wired directly to the ESP32-P4 internal USB 2.0
  High-Speed PHY (DM/DP). The BSP drives it as USB Host
  (`bsp_usb_host_start()`).
- **Display**: Two-lane MIPI-DSI on a 24-pin FPC connector, Kconfig-selectable
  panel — HX8394 (5", 720×1280, 700 Mbps/lane), ILI9881C (7", 720×1280,
  1000 Mbps/lane), or JD9365 (8" / 10.1", 800×1280, 1500 Mbps/lane, default).
  No hardware LCD reset pin; backlight is controlled over I2C.
- **Touch**: GT911 capacitive touch controller on the shared I2C bus, polled
  (reset/interrupt not wired). Both `0x5D` and `0x14` are probed.
- **Audio**: ES8311 codec, onboard microphone (one analog input), 3.5 mm
  headphone jack, and a speaker connector driven by an onboard power amplifier
  (enable on GPIO53).
- **Storage**: microSD over 4-bit SDMMC (slot 0), card power switched by a
  P-MOSFET on GPIO45; SDMMC IO voltage from on-chip LDO channel 4.
- **Camera**: Two-lane MIPI-CSI on a 24-pin FPC connector, via `esp_video`.
  The repository examples configure OV5647 (RAW8 800×1280 @ 50 fps).
  Camera XCLK/RESET are not driven by the BSP (`GPIO_NUM_NC`).
- **Expansion header**: 40-pin (2×20) header using the Raspberry Pi Pico
  footprint; 27 GPIOs are routed to it, 25 of which are enumerated by
  `bsp_get_header_gpios()` (see below).
- **Buttons / LEDs**: BOOT button on GPIO35, RESET button on `CHIP_PU`,
  power LED. The BSP does not register buttons (`BSP_CAPS_BUTTONS = 0`).
- **RTC**: 32.768 kHz crystal on GPIO0/GPIO1 for the ESP32-P4 RTC slow clock;
  no external RTC chip (`BSP_CAPS_RTC = 0`).
- **ESP-IDF target**: `esp32p4`, requires ESP-IDF >= 5.5.

## Peripheral Quick Reference

| Module | Device / Function | Interface | Address / Parameters | GPIO / Signal |
| --- | --- | --- | --- | --- |
| LCD | MIPI-DSI panel | MIPI-DSI | 2-lane; HX8394 / ILI9881C / JD9365, Kconfig-selectable; RGB565 (default) or RGB888 | No LCD reset pin (`GPIO_NUM_NC`); backlight over shared I2C |
| Touch | GT911 capacitive touch | I2C | 7-bit address `0x5D` or `0x14` (both probed) | SDA=GPIO7, SCL=GPIO8; RST/INT not wired |
| LCD backlight | Backlight controller | I2C | 7-bit address `0x45`; brightness register `0x96`, data `0..255` | SDA=GPIO7, SCL=GPIO8 |
| Camera | MIPI-CSI camera connector | MIPI-CSI + SCCB | 2-lane; OV5647 configured by examples, other `esp_video` sensors selectable in Kconfig | SCCB shares GPIO7/GPIO8; XCLK/RESET not driven by BSP |
| Audio | ES8311 codec | I2C + I2S | I2C 7-bit `0x18` (`ES8311_CODEC_DEFAULT_ADDR`); 48 kHz, 16-bit, mono, duplex by default | I2C: SDA=GPIO7, SCL=GPIO8; MCLK=GPIO13, SCLK=GPIO12, WS=GPIO10, DOUT=GPIO9, DIN=GPIO11 |
| Power amp | Onboard speaker amp enable | GPIO | Active high | GPIO53 |
| microSD | SDMMC 4-bit | SDMMC | 4-bit mode, slot 0; IO power from LDO channel 4 | CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42; power enable=GPIO45 (active low) |
| Wi-Fi/BT coprocessor | ESP32-C5-MINI-1, ESP-Hosted link | SDIO | 4-bit; 51 kΩ pull-ups on all six lines | CLK=GPIO18, CMD=GPIO19, D0=GPIO14, D1=GPIO15, D2=GPIO16, D3=GPIO17; CHIP_PU=GPIO54 (active low); IO2 handshake=GPIO6 |
| USB OTG | USB 2.0 High-Speed Host | USB 2.0 HS | Internal USB 2.0 PHY | D+/D- wired directly to ESP32-P4 DM/DP (4-pin connector) |
| USB 1.1 | Full-speed USB PHY | USB 1.1 FS | Not used by the BSP | D-=GPIO24, D+=GPIO25 (routed to header pins 19/20) |
| USB-UART | CH343P | UART0 | Download and serial monitor | TXD=GPIO37, RXD=GPIO38 (USB Type-C) |
| Buttons | BOOT / RESET | GPIO | BOOT is a strapping pin | BOOT=GPIO35; RESET=`CHIP_PU` |

## Pin Definition

### Expansion Header

The 40-pin header follows the Raspberry Pi Pico pin arrangement. The table
lists the ESP32-P4 signal on each pin (schematic `U10`).

| Pin | Signal | Pin | Signal |
| ---: | --- | ---: | --- |
| 1 | GPIO52 | 40 | VBUS (USB 5 V) |
| 2 | GPIO51 | 39 | VSYS (5 V) |
| 3 | GND | 38 | GND |
| 4 | GPIO31 | 37 | 3V3_EN |
| 5 | GPIO30 | 36 | 3V3 out |
| 6 | GPIO29 | 35 | GPIO20 |
| 7 | GPIO28 | 34 | GPIO21 |
| 8 | GND | 33 | GND |
| 9 | GPIO50 | 32 | GPIO22 |
| 10 | GPIO49 | 31 | GPIO23 |
| 11 | GPIO5 | 30 | RUN (ESP32-P4 EN) |
| 12 | GPIO4 | 29 | GPIO26 |
| 13 | GND | 28 | GND |
| 14 | GPIO3 | 27 | GPIO27 |
| 15 | GPIO2 | 26 | GPIO32 |
| 16 | GPIO8 (I2C SCL) | 25 | GPIO33 |
| 17 | GPIO7 (I2C SDA) | 24 | GPIO46 |
| 18 | GND | 23 | GND |
| 19 | GPIO24 (USB1 D-) | 22 | GPIO47 |
| 20 | GPIO25 (USB1 D+) | 21 | GPIO48 |

`bsp_get_header_gpios()` returns the BSP-owned, read-only array below (25
entries). GPIO7/GPIO8 are on the header but are excluded from the array because
they carry the shared I2C bus.

```text
52, 51, 31, 30, 29, 28, 50, 49, 5, 4, 3, 2, 24,
25, 20, 21, 22, 23, 26, 27, 32, 33, 46, 47, 48
```

GPIO24/GPIO25 are also the ESP32-P4 full-speed USB 1.1 PHY data lines. They
are free GPIOs unless the application enables that PHY.

### Camera and Display Connectors

Both FPC connectors carry the shared I2C bus (GPIO7/GPIO8) on pins 2/3 for
the camera SCCB interface and for the display's GT911 touch controller and
backlight controller respectively.

| Connector | Interface | ESP32-P4 signals |
| --- | --- | --- |
| `J1` (24-pin) | MIPI-CSI | CSI_CLK_P/N, CSI_D0_P/N, CSI_D1_P/N, I2C SDA/SCL, 3.3 V, GND |
| `J2` (24-pin) | MIPI-DSI | DSI_CLK_P/N, DSI_D0_P/N, DSI_D1_P/N, I2C SDA/SCL, 3.3 V, GND |

## Full GPIO Allocation

The table lists every GPIO (0–54) and what it is connected to on the board.
"Header pin N" refers to the 40-pin expansion header above.

| GPIO | Signal | Connected To | Notes |
| --- | --- | --- | --- |
| GPIO0 | XTAL_32K_N | 32.768 kHz crystal (`Y2`) via 0 Ω | RTC slow clock; remove the 0 Ω link before repurposing |
| GPIO1 | XTAL_32K_P | 32.768 kHz crystal (`Y2`) via 0 Ω | RTC slow clock; remove the 0 Ω link before repurposing |
| GPIO2 | GPIO2 | Header pin 15 | `bsp_get_header_gpios()` |
| GPIO3 | GPIO3 | Header pin 14 | `bsp_get_header_gpios()` |
| GPIO4 | GPIO4 | Header pin 12 | `bsp_get_header_gpios()` |
| GPIO5 | GPIO5 | Header pin 11 | `bsp_get_header_gpios()` |
| GPIO6 | C5_IO2 | ESP32-C5 IO2 via 0 Ω (`R15`) | Reserved for the coprocessor (ESP-Hosted handshake / wake); not on the header |
| GPIO7 | I2C SDA | ES8311, GT911, LCD backlight, camera SCCB, header pin 17 | Shared I2C data line; not in the header GPIO array |
| GPIO8 | I2C SCL | ES8311, GT911, LCD backlight, camera SCCB, header pin 16 | Shared I2C clock line; not in the header GPIO array |
| GPIO9 | I2S DOUT | ES8311 DSDIN | Audio playback data, ESP32-P4 → codec |
| GPIO10 | I2S WS | ES8311 LRCK | Audio frame sync |
| GPIO11 | I2S DIN | ES8311 ASDOUT | Audio capture data, codec → ESP32-P4 |
| GPIO12 | I2S SCLK | ES8311 SCLK | Audio bit clock |
| GPIO13 | I2S MCLK | ES8311 MCLK | Audio master clock |
| GPIO14 | SDIO D0 | ESP32-C5 IO8 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO15 | SDIO D1 | ESP32-C5 IO7 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO16 | SDIO D2 | ESP32-C5 IO14 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO17 | SDIO D3 | ESP32-C5 IO13 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO18 | SDIO CLK | ESP32-C5 IO9 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO19 | SDIO CMD | ESP32-C5 IO10 | Hosted Wi-Fi/BT link; 51 kΩ pull-up |
| GPIO20 | GPIO20 | Header pin 35 | `bsp_get_header_gpios()` |
| GPIO21 | GPIO21 | Header pin 34 | `bsp_get_header_gpios()` |
| GPIO22 | GPIO22 | Header pin 32 | `bsp_get_header_gpios()` |
| GPIO23 | GPIO23 | Header pin 31 | `bsp_get_header_gpios()` |
| GPIO24 | GPIO24 / USB1 D- | Header pin 19 | `bsp_get_header_gpios()`; full-speed USB PHY data line if enabled |
| GPIO25 | GPIO25 / USB1 D+ | Header pin 20 | `bsp_get_header_gpios()`; full-speed USB PHY data line if enabled |
| GPIO26 | GPIO26 | Header pin 29 | `bsp_get_header_gpios()` |
| GPIO27 | GPIO27 | Header pin 27 | `bsp_get_header_gpios()` |
| GPIO28 | GPIO28 | Header pin 7 | `bsp_get_header_gpios()` |
| GPIO29 | GPIO29 | Header pin 6 | `bsp_get_header_gpios()` |
| GPIO30 | GPIO30 | Header pin 5 | `bsp_get_header_gpios()` |
| GPIO31 | GPIO31 | Header pin 4 | `bsp_get_header_gpios()` |
| GPIO32 | GPIO32 | Header pin 26 | `bsp_get_header_gpios()` |
| GPIO33 | GPIO33 | Header pin 25 | `bsp_get_header_gpios()` |
| GPIO34 | — | Test point `TP1` only | Strapping pin; not routed to any connector |
| GPIO35 | BOOT | BOOT button (`Key1`) | Strapping pin; hold at power-on to enter download mode |
| GPIO36 | — | Test point `TP2`, 3.3 V pull-up (`R36`) | Strapping pin; not routed to any connector |
| GPIO37 | UART0 TXD | CH343P | Firmware download and serial monitor; avoid using as a plain GPIO |
| GPIO38 | UART0 RXD | CH343P | Firmware download and serial monitor; avoid using as a plain GPIO |
| GPIO39 | SD D0 | microSD card | SDMMC 4-bit data line; 51 kΩ pull-up |
| GPIO40 | SD D1 | microSD card | SDMMC 4-bit data line; 51 kΩ pull-up |
| GPIO41 | SD D2 | microSD card | SDMMC 4-bit data line; 51 kΩ pull-up |
| GPIO42 | SD D3 | microSD card | SDMMC 4-bit data line; 51 kΩ pull-up |
| GPIO43 | SD CLK | microSD card | SDMMC clock |
| GPIO44 | SD CMD | microSD card | SDMMC command; 51 kΩ pull-up |
| GPIO45 | SD_VDD_EN | microSD power switch (`Q1`, P-MOSFET) | Active low; not on the header |
| GPIO46 | GPIO46 | Header pin 24 | `bsp_get_header_gpios()` |
| GPIO47 | GPIO47 | Header pin 22 | `bsp_get_header_gpios()` |
| GPIO48 | GPIO48 | Header pin 21 | `bsp_get_header_gpios()` |
| GPIO49 | GPIO49 | Header pin 10 | `bsp_get_header_gpios()` |
| GPIO50 | GPIO50 | Header pin 9 | `bsp_get_header_gpios()` |
| GPIO51 | GPIO51 | Header pin 2 | `bsp_get_header_gpios()` |
| GPIO52 | GPIO52 | Header pin 1 | `bsp_get_header_gpios()` |
| GPIO53 | PA_CTRL | Speaker amplifier enable | Active high; not on the header |
| GPIO54 | C5_CHIP_PU | ESP32-C5 EN via 0 Ω (`R26`) | Active-low reset for the Wi-Fi/BT coprocessor |

## Usage Notes

- GPIO7/GPIO8 form the shared I2C bus for ES8311 (`0x18`), GT911
  (`0x5D`/`0x14`), the LCD backlight controller (`0x45`), and the camera SCCB
  interface. They are also exposed on header pins 16/17; check for address
  conflicts before adding another I2C device.
- GPIO45 controls microSD power, GPIO53 controls the audio power amplifier,
  GPIO54 resets the ESP32-C5, and GPIO6 is connected to ESP32-C5 IO2. None of
  these are on the header; do not drive them independently while the
  corresponding peripheral is active.
- GPIO37/GPIO38 are the UART0 download port (TXD/RXD) behind the CH343P,
  used for flashing firmware and the serial monitor; avoid using them as
  plain GPIO.
- GPIO34–GPIO38 are ESP32-P4 strapping pins. GPIO35 is the BOOT button,
  GPIO36 has a 3.3 V pull-up, GPIO37/GPIO38 are UART0. GPIO34 and GPIO36 are
  only brought out to test points. Check the ESP32-P4 datasheet before
  changing their state during power-up or reset.
- GPIO0/GPIO1 connect to the onboard 32.768 kHz crystal through 0 Ω links
  (`R27`/`R31`). The BSP does not configure the RTC slow clock source.
  Repurposing either pin requires removing the corresponding link.
- GPIO24/GPIO25 double as the ESP32-P4 full-speed USB 1.1 PHY data lines and
  are brought out on header pins 19/20. They remain plain GPIOs unless the
  application enables that PHY.
- GPIO14–GPIO19 are the ESP32-C5 SDIO link. Flashing an ESP32-P4 example
  does not update the ESP32-C5; keep the slave firmware compatible with the
  `esp_hosted` / `esp_wifi_remote` versions declared by each wireless project.
  The prebuilt ESP32-C5 image is under
  [`firmware/factory-firmware/ESP32-C5`](../../../firmware/factory-firmware/ESP32-C5/).
- LCD reset, GT911 reset, and GT911 interrupt are not wired; the display and
  touch drivers handle this in software (polling for touch).
- `bsp_sdcard_mount()` power-cycles the card (100 ms off, 200 ms settle)
  before SDMMC initialization and enables the internal pull-ups; the board
  also has 51 kΩ external pull-ups on CMD and D0–D3.
- The header GPIO array (`bsp_get_header_gpios()`) is a software enumeration
  used by examples such as `01_gpio_input`; it deliberately omits GPIO7/GPIO8.

## Board Dimensions

Board outline and mounting-hole positions are published on the
[official wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB) and are not
reproduced in this repository.

## Related Documentation

- [BSP README](../../../components/waveshare_bsp/esp32_p4_wifi6_db/README.md)
- [BSP API reference](../../../components/waveshare_bsp/esp32_p4_wifi6_db/API.md)
- [BSP changelog](../../../components/waveshare_bsp/esp32_p4_wifi6_db/CHANGELOG.md)
- [Schematic](../../../hardware/ESP32-P4-WIFI6-DB.pdf)
- [Official wiki](https://docs.waveshare.net/ESP32-P4-WIFI6-DB)
- [Product page](https://www.waveshare.net/shop/ESP32-P4-WIFI6-DB.htm)

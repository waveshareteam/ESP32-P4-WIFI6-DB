# ESP32-C5 Hosted Wi-Fi Station / SoftAP

[中文版本](./README_CN.md)

Verify Wi-Fi on ESP32-P4 through an ESP32-C5 wireless co-processor and
ESP-Hosted. Station mode is the default. SoftAP mode can be enabled for
phone-based connection and signal checks.

## Hardware Required

- ESP32-P4 board connected to an ESP32-C5 through the board's ESP-Hosted
  transport.
- Matching ESP-Hosted co-processor firmware already flashed to the ESP32-C5.
- A 2.4 GHz access point for Station mode, or a Wi-Fi phone/client for SoftAP
  mode.

ESP32-P4 has no native Wi-Fi radio. Flashing this host project does not update
the ESP32-C5 firmware; the two sides must use compatible ESP-Hosted firmware,
transport pins, and reset wiring.

## Default Transport Configuration

`sdkconfig.defaults` selects:

```text
CONFIG_SLAVE_IDF_TARGET_ESP32C5=y
CONFIG_ESP_WIFI_REMOTE_LIBRARY_HOSTED=y
```

The component manifest resolves `esp_wifi_remote` and `esp_hosted` versions
according to the ESP-IDF version.

## Build and Flash

Run the commands from an ESP-IDF PowerShell:

```powershell
cd examples\esp_idf\06_wifi
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the ESP32-P4 serial port.

## Configuration

Open **Example Configuration** in `menuconfig`:

- Set **WiFi SSID** and **WiFi Password**. Both modes use these fields.
- Leave **Run in SoftAP mode** disabled for Station mode.
- Enable it for SoftAP mode, then set the channel and maximum connection count.
- In Station mode, set the retry count and minimum accepted authentication
  mode if the defaults are unsuitable.

The example prints the configured SSID and password. Do not use production
credentials in serial logs that will be shared.

## Expected Behavior

### Station mode

The log should show ESP-Hosted startup, connection retries if any, a DHCP
address, and the result of a gateway ping.

### SoftAP mode

The log should include:

```text
SoftAP ready
SSID: myssid
Password: mypassword
Channel: 1, max connections: 4
AP IP address: 192.168.4.1
Station connected: XX:XX:XX:XX:XX:XX, aid=1
```

## Troubleshooting

- If ESP-Hosted does not connect, check the ESP32-C5 firmware, transport pins,
  reset GPIO, and co-processor target selection before debugging Wi-Fi.
- Confirm the network or phone uses 2.4 GHz Wi-Fi.
- In Station mode, verify the SSID, password, and authentication threshold.
- In SoftAP mode, use an empty password for an open network or at least eight
  characters for WPA2.
- If managed-component resolution fails in an editor, retry from an ESP-IDF
  command-line shell.

Source inspection confirms the configuration and code paths above. Build,
flash, association, SoftAP connection, and RF behavior remain to be verified
on the board.

# ES8311 I2S loopback

[中文版本](./README_CN.md)

This example targets the `ESP32-P4-WIFI6-DB` and uses the onboard ES8311 to
sample one analog microphone in mono and play it back through the single
speaker.

## Usage

1. Use Arduino-ESP32 `3.3.11` and select `Waveshare ESP32-P4-WIFI6-DB`.
2. Use the onboard microphone and speaker. If you connect external audio
   hardware, confirm that its levels and interface match the ES8311.
3. Open the serial monitor at `115200` baud. After upload the example runs a
   continuous loopback at 48000 Hz, 16-bit, I2S standard mono format.

## Board parameters

- ES8311 control address: 7-bit address `0x18` for Arduino `Wire1` (the
  ESP-IDF BSP uses `0x30` as the 8-bit write address).
- I2C1: SDA GPIO7, SCL GPIO8.
- I2S: MCLK GPIO13, BCLK GPIO12, LRCK GPIO10, DOUT GPIO9, DIN GPIO11, using a
  mono slot (left slot).
- Amplifier enable: GPIO53, active high.
- Microphone ADC gain: 24 dB, matching the recording configuration of the
  ESP32-P4-WIFI6-DB BSP.
- MCLK is 12.288 MHz (48 kHz × 256); GPIO13 must be wired to the ES8311 MCLK.

## Expected output and notes

On a successful start you see `ES8311 ready`, followed by one
`I2S input peak` line per second. The loopback will not work if the I2C scan
does not find `0x18`, if MCLK is not connected, or if the amplifier is not
enabled. Placing the microphone too close to the speaker causes acoustic
feedback; lower the volume or separate them first.

The ES8311 register initialization in this example comes from the ES8311
driver configuration in Espressif's `esp_codec_dev`. The Arduino core does not
expose an `esp_codec_dev` board-level BSP wrapper directly, so a local control
helper is kept here that covers only the current 48 kHz loopback path.

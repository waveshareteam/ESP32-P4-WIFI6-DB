# ES8311 I2S Codec

[中文版本](./README_CN.md)

Run continuous PCM playback or microphone echo through an I2S audio codec.
The example uses the local ESP32-P4 BSP and its ES8311 playback and single-microphone recording devices.

## Hardware Required

- ESP32-P4-WIFI6-DB board for the default BSP/ES8311 path.
- Speaker or headphones for playback.
- Microphone input for echo mode.

## Build and Flash

```powershell
cd examples\esp_idf\07_i2s_codec
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the actual serial port.

## Configuration

Open **Example Configuration**:

- Select `music` to loop the embedded `main/canon.pcm` sample.
- Select `echo` to read microphone PCM and write it back to the output.
- Set **Voice volume** from 0 to 100.
- Set **Microphone gain** to 0–42 dB in 6 dB steps (default: 24 dB).

The Kconfig default is `music`. Both modes use the BSP; there is no BSP toggle.
The codec helper uses 16 kHz, 16-bit stereo PCM and the microphone gain selected in menuconfig.
`CODEC_DEFAULT_*` in `bsp_board_extra.h` defines the codec defaults;
`example_config.h` only defines the Echo buffer size in bytes.

The BSP sets ES8311 `no_dac_ref = true`. In two-channel capture, the right
channel is left empty rather than carrying a DAC-output reference.

Current ESP32-P4 I2C/I2S wiring:

| Signal | GPIO |
| --- | ---: |
| I2C SDA | 7 |
| I2C SCL | 8 |
| I2S MCLK | 13 |
| I2S BCLK | 12 |
| I2S LRCK | 10 |
| I2S DOUT → ES8311 DSDIN | 9 |
| ES8311 ASDOUT → I2S DIN | 11 |

The local BSP assigns GPIO53 to power-amplifier enable.

## Expected Behavior

- Successful initialization logs `codec init success`.
- Music mode repeatedly logs the number of PCM bytes written.
- Echo mode logs `[echo] Echo start`, then continuously captures and plays PCM.

Initialization logs prove the software path opened. They do not prove that
either PCM channel contains a valid microphone signal or that the analog output
is audible.

## Troubleshooting

- Confirm codec power, MCLK/BCLK/LRCK activity, I2C control, I2S wiring, and
  the selected speaker/headphone route.
- In echo mode, inspect left and right PCM peak or RMS values before assigning
  the fault to the codec or analog hardware.
- Use [02_i2c_console](../02_i2c_console/) to confirm that the
  expected codec responds on GPIO7/GPIO8.

The README has been checked against the source. Audio output and capture remain
to be verified on hardware.

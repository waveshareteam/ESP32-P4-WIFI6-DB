# xiaozhi-esp32 adaptations

This directory contains the small OGG demuxer imported from
78/xiaozhi-esp32 at commit
0ec696f64f5843ca0f5fcf700ae45977d1dcd2e8.

The matching Chinese activation prompt and digit recordings are stored in
spiffs/xiaozhi/:

- activation.ogg
- 0.ogg through 9.ogg
- success.ogg

The recordings are 16 kHz mono Opus audio in OGG containers. The firmware
demuxes each file and feeds raw Opus packets to esp_audio_codec, avoiding a
dependency on the complete upstream audio framework.

The protocol, activation HTTP flow, and AFE service code now lives in the
`components/Xiaozhi/services` submodule and uses a C API. The P4 identity
backend lives in `components/Xiaozhi/board`. This directory only keeps the
board-side activation prompt, OGG demuxer, codec access, and single-microphone
audio adapter used by the Brookesia application. The board adapter uses the
ES8311's single analog microphone input and a mono 16 kHz PCM path.

The imported code and recordings are distributed under the MIT license in
LICENSE.

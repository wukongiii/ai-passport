<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Pinned ESP32-C3 Chinese TTS subset

Unmodified files from [Espressif ESP-SR](https://github.com/espressif/esp-sr/tree/78a923d29a45e4e1433b909d3da12ab5b0f994fc/esp-tts/esp_tts_chinese),
commit `78a923d29a45e4e1433b909d3da12ab5b0f994fc`:

| Local file | Upstream relative path | SHA-256 |
| --- | --- | --- |
| `lib/libesp_tts_chinese.a` | `esp32c3/libesp_tts_chinese.a` | `5a1125b4739162e454fbbe4321fab944d5339ffd92f79401ff5542cc8c0038ea` |
| `lib/libvoice_set_xiaole.a` | `esp32c3/libvoice_set_xiaole.a` | `a841cd613cd4a81e9c45718679ebb3899778bec7fe75107de28914dedf530816` |

The three headers under `include/` retain their upstream contents; `esp_tts.h`
retains its Apache-2.0 notice. [LICENSE](LICENSE) is the upstream Espressif MIT
license, restricted to Espressif products. No recognition models are included.
The local CMake file links only the C3 synthesis library and voice descriptor;
unused voice objects are discarded by the linker. The separate Xiaoxin voice
asset lives under [assets](../../assets/README.md#offline-chinese-speech-voice).
No extra partition or whole-chip erase is required.

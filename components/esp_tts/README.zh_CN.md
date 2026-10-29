<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 固定版本 ESP32-C3 中文 TTS 子集

未修改文件来自[乐鑫 ESP-SR](https://github.com/espressif/esp-sr/tree/78a923d29a45e4e1433b909d3da12ab5b0f994fc/esp-tts/esp_tts_chinese)，
提交 `78a923d29a45e4e1433b909d3da12ab5b0f994fc`：

| 本地文件 | 上游相对路径 | SHA-256 |
| --- | --- | --- |
| `lib/libesp_tts_chinese.a` | `esp32c3/libesp_tts_chinese.a` | `5a1125b4739162e454fbbe4321fab944d5339ffd92f79401ff5542cc8c0038ea` |
| `lib/libvoice_set_xiaole.a` | `esp32c3/libvoice_set_xiaole.a` | `a841cd613cd4a81e9c45718679ebb3899778bec7fe75107de28914dedf530816` |

`include/` 的三个头文件保留上游原文；`esp_tts.h` 保留 Apache-2.0 声明。
[LICENSE](LICENSE) 为上游乐鑫 MIT 许可，使用范围限定于乐鑫产品。未包含识别模型。
本地 CMake 文件仅链接 C3 合成库和音库描述符；链接器丢弃未引用的音库对象。
独立小新音库位于 [assets](../../assets/README.zh_CN.md#离线中文语音音库)。
无需新增分区或整片擦除。

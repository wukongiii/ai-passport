<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, sound effects, and battery profiles, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.

## Battery profiles

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
contains the supplied CW2017 profile for the Wanli New Energy 500 mAh cell
(profile2, XIL40X, 2026-10-20). It is an unchanged copy of the project-supplied
file, retained at the maintainer's request for this firmware update. Its
original filename is recorded in the paired Chinese document. No separate
license was included in the supplied file.

The format is 80 lines of two hexadecimal digits, one byte per line in register
order. The BSP embeds those bytes in `components/bsp/src/bsp_battery.c`; the
host gate checks the source file and initializer for an exact match. Text-file
SHA-256: `7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`.
Decoded 80-byte SHA-256:
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`.
This is a fuel-gauge profile, not a charging configuration or a generic profile
for every 500 mAh cell; see the [hardware guide](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md#9-cw2017-fuel-gauge).

<a id="scoreboard-font"></a>
### Scoreboard font

`fonts/score_font_16.c`, `fonts/score_font_14.c` and `fonts/score_font_inventory.c` are generated for the
fixed scoreboard strings plus printable ASCII. The font is Noto Sans CJK SC
Regular, 16 px for the application and 14 px for compact input, 4 bpp uncompressed, without kerning; license: SIL OFL 1.1
([license](fonts/OFL.txt)). Source: [Noto CJK](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c),
`Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`, SHA-256
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`.
The full source font and converter cache stay under ignored `build/font-tools/`.
Regenerate after changing `main/score/score_strings.h`, `main/ui/app_strings.h`
or `main/app/app_strings.h` / `main/input/text_input_strings.h`:

```bash
npm install --prefix build/font-tools --no-audit --no-fund lv_font_conv@1.5.3
python3 tools/generate_score_font.py --font build/font-tools/NotoSansCJKsc-Regular.otf --converter build/font-tools/node_modules/.bin/lv_font_conv
```

The generator verifies the source hash. The application compiles both sizes and the shared inventory; each size is checked
for required code points before its UI is created. The subset does not
cover arbitrary player names. Physical Chinese rendering remains device acceptance.

### Launcher illustrations

`images/launcher/{eight,chase,snooker,settings,voice}.png` are original geometric
illustrations authored for this application; they follow the repository license.
`images/launcher/launcher_art.c` embeds the same five 176 x 96 images as RGB565
constants (168960 pixel bytes total). No full-image decompression is needed.
Regenerate both editable generator output and firmware pixels with Pillow 13.0.0
and the documented OFL font source (digits only):

```bash
python3 -m pip install --target build/art-tools Pillow==13.0.0
PYTHONPATH=build/art-tools python3 tools/generate_launcher_art.py --font build/font-tools/NotoSansCJKsc-Regular.otf
```

The source is `tools/generate_launcher_art.py`. The firmware and host preview
compile the generated C once; PNG files are review assets, not runtime decodes.

### Network-name font

`fonts/network_font_14.c` uses the same pinned Noto CJK source and OFL license
as the scoreboard font. It includes printable ASCII, CJK unified ideographs
U+4E00..U+9FFF, CJK punctuation and fullwidth ASCII at 14 px / 2 bpp. This is a
separate Flash-resident font for dynamic Wi-Fi names; fixed UI strings retain
the smaller verified application font. Unsupported code points are replaced
with `?` for display only, preserving original SSID bytes when connecting.
No full glyph atlas is allocated in RAM. Regenerate with:

```bash
python3 tools/generate_network_font.py --font build/font-tools/NotoSansCJKsc-Regular.otf --converter build/font-tools/node_modules/.bin/lv_font_conv
```

### Offline Chinese speech voice

`voices/esp_tts_voice_data_xiaoxin_small.dat` is the unmodified Espressif
ESP-SR Xiaoxin small voice at commit
`78a923d29a45e4e1433b909d3da12ab5b0f994fc`, downloaded from
[the upstream voice directory](https://github.com/espressif/esp-sr/tree/78a923d29a45e4e1433b909d3da12ab5b0f994fc/esp-tts/esp_tts_chinese).
Size: 2913777 bytes; SHA-256:
`cc9a81fd716b3c07fae3ca2f802dc026081896f2e34db9b9db117d4de5a85c01`.
It is embedded once in application rodata and streamed, not loaded entirely into
RAM. The unchanged Espressif MIT license is retained at
[`components/esp_tts/LICENSE`](../components/esp_tts/LICENSE); use is restricted
to Espressif products. The matching C3 library provenance is recorded in
[`components/esp_tts/README.md`](../components/esp_tts/README.md).

### Model capability test audio

`voices/capability_probe.adpcm` is a synthetic Chinese utterance identifying the
left player, generated locally using macOS speech synthesis for this project's
API test. It contains no personal recording. The 8 kHz mono IMA ADPCM payload has
15707 samples (7854 bytes); initial predictor/index are zero. It is decoded into
standard 16-bit PCM WAV only during the isolated capability check. It is embedded
in application rodata; no microphone capture or game change occurs in this check.
SHA-256: `24e5b93fbd0171868ccc21cf0b560188add9ef8541f13034ecac12a9a18e6a0f`.

### Billiards publishing cover

[`images/publishing/doubao-billiards-cover-v1.png`](images/publishing/doubao-billiards-cover-v1.png)
is a 1086 × 1448 PNG (exact portrait 3:4), generated with the built-in image
service on 2026-11-02 for the Doubāo Billiards Scorekeeper community submission.
Creative brief: forest-green pool-table poster, ivory Chinese product title,
black eight ball, amber 07:06 score and microphone symbol; emphasize button
scoring and voice control. Original AI-generated promotional illustration, marked
as such; not a device screenshot. No third-party reference image or separate
license was supplied. Publication asset only, not embedded in firmware.
Publisher image/firmware validation passed; publication approval is pending.

### Billiards demo video

[`videos/doubao-billiards-demo.mp4`](videos/doubao-billiards-demo.mp4) is the
creator-supplied device demonstration, provided for publication on 2026-11-02.
It shows launcher navigation, button scoring, and voice control. The 79.52-second
720 × 1280 recording uses H.264 video and AAC audio, occupies 10301561 bytes,
and is linked from the bilingual [app description](../docs/assets/billiards-description.md).
It is publication material, not embedded in firmware. No separate license was supplied.

The MP4 was transcoded from the supplied MOV with FFmpeg using `libx264`,
`-preset medium -crf 24 -pix_fmt yuv420p`, AAC at 96 kbit/s,
`-map_metadata -1`, and `-movflags +faststart`. The original remains outside
the repository. Full-file decoding of the MP4 passed.

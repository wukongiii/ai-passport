<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐、电池参数等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## 电池参数（battery）

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
保存万锂新能源 500 mAh 电芯的 CW2017 参数（profile2、XIL40X、2026-10-20）。
该文件是项目提供的 `喜梨_智能工卡_万锂新能源500mAh_profile2_XIL40X_20261020.txt`
的原样副本，按维护者要求保留用于本次固件更新；原文件没有附带单独的许可说明。

格式为 80 行两位十六进制数字，每行一个字节，按寄存器顺序排列。
BSP 在 `components/bsp/src/bsp_battery.c` 中嵌入这 80 字节，主机检查会验证
源文件及数组内容完全一致。文本文件 SHA-256：
`7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`。
解码后 80 字节 SHA-256：
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`。
这是电量计参数，不是充电配置，也不能通用于所有 500 mAh 电芯；详见
[硬件指南](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md#9-cw2017-电池计)。

<a id="scoreboard-font"></a>
### 记分牌字体

`fonts/score_font_16.c`、`fonts/score_font_14.c` 与 `fonts/score_font_inventory.c` 为记分牌固定文本及可打印
ASCII 生成。字体为 Noto Sans CJK SC Regular，应用使用 16 px，紧凑输入使用 14 px，
4 bpp、不压缩、无字距调整；
许可 SIL OFL 1.1（[许可](fonts/OFL.txt)）。来源：[Noto CJK](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c)，
`Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`，SHA-256：
`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。
完整源字体和转换器缓存位于被忽略的 `build/font-tools/`。
修改 `main/score/score_strings.h`、`main/ui/app_strings.h` 或
`main/app/app_strings.h`、`main/input/text_input_strings.h` 后重新生成：

```bash
npm install --prefix build/font-tools --no-audit --no-fund lv_font_conv@1.5.3
python3 tools/generate_score_font.py --font build/font-tools/NotoSansCJKsc-Regular.otf --converter build/font-tools/node_modules/.bin/lv_font_conv
```

生成器校验源哈希。应用编译两种字号及共享码点清单，各字号在创建对应界面前校验所需码点。
此子集不覆盖任意球员姓名；实际中文渲染仍需设备验收。

### 大厅插画

`images/launcher/{eight,chase,snooker,settings,voice}.png` 是为本应用创作的原创几何
插画，遵循仓库许可证。`images/launcher/launcher_art.c` 将相同五张 176 x 96 图片
嵌入为 RGB565 常量，像素共 168960 字节，无需整图解压。使用 Pillow 13.0.0 与
已记录的 OFL 字体源（仅数字）重新生成预览图和固件像素：

```bash
python3 -m pip install --target build/art-tools Pillow==13.0.0
PYTHONPATH=build/art-tools python3 tools/generate_launcher_art.py --font build/font-tools/NotoSansCJKsc-Regular.otf
```

源文件为 `tools/generate_launcher_art.py`。固件与主机预览各编译生成的 C 一次；
PNG 用于审阅，运行时不解码 PNG。

### 网络名字体

`fonts/network_font_14.c` 使用与记分字库相同的固定版本 Noto CJK 源和 OFL 许可。
包含可打印 ASCII、U+4E00..U+9FFF 中日韩统一表意文字、CJK 标点和全角 ASCII，
字号 14 像素、2 bpp。该字库单独驻留 Flash，用于动态 Wi-Fi 名称；固定文案
继续使用经过覆盖检查的小型应用字库。未覆盖字符仅显示为 `?`，连接时保留原 SSID 字节。
不在 RAM 中分配完整字形图集。重新生成：

```bash
python3 tools/generate_network_font.py --font build/font-tools/NotoSansCJKsc-Regular.otf --converter build/font-tools/node_modules/.bin/lv_font_conv
```

<p align="right"><a href="billiards-user-guide.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Doubao Billiards Scorekeeper instructions

## Start a match

Browse with left/right; press middle to open Chinese eight-ball. Chase and Snooker are UI previews. Manual scoring and local speech work offline; AI requires Wi-Fi and a model.

| Scoreboard | Left | Middle | Right |
| --- | --- | --- | --- |
| Short press | Left player +1 | Settings; stop speech during playback | Right player +1 |
| Hold | Left player -1 | Return to launcher | Right player -1 |
| Double press | — | Start voice recording | — |

The default race is 10, adjustable in game settings, with undo. Follow the button-edge hints on subpages. App Settings > Operation adjusts double-press timing; Sound controls announcements and volume in 10% steps.

## Connect and configure with a phone

1. Open AI / Voice > Hotspot Setup and press middle, or use App Settings > Network.
2. Join the displayed device hotspot on your phone, without a password. Stay connected despite any no-internet warning.
3. Open the captive page or browse to `http://192.168.4.1/`.
4. Expand Network, select a 2.4 GHz Wi-Fi, enter its password, and choose Connect and Save. Success shows the network name and IP on the page and device. Networks reconnect automatically; up to 10 are saved, overwriting the oldest when full.
5. Rescan or enter a missing name manually. For iPhone hotspots, enable Maximize Compatibility and keep Personal Hotspot settings open while scanning.

Reopen the hotspot when its countdown expires. If disconnected, check your phone's hotspot connection. Prepare the model account while your phone has internet, then join the device hotspot to configure it.
Game Configuration > Eight-ball changes names. AI Assistant stores the nickname (default: Doubao) and prompt separately from model connections, independent of the provider.

## General AI model requirements

Choose a **multimodal LLM with Chinese understanding, audio input, and Tool / Function Calling**. It must interpret scoring requests using player names, scores, and rules, then return executable operations.

The same model endpoint must support all of these:

| Capability | Required behavior |
| --- | --- |
| API | Chat Completions compatibility, a full `/chat/completions` URL, and API Key Bearer authentication |
| Audio | Message content containing `input_audio` with Base64 WAV; recordings are 8 kHz, 16-bit mono |
| Actions | Tool / Function Calling in audio requests, returning a function name and JSON arguments using game context |
| Response | Non-streaming `choices[0].message` containing text or one `tool_calls` entry |
| Parameters | Accept `tools`, `tool_choice:auto`, `parallel_tool_calls:false`, `temperature:0`, `max_tokens:256`, and `stream:false` |

Text-only, image-only, standalone ASR/TTS, Responses-only, and WebSocket-only services are insufficient. This version has no separate ASR connection or ASR-to-LLM bridge.
**Audio output is unnecessary:** the device speaks text replies and actual scores locally.

Test the exact version and endpoint; prefer fast, concise models. There is no provider-specific thinking switch; long reasoning may exceed the response allowance.

## Dedicated tutorial: Doubao doubao-seed-2.0-mini

Use the **Doubao Seed API from Volcengine Ark**, not consumer chat-app credentials. Information checked on 2026-11-02; console labels, versions, and allowances may change.

### 1. Register and enable access

1. Register or sign in at [Volcengine](https://www.volcengine.com/). Complete identity verification and service agreements as prompted.
2. Open the [Ark console](https://ark.volcengine.com/) and select your project.
3. Find Doubao Seed 2.0 Mini in the model list and check audio understanding and Function Calling support. Follow the console to use any available trial allowance or enable the model in [Activation Management](https://ark.volcengine.com/region:cn-beijing/openManagement). Check allowance, balance, and billing terms.
4. Open its API example, choose **Chat Completions**, and copy the full request URL and exact `model` value.

The official [release announcement](https://docs.volcengine.com/docs/ark/model-release-announcement?lang=zh) lists audio-capable `doubao-seed-2-0-mini-260428`. Other Seed models, older Mini versions, and fine-tuned endpoints may differ.

### 2. Create an API Key

Create and copy a Key in the same project's [API Key Management](https://ark.volcengine.com/region:cn-beijing/apiKey). Allow the selected Model ID or Endpoint ID if permissions are restricted. Keep it out of public copy, screenshots, and source. Provider billing applies; free use is not guaranteed.

### 3. Configure the scorekeeper

Reconnect to the device hotspot, open the phone setup page, expand Model Connections, and select an empty profile:

| Field | What to enter |
| --- | --- |
| Custom name | For example, `Doubao Mini`; a display label only |
| Model ID / Endpoint ID | Copy the console example's exact `model`, such as `doubao-seed-2-0-mini-260428`; if the example uses an `ep-...` endpoint, copy that complete value |
| API URL | The standard Beijing Chat endpoint is `https://ark.cn-beijing.volces.com/api/v3/chat/completions`; for another service entry, copy the full matching URL |
| API Key | Paste your Key without a `Bearer ` prefix |

`doubao-seed-2.0-mini` is a family name, **not a complete callable Model ID**. Use the full `/chat/completions` URL, not a base URL, console page, `/responses`, or ASR address. Avoid duplicate paths. URL, model, and Key must match the service and permissions.

### 4. Save and test

Choose Save, Activate and Test. Check that **Text, Tools, and Audio** all pass before trying scoring. Testing is also available in the device's model details.
Detection sends up to three small API requests and may incur charges. It uses fixed sample audio, does not upload microphone recordings, and does not change scores. Correct failures and choose Test Saved Model again.

Save up to four profiles; switch with Set as Current. Profile labels, assistant nicknames, and Model IDs differ. Follow detection with a real voice test.

### 5. Try a voice command

In eight-ball, double-press middle to record a Chinese request to add three points to the left player; short-press to send or hold to cancel. Recording auto-sends at 10 seconds. Keep commands brief.
On success, the left score increases by three. With announcements enabled and volume above zero, the device reads the score. Also try requests to set the right score to four, undo the last operation, or ask the current score; configured player names work too.
Scores remain subject to the race target and game rules. These tests change the current match; use undo afterward if needed. Short-press middle during speech to stop playback.

## Troubleshooting

| Symptom | Check first |
| --- | --- |
| Detection fails immediately while offline | Device Wi-Fi status, network name, and IP; a phone connected to the device hotspot does not give the device internet access |
| HTTP 401 | Complete, valid Key with no added `Bearer ` prefix |
| HTTP 403 | Project, Key permissions, model activation, and the provider's specific error |
| HTTP 404 | Full API URL, exact console Model ID / Endpoint ID, and retired versions |
| HTTP 429 | Provider quota, rate limits, balance, and returned instructions |
| Text passes; Tools or Audio fails | Exact version and endpoint support, including audio and tools together |
| Reply arrives without sound | Enable Sound > Speech Announcements, raise volume, and try local playback |

Official references: [Beginner quickstart](https://docs.volcengine.com/docs/ark/quick-start-beginner?lang=zh), [Chat API](https://docs.volcengine.com/docs/ark/chat-api?lang=zh), [Audio understanding](https://docs.volcengine.com/docs/ark/audio-understanding?lang=zh), and [API Key management](https://docs.volcengine.com/docs/ark/manage-api-keys?lang=zh).

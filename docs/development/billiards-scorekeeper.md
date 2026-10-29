<p align="right"><a href="billiards-scorekeeper.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Billiards Scorekeeper

This branch starts a landscape game launcher, with Chinese eight-ball as its
first functional module, replacing the hardware-test startup with its own
320 x 240 UI. Original demo sources
remain as references but are not linked into the application. The 8 MB partition
layout is unchanged. Chase scoring, snooker, networking and AI speech are future
increments; they are not implemented in this checkpoint.

## Use and interaction contract

Two fixed players A/B count frames, with winner detection and undo of the last
32 operations. This is a match counter, not a ball tracker or referee. Player
names, ball groups, turn tracking and handicaps are not implemented.

| Screen | Short left / middle / right | Hold left / middle / right |
| --- | --- | --- |
| Scoreboard | A +1 / Settings / B +1 | A -1 / unassigned / B -1 |
| Settings | Previous / execute selection / Next | unassigned / Back / unassigned |
| Race setup | Decrease / Continue / Increase | unassigned / Back / unassigned |
| New-match confirmation | Cancel / Confirm / Back | unassigned / Back / unassigned |
| Global direction | Select 270 / Confirm / Select 90 | unassigned / Cancel / unassigned |

Board scoring applies immediately without selection or confirmation. Zero cannot
be decremented. Winning disables additions, but subtraction can correct a
mistaken winning point and reopen scoring. Game settings contains New Match and Undo. New-match clearing requires confirmation and is itself undoable.
Undo and match data survive restart.

Subpages consistently use middle short for execute/confirm and middle hold for
Back. Direction selection applies only after middle confirmation. Future modules
must preserve these meanings rather than moving confirmation to another key.
Double clicks are separate, currently unassigned events and do not act as short
clicks. Holds do not repeat. The BSP uses a 180 ms short-press window and a 500 ms
hold threshold; direct scoring still waits for gesture classification.

Default rotation is 270 degrees; Settings also offers 90 degrees, with the last
confirmed direction restored on restart. No portrait mode is exposed. The strip
follows the physical key edge: top at 270, bottom at 90. BSP hardware indices
UP/DOWN/OK map to visual left/middle/right at 270 and reverse at 90. All gestures
use this shared conversion. The user confirmed physical mapping on the device.
The separate hardware power button is not exposed as a software input and cannot
serve as a hint toggle with the documented wiring/BSP.

By default the display dims after 30 seconds and blanks after 120 seconds;
App Settings can change or disable either timer. The complete
wake gesture is consumed before scoring resumes. This saves backlight power;
it is not system sleep and does not establish battery runtime.

## Independent action registration and UI shell

`main/ui/app_actions.h` is a pure-C, allocation-free registry. Pages register
an action ID, label, enabled state, semantic role, visual slot and gesture.
`app_actions_resolve` dispatches from this same registry, so unregistered or
disabled actions cannot execute. Duplicate slot/gesture registrations are
rejected. Labels must outlive the registry.

```c
app_bindings_t bindings;
app_actions_clear(&bindings);
app_actions_register(&bindings, 0, APP_GESTURE_SHORT,
                     ACTION_PREVIOUS, "Previous", true, APP_ACTION_NAVIGATE);
app_actions_register(&bindings, 1, APP_GESTURE_SHORT,
                     ACTION_CONFIRM, "Confirm", true, APP_ACTION_CONFIRM);
uint16_t action = app_actions_resolve(&bindings, slot, gesture);
```

`app_shell` renders registered actions independently of the sport. It reserves
24 pixels per configured gesture group in short/hold/double order, omitting
empty groups. One/two/three rows leave 216/192/168 pixels of content height.
Symbols are `·` (short), `—` (hold), `··` (double). Empty cells show `--`; disabled
cells are muted. Eight-ball uses two rows for addition/subtraction. Modules draw
only inside `shell.content` and reflow to its height, without scattered button
instructions or undisplayed gestures.

Semantic backgrounds are shared across sports and gestures: green for confirm /
increase, orange-red for cancel / decrease, gray for Back, blue-gray for navigation
/ neutral actions. Labels and symbols remain alongside color. Modules register
roles, never page-specific raw colors.

## Configuration, architecture and extension

`score_config.h` / `.c` own sport defaults and bounds. Eight-ball is configured
for two players, default race to 10, range 1 through 35. Initialization, New Match
setup, controller limits, rule validation and displayed range read this config.
Existing records retain their target and score; changing defaults does not
migrate matches. Changing bounds requires a stored-record compatibility review.

- `score.h` / `score.c`: plain-C session, semantic commands, bounded snapshots,
  rule registry and versioned persistent codec.
- `chinese_eight.c`: validation, reducer and winner rule; accepts one-frame +1/-1
  adjustments and legacy frame-award commands, rejecting unsupported operations.
- `score_controller.c`: pages and action registration/dispatch.
- `score_ui.c`: game content widgets mounted inside the container-owned shell.
- `score_store.c`: backward-compatible NVS adapter.
- `games/eight_module.c`: eight-ball session owner, rule/controller adapter,
  initialization, command handling and persistence retries.
- `app/app_container.c`, `app_registry.c`: module registry, entry/exit and routing.
- `app/app_ui.c`: shared shell and single mounted content tree.
- `main.c`: hardware/input owner, battery snapshots, global backlight policy
  and LVGL synchronization.

Add a stable rule descriptor and configuration for another sport, with its
controller/presentation and tests. Signed points do not implement future rules;
ball/turn state may require a deliberate schema extension. BSP reusable hardware
logic remains separate from application state and screens.

Future voice must bind player and revision at recording start, validate and
deduplicate responses, then enqueue semantic commands to the same session owner.
`score_dispatch` rejects stale revisions and invalid operations and never calls
AI, networking, NVS or LVGL. Transport, authentication, timeouts and request
identity still need implementation.

## Persistence and failure behavior

Score changes write `billiards/session`: a fixed-width little-endian record with
version, CRC32 and undo history. The legacy direction blob is the versioned
`billiards/ui` record; global preferences now use `app_settings/config` as
described below and do not enter score history. Focus changes do not write
Flash. NVS commits run outside the LVGL lock; failed saves remain pending and
retry every five seconds, with visible failure status. Uncommitted changes may
be lost on power loss.

NVS initialization failures never trigger erase. Invalid score records are
preserved until confirming New Match permits replacement of this application's
record. Missing/invalid settings fall back to defaults; invalid records are
not replaced until the user confirms a setting. Other namespaces are not cleared.

## Fonts and verification

Fixed strings are inventoried in `score_strings.h` and `ui/app_strings.h`.
[Font provenance and regeneration](../../assets/README.md#scoreboard-font) cover
the licensed 16 px subset. Startup checks every required glyph against the actual
widget font and rejects placeholders. Arbitrary names/ASR output are unsupported.

The normal gate tests container navigation/lifecycle, mock isolation, module
re-entry and inactive save retries, plus rules, boundaries, winner correction, undo, stale commands,
configuration, old record restore, corruption, NVS failures, wake ordering,
physical-input conversion, semantic continuity and landscape BSP masks.

Actual LVGL host preview (requires LVGL 9.5.0 sources):

```bash
cmake -S tests/score_preview -B build/score-render -DSCORE_LVGL_SOURCE=/absolute/path/to/lvgl-9.5.0
cmake --build build/score-render -j 8
(cd build/score-render && ./preview)
```

It exports game pages, launcher and placeholders at both rotations, plus a
nine-action shell preview and one entry/scoring/exit path. It does not run
repetition/stress loops by default, as requested. It uses the device's 32 KB LVGL
pool; current key-screen previews observed a peak of 26048 bytes. Host rendering cannot prove physical
key placement, screen quality, timing or power consumption.

## Accepted stage checkpoint

2026-10-29: the user accepted the current interaction and visual design and
requested a local checkpoint commit. No push or release was requested.

- Build: PASS, complete gate with ESP-IDF 5.5.3.
- Host tests: PASS, including 500 stable LVGL renders.
- Device tests: PASS for application-write hash verification and 18-second boot
  observation; user accepted the revised UI and controls.
- Unverified: exhaustive page/glyph checks, power-loss/restart durability, all
  dim/blank/wake cases, long-run memory/stack margins and measured power/runtime.

Verified local archive:
`build/firmware/4c88c60ecf91f1057b88d72d8a0016e0c08c15127b5f5d6e50c6c51040454ea2/`.

- Full-image SHA-256: `4c88c60ecf91f1057b88d72d8a0016e0c08c15127b5f5d6e50c6c51040454ea2`
- Matching ELF SHA-256: `ab11160a7ccebdd9d1a8c5b6360c6d5091662f800c288659045bff8969a9e612`

Under the user's standing authorization to flash after successful builds, only
the matching 835920-byte application was written at `0x10000`, retaining the
compatible partition layout and NVS. Startup reached Ready with 215092 bytes free
heap and a 114688-byte largest block; no panic/assert/reset loop was observed.
The serial port was closed. Firmware, debug bundles and raw logs remain local
and ignored. A merged image at `0x0` may reset NVS; preserving state requires a
compatible segmented update, not whole-chip erase. This checkpoint commit does
not rebuild or reflash the accepted firmware.

## Launcher increment

Boot now always opens the game hall, with left Previous, middle Enter, right
Next. Select Eight-ball to resume the existing session. Chase, Snooker, App
Settings and AI/Voice are explicit UI-only placeholders; they can be entered
and exited but do not change data, provision networks or start services.
On the eight-ball board, middle hold returns to the hall. Existing subpage
middle-hold Back is retained; first return to the board to exit. Returning to the
hall and re-entering preserves score/history and pending saves.

`app_module.h` defines descriptors (stable ID/title/description/kind/context) and
callbacks: optional owner startup init, enter, leave, bindings, input, optional
tick, UI create and UI render. `app_registry.c` is the single module list;
`games/eight_module.c` supplies the real eight-ball descriptor. A future game
implements the callbacks and adds its descriptor to that list; it does not
modify `main.c` or the launcher renderer. A NULL ops pointer explicitly declares
a placeholder. Container-reserved action IDs are 65000 and above.

Owner callbacks run outside the LVGL lock and must not touch widgets. UI create /
render run inside it. Leave must stop all UI producers and return true before
the container destroys content; a failed leave keeps the game visible. There is
only one mounted content tree and one shell. Module tick runs even when the game
is inactive so failed saves are retried. Initialization loads legacy eight-ball
NVS once without rendering or starting a match. Neither menu browsing nor mocks
write match data.

The container owns current orientation, navigation and the application-level
entry points. In that initial increment, eight-ball reported direction changes as an effect to the
container; the existing UI preference blob/namespace remains unchanged. App
Settings was a mock in that initial increment; direction adjustment has since
moved to the global settings module described below. The AI/Voice entry and service kind establish navigation
placement only: no ASR/network service API or implementation is claimed here.
Global settings/service extraction can follow after this minimal container.

Host checks cover duplicate registration, failed entry/exit, nested Back vs Home,
placeholder isolation, inactive tick, eight-ball adapter re-entry and old saved
race restoration. Actual LVGL previews mount/unmount the real adapter once for
entry/scoring/exit and cover each important screen without stress loops.
No new commit or push is implied by this increment.

Validation on 2026-10-29:

- Build: PASS, complete gate with ESP-IDF 5.5.3 and verified debug archive.
- Host tests: PASS, including container/adapter logic and key-screen previews.
- Device tests: PASS for application-write hash verification and an 18-second
  startup observation only; physical interaction and appearance await user checks.
- Unverified: hall navigation, entry/return, score retention and both orientations
  on the physical device.

Verified local archive:
`build/firmware/7d9c7717ca4d6c05493e47eb9329f27df1d15fe151da3f4bf19b4bd35b5f8c3f/`.

- Full-image SHA-256: `7d9c7717ca4d6c05493e47eb9329f27df1d15fe151da3f4bf19b4bd35b5f8c3f`
- Matching ELF SHA-256: `ca0ebf3055aa3dee1f0ff13ea157c0a8538a235cf419190674d4c12353b34ef7`

The partition table matches the accepted checkpoint. Under the standing flash
authorization, only the verified 844000-byte application was written at
`0x10000`; NVS was not overwritten. Startup reached Ready with 214868 bytes free
heap and a 114688-byte largest block. No panic/assert/reset loop was observed,
and the serial port was closed.

## Illustrated carousel

The hall presents a horizontal carousel instead of list rows. Each registered
entry selects artwork through LVGL-independent `artwork` metadata. The center
card shows the image and title at full size; adjacent cards are smaller and
dimmer. Previous/Next uses a 220 ms eased slide with image/card scaling. The
existing three-button hints, semantic colors, wrap navigation and middle Enter
remain unchanged. Selection takes effect immediately, including Enter during
an animation. Fast reversals rebase the current offset without queuing effects.

`app_carousel` owns five reusable card widgets and one finite LVGL animation.
It cancels its animation before the container deletes content; idle cards have
no continuous animation. Pure geometry and wrap direction live in
`app_carousel_layout` and have focused host checks. Preview coverage includes
both rotations, an intermediate frame, a reversal, wrap, and leaving during a
transition; there are no repeated stress cycles.

Five original 176 x 96 RGB565 illustrations use 168960 bytes of Flash pixel
storage. They are read directly without a full-image decoder or new framebuffer.
The source generator and PNG previews are under the documented assets workflow.
The device still uses a 32 KB LVGL pool. Host previews establish layout and
callback safety only; animation smoothness and readability require the device.

Carousel validation, 2026-10-29:

- Build: PASS, complete ESP-IDF 5.5.3 gate; application 1014656 bytes.
- Host tests: PASS, focused geometry/routing tests and finite LVGL previews;
  observed peak pool usage 26048 bytes, final free pool 7944 bytes.
- Device tests: PASS for write-hash verification and 18-second startup only.
- Unverified: physical animation frame rate, appearance and rapid button switching.

Verified archive:
`build/firmware/d211c8dc88b47fcc2fe7284cbe8b783f60ab0a48159d2fb7617021a6c5dda874/`.
Full-image SHA-256: `d211c8dc88b47fcc2fe7284cbe8b783f60ab0a48159d2fb7617021a6c5dda874`.
Matching ELF SHA-256: `b54fae49a6925968ad56d8a29226440cc2e705ebba79041e8c77bd3029c2d579`.
Only the verified application was written at `0x10000`, with the unchanged
partition table and no NVS overwrite. Startup reached Ready, free heap 214772
bytes, largest block 114688 bytes, with no observed panic/assert/reset loop.
Serial observation ended and the port was closed. No commit or push was made.

The user accepted the carousel appearance and functionality on 2026-10-29 and
requested a local checkpoint commit. No push or release was requested.


## Global settings and shared ranges

App Settings is now functional: direction, brightness, auto-dim, auto-blank and
device information. Eight-ball retains only New Match and Undo. Direction is
confirmed in the global module and propagated to the container; 270/90 key
mapping and hint placement remain shared. Left/right browse settings, middle
short press edits/confirms, middle hold cancels editing or returns to the hall
when browsing. Device information shows firmware version and SOC/mV snapshots
refreshed by the owner every 30 seconds; this does not calibrate the gauge.

The shared `ui/app_range` widget displays the current value above a rail and
handle, with minimum/maximum labels below the ends. Brightness uses 20..100%
in steps of 10. Dim timeout stops are 15/30/60/120 seconds/unlimited; blank
stops are 60/120/300/600 seconds/unlimited. Unlimited is the far-right stop
and disables that timer. Ends clamp rather than wrap. A disabled dim timer does
not disable blanking, or vice versa; blanking takes priority when both expire.
Brightness previews immediately, but Cancel restores the confirmed value.
New-match race selection also uses the shared rail with its configured bounds.
It creates that widget only while its page is visible to fit the 32 KB pool.

`app_preferences` owns defaults, validation, codec and idle policy;
`app_settings_model` owns button editing independently of ESP-IDF/LVGL.
The real settings module owns persistence and UI. Confirmed preferences use
`app_settings/config`: 16-byte AP/version-1 blob, direction code, brightness,
little-endian dim/blank seconds (zero disables), reserved bytes and XOR check.
With no new record, the legacy `billiards/ui` direction is read and migrated
with default brightness/timeouts. The old direction record and match blob are
not overwritten. Malformed/unreadable records use defaults and show an error;
only explicit confirmation replaces them. Save failures retry at five-second
intervals, even after returning to the hall. No browsing/preview writes occur.

Host checks cover codec corruption, timeout boundaries, bounded adjustments,
cancel/confirm, legacy migration and NVS read/write/commit failures. Finite
LVGL previews cover range endpoints, infinity, both orientations, device info,
confirmed direction propagation and cancellation. No stress loop is used.
Wi-Fi provisioning, AI/voice and hardware sleep remain deferred.


Settings build verification, 2026-10-29: Build PASS (complete ESP-IDF 5.5.3 gate),
Host tests PASS and finite key-screen previews PASS; peak LVGL pool 25440 bytes,
final free pool 7800 bytes. Verified application size 1033904 bytes.
Archive: `build/firmware/6e5d6e239cef9b0d4f7039e179c37f02b4d01da71c3764cbf757aff4af9e595e/`.
Full-image SHA-256: `6e5d6e239cef9b0d4f7039e179c37f02b4d01da71c3764cbf757aff4af9e595e`.
Matching ELF SHA-256: `b4dfbdfef855138d234651b5f59f6dd755e64b71307c3ebbf08945b61333790f`.
Under the standing authorization, only the verified application was written at
`0x10000`, retaining the compatible partition table and existing NVS contents.
Physical slider operation, brightness preview/cancel, timeout/wake and restart
persistence remain user/device acceptance checks. No new commit or push.

Device tests: PASS for write-hash verification and 18-second startup only.
Ready reported 214516 bytes free heap and largest block 114688 bytes; no
panic/assert/reset loop was observed. Serial closed. Physical checks above
remain unverified.

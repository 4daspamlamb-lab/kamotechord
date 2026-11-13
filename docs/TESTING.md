# Tests and verification

## KamoteKeys 0.51 (one codebase, all devices)

Run everything with `WASI_SDK=/path/to/wasi-sdk-27.0-x86_64-linux tools/test-all.sh` (it copies `core/` into both device trees first).

| Suite | Device | Result |
|---|---|---|
| Host unit, behaviour and fuzz tests (`tests/host/run.sh`), incl. Clip Mode overdub/delete | SMK-37 | **6772 checks, 0 failures** (ASan + UBSan) |
| Browser-simulator integration, real SMK37 OPEN engine (`tests/integration/sim_test.mjs`) | SMK-37 | **15 checks, 0 failures** |
| Browser-simulator integration, real FM1 OPEN engine (`tests/integration/fm1_sim_test.mjs`): chord keys, LOCK, every button and HOME combination, SAVE + knobs, Key Scale, Drum, Pad Scale, Clip launch/overdub/delete and feedback, LEDs, random play with no stuck notes | FM-1 | **47 checks, 0 failures** |
| Remote Script against a mock Live API (incl. overdub / delete) | all | **15 tests, OK** |
| 32-bit embedded syntax check, all app sources, `-Wall -Wextra` | both | clean |
| JieLi pi32v2 build and flash | SMK-37 Pro by the user up to 0.48; Elite and FM-1 not yet | |

## KamoteKeys 0.37 (consolidated)

| Suite | Result |
|---|---|
| Host unit, behaviour and fuzz tests (`tests/host/run.sh`) | **6709 checks, 0 failures** (ASan + UBSan) |
| WASI integration against the SMK37 OPEN engine (`-Werror` web build) | **15 checks, 0 failures** |
| Remote Script against a mock Live API (`python3 tests/remote_script/test_kamotkeys_script.py`) | **11 tests, 0 failures** |
| 32-bit embedded syntax check | clean (only the pre-existing upstream warning in `scan.c`) |
| JieLi pi32v2 build and flash | done by the user on an SMK-37 Pro (0.30–0.36) |

Hardware findings so far, and what changed because of them:

| Finding on hardware | Change |
|---|---|
| Light shows looked frozen | phase step was 1000× too small; fixed |
| Spare pads lit "randomly" with chords | spare pads now light only for their own presses |
| Note name too heavy | lighter, smaller font |
| Pads garbled on key presses and Fader 4 moves | colours resent continuously (25 ms); selectable LED timing; Diagnostics RGB tests and frame counter |
| Clip Mode "only sends MIDI" | Remote Script reports every pad action in Live's status bar for diagnosis |

Still open: pads not fully steady in normal play; Clip Mode launching not yet confirmed in Live.

## Summary (KamoteKeys 0.30 development snapshot)

| Suite | Result | What it runs |
|---|---|---|
| Host unit, behaviour and fuzz tests (`tests/host`) | **6669 checks, 0 failures** (AddressSanitizer + UBSan) | the real KamoteKeys sources and the real `app/midi_router.c` on a PC with a stubbed SDK |
| Integration test (`tests/integration`) | **12 checks, 0 failures** | the SMK37 OPEN engine's browser build (DX7, 808, project storage, router) plus KamoteKeys, compiled with WASI SDK 27 using the same flags as `tools/Build-Web.ps1` (`-Werror`) |
| 32-bit embedded syntax check | clean (only pre-existing upstream warnings in unmodified code) | all modified app and board files, `clang --target=armv7m-none-eabi`, SDK libc headers, firmware macros (no `FM1_HOST_TEST`) |
| pi32v2 firmware build | **not run**: the JieLi toolchain is Windows-only and was not available | |
| Physical hardware | **not run** | |

## Running the tests

```sh
tools/test-all.sh                 # every suite for every device (set WASI_SDK for the simulator tests)
tests/host/run.sh                 # SMK-37 host tests only; writes build/host/*.ppm screenshots
```

Integration (needs [WASI SDK 27](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-27) and Node 18+):

```sh
W=/path/to/wasi-sdk-27.0-x86_64-linux
cd devices/smk37                  # or devices/fm1 (run tools/sync-core.sh first)
FILES=$(find app web third_party/synth -type f -name '*.c' ! -path 'app/board/*' | LC_ALL=C sort)
$W/bin/clang --sysroot=$W/share/wasi-sysroot -std=c11 -O2 -fwrapv -fno-fast-math -ffp-contract=off \
  -DFM1_HOST_TEST -DFM1_OPEN_WEB -Wall -Wextra -Werror -Wno-unused-function -Wno-array-parameter \
  -mexec-model=reactor -Iweb/engine -Iweb/synth -Ithird_party/synth -Isdk/include -Iapp/synth -Iapp $FILES \
  -Wl,--strip-all -Wl,-z,stack-size=131072 -Wl,--initial-memory=4194304 -Wl,--max-memory=33554432 -o simulator.wasm
node tests/integration/sim_test.mjs simulator.wasm build/host
```

## Coverage against the specification (Part 16)

| Spec item | Test(s) | Result |
|---|---|---|
| Every button performs its function | `test_buttons`: CC 20/21/22/24/27 press and release, CC 25 toggle, octave, ARP tap/hold, PATCH, BT | ✅ |
| Disabled buttons do nothing | CHORD/PARA taps and a 2 s hold: no MIDI (external or local), no page or mode change, LEDs off | ✅ |
| SCALE and SEQ activate different modes, LEDs correct | SEQ → Key Scale with only the SEQ LED bright; SCALE → Pad Scale with only the SCALE LED (swapped in 0.45); FX → Drum; second press → Normal | ✅ |
| SEQ idle LED matches SCALE / NOTE REPEAT (Sustain) | equal levels asserted | ✅ |
| KNOB BANK LED momentary | bright while pressed, dim after release while the CC state stays on | ✅ |
| SEQ REC controls animations, BT opens the BT menu | REC never opens BT; BT toggles the menu; ◀ acts as HOME there and sends no Undo, not even on release | ✅ |
| 2nd C = tonic; keys above **and below** follow the scale | A major (8 keys), A minor pentatonic up and down (14 keys); all 23 scales × 12 roots: anchor = tonic, strictly ascending, every note in the scale | ✅ |
| Pentatonic octave changes | anchor at octave +1/−1 | ✅ |
| Chord pads modify key output | MIN + 7 on the 2nd C in A minor pentatonic = A C E G, named "Am7"; releasing 7 stops only G | ✅ |
| Single note: one highlight; chords: all notes | rendered LCD: one green key for one note; G7 four keys, root cyan | ✅ (visual, screenshots) |
| Pad Scale: 16 notes, root highlight, ARP, Note Off | 16 on/16 off; pad 1 = D3, pad 8 = D4; root cyan / others green; ARP alternates held pads | ✅ |
| Drum: mapping, pad 1 = 36, octave, ARP, colours | full table; 48 at +1, 24 at −1, off-range disabled; kick red, hi-hat yellow; drum ARP on channel 10 only | ✅ |
| Fader 1 threshold and speed override | 9 % natural, 100 % = 127, mid ≈ 64; REC held: 0 % = half max, ~10 % very slow, velocity restored after release | ✅ |
| Fader 2 / 3 selection and hysteresis | ends = Major / Minor 6-Diminished, C / B; ≤ 1 change while dithering ±1 count at a boundary | ✅ |
| Fader 4 brightness, zero brightness | 0 % = every pad 0 even when pressed; 100 % = peak; 50 % ≈ half; spare idle ≈ 4 %; LCD dim flag at ≤ 10 % | ✅ |
| Display: labels, chord names, root/scale, octave, centring, transitions | 17 chord-naming cases plus inversion and single note; screenshots of every page; piano scroll mid/end frames | ✅ |
| BPM and ARP restored; octave resets to 0 | settings round-trip (host); **integration: deferred flash save, simulated power cycle, BPM 126 and ARP restored, octave 0** | ✅ |
| USB and BLE notes and CCs; no duplicates | AUTO → USB only while USB is ready, BLE when not; ALL → both; CCs on USB only in AUTO | ✅ |
| No stuck notes during mode changes and disconnects | 20 000-event random fuzz (keys, pads, mode/ARP/Sustain/octave/REC buttons, faders, time): every owner count back to 0 and every external Note On balanced by exactly one Note Off per port and channel | ✅ |
| Aurora LCD and RGB boot; Fader 4 = 0 prevents illumination; no old splash; main screen after boot | boot frames at 300/600/860 ms; pads stay 0 until Fader 4 has been read | ✅ (visual) |
| Internal synth plays; drums never reach the melodic synth | integration: DX7 audio energy on a key; Drum Mode audio from the drum engine on channel 10 | ✅ |
| Engine ARP never doubles notes | integration: engine ARP disabled on tracks 1 and 8 | ✅ |
| Connection and reconnection over Bluetooth | — | ❌ hardware only |

Bugs these tests found and that are fixed in this snapshot: the 37-key bitmap overflowed a 32-bit integer; "Cdim" was named "Cmdim"; releasing ◀ after it closed the BT menu sent the Undo release CC; the piano could tear between LCD bands during a scroll (the offset is now latched per redraw); the engine BPM/patch were not re-synced after a warm restart; and one band per poll made redraws depend on the control-loop rate (now time-budgeted).

## Screenshots

Rendered by the host tests from the real display code: [`docs/screenshots/`](screenshots/). `integration_lcd.png` comes from the WASI integration build.

## First-flash checklist (on hardware)

Do these once after flashing; each takes seconds.

1. Boot shows the Aurora animation, then the main screen with `KamoteKeys 0.37` top left.
2. Fader 4 at 0 before power-on → no pad lights at all during boot. Raise it → pads appear.
3. Press each of the 18 buttons and compare with [CONTROLS.md](CONTROLS.md) (action, and LED dim/bright/off). CHORD and PARA must stay dark.
4. Play the second C: the screen shows C4 (or the tonic in Key Scale Mode). Hold pad 2 + pad 9 and play a key → a minor 7th chord name.
5. FX → pad 1 must be **red** and send note 36 on channel 10 (MIDI Monitor or Ableton). If pad 9 is red instead, the pad rows are swapped: report it.
6. SCALE → pads green with cyan roots; move Fader 3 and watch the cyan pads move.
7. Fader 1 up → note velocity follows; hold SEQ REC → light show, Fader 1 changes speed.
8. BT → Bluetooth menu; pair with the Mac; confirm notes arrive over BLE with USB unplugged (AUTO).
9. Change BPM (E1) and ARP gate (E4), wait 3 s, power-cycle → both restored, octave 0.
10. Check button-LED dim level and flicker. If the dim level is too faint or flickers, report it: it is a single constant in `led_scan()` (`app/board/scan.c`).

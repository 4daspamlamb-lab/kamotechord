# KamoteKeys: notes for Claude

Performance firmware for the M-VAVE SMK-37 Pro/Elite (on SMK37 OPEN) and FM-1 (on FM1 OPEN), plus an Ableton Live 12 Remote Script. One codebase builds all three.

## Rules
- Git: push only to `origin` (4daspamlamb-lab/kamotechord), branch `kamotekeys`. Never push to `upstream` (m-vave/smk37_open).
- Never edit `devices/*/sdk/` (hash-locked by `manifest.json`, incl. `Build.ps1`). Never edit the synced copies under `devices/*/app/` that `.gitignore` lists; edit `core/` instead.
- The user builds on Windows (Parallels, toolchain `C:\JL\pi32`) and flashes in Chrome/Brave on a Mac. Every release reply includes the PowerShell blocks from `build/KamoteKeys-<v>/BUILD.txt` and the flash steps at its end.
- Name: "KamoteKeys" (one word). Code identifiers use `kamotkeys`/`kk_` (historic; keep).

## Where things are
| What | File |
|---|---|
| Version (one place) | `core/kamotkeys/kk_config.h` `KAMOTKEYS_VERSION` |
| Constants: buttons `KK_BTN_*`, modes, MIDI channels/CCs, step protocol | `core/kamotkeys/kk_config.h` |
| Performance core (modes, routing, faders, public actions, init/tick) | `core/kk_controls.c` |
| ↳ ARP + Note Repeat | `core/kamotkeys/kk_ctl_arp.inc` |
| ↳ Buttons, combos, long presses | `core/kamotkeys/kk_ctl_buttons.inc` |
| ↳ Keys/pads press-release, Sustain, velocity curve | `core/kamotkeys/kk_ctl_keys.inc` |
| ↳ Engine guard, saved settings | `core/kamotkeys/kk_ctl_engine.inc` |
| LCD (state, frame pump, entry points) | `core/screen.c` |
| ↳ drawing primitives / main screen / pages + boot / input | `core/kamotkeys/kk_scr_{canvas,main,pages,input}.inc` |
| LED decisions (pads + button LEDs) | `core/kamotkeys/kk_led.c` |
| MIDI in/out, clip + step feedback from Live | `core/kamotkeys/kk_midi.c` |
| Scales, chords | `core/kamotkeys/kk_theory.c` |
| Device values, key maps (Step keys, Clip keys) | `devices/{smk37,fm1}/app/kamotkeys/kk_device.h` |
| SMK-37 RGB pads, ripples, boot wave | `devices/smk37/app/kamotkeys/kk_led_rgb.c` |
| FM-1 input adapter (keys→pads, buttons, LEDs) | `devices/fm1/app/kamotkeys/kk_fm1_input.c` |
| Remote Script (Clip Mode, transport) / step sequencer | `remote_script/KamoteKeys/KamoteKeys.py` / `StepSequencer.py` |

The `.inc` files are `#include`d by `kk_controls.c` / `screen.c` at their original position (one compilation unit), because the locked SDK build gives `-Oz` only to those file names. Add new firmware code to an existing `.inc`/`.c` or a new `.inc`; a new `.c` in `app/` compiles at `-O2` (bigger).

`core/` is copied into the devices by `tools/sync-core.sh` (Windows: `tools/Sync-Core.ps1`, run by `build.bat`): `core/kk_controls.c` → `app/smk37_controls.c` (SMK) / `app/kk_controls.c` (FM-1).

## Docs: one place per fact
- SMK-37 buttons, faders, knobs, modes, LEDs, light show: `README.md` (user guide).
- FM-1 differences: `docs/FM1.md`. Physical layout/indexes only: `docs/CONTROLS.md`.
- Every CC/note: `docs/MIDI_MAPPING.md`. Ableton setup, Clip/Step Mode use: `docs/ABLETON_REMOTE_SCRIPT.md`.
- Release notes: `CHANGELOG.md` (newest first, `## KamoteKeys <v> (development snapshot)`).
- Do not write version numbers or test counts into docs other than the CHANGELOG.

## Tests
- Everything: `WASI_SDK=/tmp/claude-0/wasi/wasi-sdk-27.0-x86_64-linux tools/test-all.sh` (host ASan/UBSan tests, ARM syntax check with `-Werror`, Remote Script, both browser simulators). Without WASI_SDK the simulators are skipped. If the WASI SDK is missing, fetch wasi-sdk-27.0-x86_64-linux from the WebAssembly/wasi-sdk GitHub releases.
- Host tests only (fast): `tests/host/run.sh`. Split by feature: `tests/host/test_{controls,modes,live,features}.c`, helpers (`btn`, `tap`, `key`, `pad`, `fader`, `frames`, `settle`, `show_on/off`, `out_count`, `last_out`, `shot`) in `tests/host/helpers.h`. New test group: add `void test_x(void)` to a file, declare it in `helpers.h`, call it from `test_main.c`.
- Remote Script: `python3 tests/remote_script/test_kamotkeys_script.py`. FM-1 behaviour: `tests/integration/fm1_sim_test.mjs`.

## Release (after the feature is committed)
1. Add the `CHANGELOG.md` section for the new version.
2. `tools/release.sh 0.NN` (sets the version, runs all tests, writes `build/KamoteKeys-0.NN.zip` + `BUILD.txt`). Commit the version bump, push `origin kamotekeys`, then `SKIP_TESTS=1 tools/release.sh` so the source zip contains HEAD.
3. Send the zip to the user with the blocks from `BUILD.txt`.

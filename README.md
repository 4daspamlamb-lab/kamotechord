# KamotKeys 0.41 for M-VAVE SMK-37 Pro / Elite

KamotKeys is a performance firmware for the M-VAVE SMK-37 Pro and SMK-37 Elite, built on **SMK37 OPEN V01**. It keeps SMK37 OPEN's hardware drivers, native USB MIDI, Bluetooth LE MIDI, Bluetooth audio, flash storage and the official browser updater. It replaces the sequencer/editor application with KamotKeys: chord pads, a scale-remapped keyboard, 16-pad scale and drum modes, ARP and Note Repeat, four dedicated faders, Ableton Live transport buttons, an animated piano display, RGB pad lighting and an Aurora boot.

> **Status: development snapshot (consolidated 0.37).** Builds with the JieLi toolchain (0.30–0.36 were built and flashed to an SMK-37 Pro). PC tests: 6709 unit/fuzz checks, 15 integration checks against the SMK37 OPEN engine, 11 Remote Script tests against a mock Live API. **Open on hardware:** pad LEDs are not yet fully steady, and Clip Mode launching in Live is not yet confirmed. See [Hardware validation](#hardware-validation).

This release is one clean change on top of the unmodified SMK37 OPEN V01 (`kamotkeys-0.37.diff`). Feature history is in the [CHANGELOG](CHANGELOG.md).

![Key Scale Mode, A Minor Pentatonic, Am7](docs/screenshots/02_key_scale_Am7.png)

## Contents

- [Hardware compatibility](#hardware-compatibility)
- [Controls at a glance](#controls-at-a-glance)
- [Faders](#faders)
- [Performance modes](#performance-modes)
- [ARP and Note Repeat](#arp-and-note-repeat)
- [Knobs (encoders)](#knobs-encoders)
- [Screens](#screens)
- [RGB and button lighting](#rgb-and-button-lighting)
- [Bluetooth and USB MIDI](#bluetooth-and-usb-midi)
- [Ableton Live setup](#ableton-live-setup)
- [Saved settings](#saved-settings)
- [Build](#build) · [Flash](#flash) · [Recovery](#recovery)
- [Hardware validation](#hardware-validation)
- [Project layout](#project-layout)
- [License and attribution](#license-and-attribution)

Detailed references: [MIDI mapping](docs/MIDI_MAPPING.md) · [Button and pad diagrams](docs/CONTROLS.md) · [Build, flash and recovery](docs/BUILD_FLASH_RECOVERY.md) · [Tests and verification](docs/TESTING.md) · [Changelog](CHANGELOG.md)

## Hardware compatibility

| Model | Status |
|---|---|
| SMK-37 Pro | Supported by source (`build.bat pro`). Shares the SMK37 OPEN board driver. |
| SMK-37 Elite | Supported by source (`build.bat elite`). |

Both models need SMK37 OPEN's update path (the model-specific workbench page). Pro and Elite packages are **not** interchangeable. The firmware keeps SMK37 OPEN's update identity (`SMK-37 Pro_8000001` / `SMK-37 Elite_8000001`, release `V01`) so the official workbench recognises the device. The KamotKeys version is a separate constant, `KAMOTKEYS_VERSION` in [`app/kamotkeys/kk_config.h`](app/kamotkeys/kk_config.h).

## Controls at a glance

```
 LEFT BLOCK                         RIGHT BLOCK
 ┌────────────┬────────────┐        ┌──────┬────────────┬───────┬───────┬───────┬──────┐
 │ KNOB BANK  │ FADER BANK │        │ ARP  │ NOTE REPEAT│ SCALE │ CHORD │ GLOBE │  BT  │
 │ Sess.Rec   │ Capture    │        │ arp  │ repeat     │ KeyScl│ Clip  │ Stop  │ menu │
 ├────────────┼────────────┤        ├──────┼────────────┼───────┼───────┼───────┼──────┤
 │   ◀ LEFT   │  RIGHT ▶   │        │PATCH │   PARA     │  FX   │  SEQ  │ PLAY  │ REC  │
 │   Undo     │  Redo      │        │patch │    ––      │ Drum  │PadScl │ Start │ show │
 ├────────────┼────────────┤        └──────┴────────────┴───────┴───────┴───────┴──────┘
 │   OCT −    │   OCT +    │        –– = disabled (dark, no action, no MIDI)
 └────────────┴────────────┘
```

| Button | Short press | Hold |
|---|---|---|
| ARP | ARP on/off | 0.6 s: ARP settings page |
| NOTE REPEAT | Note Repeat on/off | 0.6 s: Note Repeat page |
| SCALE | Key Scale Mode on/off | |
| CHORD | **Clip Mode** on/off (8 × 2 Ableton clip launcher) | |
| GLOBE | Ableton **Stop** (CC 21) | |
| BT | Open/close the Bluetooth menu | 1.5 s: diagnostics |
| PATCH | Open/close the patch page | |
| PARA | **Disabled** | |
| FX | Drum Mode on/off | |
| SEQ | Pad Scale Mode on/off | |
| SEQ PLAY | Ableton **Start** (CC 20) | |
| SEQ REC | Light show: each press selects the next pattern; it runs while held | |
| KNOB BANK | Ableton **Session Record** (CC 25, 127 press / 0 release) | |
| FADER BANK | Ableton **Capture MIDI** (CC 22) | |
| ◀ LEFT | Ableton **Undo** (CC 24). In the BT menu: HOME (back) | |
| RIGHT ▶ | Ableton **Redo** (CC 27). In the BT menu: SAVE (OK) | |
| OCT − / OCT + | Octave −4 … +4. Keyboard starts at **−2**; in Pad Scale/Drum Mode they move the pad octave (starts at 0) | |

All Ableton commands are on **MIDI channel 15**. The full table, with the pad map and the button-index verification, is in [docs/CONTROLS.md](docs/CONTROLS.md).

## Faders

| Fader | Function |
|---|---|
| 1 | **Velocity.** 0–10 %: natural key/pad velocity. Above 10 %: fixed velocity proportional to position, 100 % = 127. Applies to keys, Key Scale, Pad Scale, Drum and spare pads, ARP and Note Repeat. It is read at Note On only and never changes sounding notes or Note Off. **While SEQ REC is held:** light-show speed. |
| 2 | **Scale** (23 scales, bottom = Major, top = Minor 6-Diminished), with 2-count hysteresis against ADC noise. |
| 3 | **Root key** C … B, with hysteresis. |
| 4 | **Pad brightness** multiplier: 0 % = pads fully off, 50 % = half, 100 % = full. At ≤ 10 % the LCD also drops to half apparent brightness (software dimming). |

Scales: Major, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic Minor, Melodic Minor, Major Pentatonic, Minor Pentatonic, Blues, Phrygian Dominant, Double Harmonic, Hirajoshi, Whole Tone, Diminished Whole-Half, Diminished Half-Whole, Bebop Dominant, Bebop Major, Bebop Minor, 6-Diminished, Minor 6-Diminished. One shared engine ([`kk_theory.c`](app/kamotkeys/kk_theory.c)) serves the keys, pads, chords, LEDs and display.

## Performance modes

The modes are mutually exclusive. Pressing a mode button switches to that mode; pressing the active one returns to **Normal**. Each switch releases the notes it affects.

### Normal Mode (default)

- **Keyboard:** chromatic, channel 1. At the power-on octave −2 the lowest key is C1 (MIDI 24); at octave 0 it is C3 (48).
- **Chord pads** (pads 1–4, 9–13) are modifiers: hold one or more, then play a key. The chord root follows the key you play.

  | Pad | Modifier | Pad | Modifier |
  |---|---|---|---|
  | 1 | MAJ (triad) | 9 | 7 (adds ♭7, or the diatonic 7th) |
  | 2 | MIN | 10 | MAJ7 |
  | 3 | DIM | 11 | 6 |
  | 4 | SUS4 | 12 | 9 (adds 9th; with 7/MAJ7 gives 9 chords) |
  |   |   | 13 | **LOCK**: latch the held modifiers (see below) |

  **LOCK (pad 13):** hold your modifiers and tap 13 → they stay on after you let go, so one hand plays chords from the keys. While locked, tapping a modifier pad toggles it; tap 13 again to unlock. Pad 13 and the latched modifiers stay lit.

  Holding only extension pads (9–12) builds the chord **diatonically** from the selected scale and root: in C major, 7 on G gives G7 and 7 on B gives Bm7♭5. Changing modifiers while keys are held re-voices those keys: common tones keep sounding, the others stop or start.
- **Spare pads** 5–8 and 14–16 send notes 84–90 (C6–F♯6) on channel 1, each with its own colour.

### Key Scale Mode (SCALE)

The **second physical C** plays the selected tonic, at octave 0 the tonic in the octave starting at C4 (e.g. A → A4 = 69). Successive white keys play successive scale degrees, upwards and **downwards**. Pentatonic and 6/8-note scales wrap correctly. Black keys are silent. Chord pads and spare pads work as in Normal Mode.

| Physical key | A Major | A Minor Pentatonic |
|---|---|---|
| C (lowest) | A3 | E3 |
| … | … | … |
| B below anchor | G♯4 | G4 |
| **2nd C (anchor)** | **A4** | **A4** |
| D | B4 | C5 |
| E | C♯5 | D5 |
| F | D5 | E5 |
| G | E5 | G5 |
| A | F♯5 | A5 |
| B | G♯5 | C6 |
| next C | A5 | D6 |

### Clip Mode (CHORD)

Pads 1–8 launch clip 1 and pads 9–16 clip 2 of Live tracks 1–8, on channel 16, with LEDs that follow Live (empty very dim, stopped dim blue, playing green, queued blinking amber, recording red). Needs the [KamotKeys Remote Script](docs/ABLETON_REMOTE_SCRIPT.md). Keys keep playing normally.

### Pad Scale Mode (SEQ)

All 16 pads play consecutive scale degrees in pad order (pad 1 = tonic at octave 3, e.g. D3), following Fader 2/3 and the octave buttons. Channel 1. Root pads are **cyan** and the other pads **green**, both dim when idle and bright when pressed. The screen shows the note, root, scale and octave in a 2 × 8 pad map. The keyboard keeps playing chromatically.

### Drum Mode (FX)

All 16 pads send drum notes on **channel 10**. The layout is vertical: pad 1 = 36, pad 9 = 37, pad 2 = 38, and so on (full table in [docs/MIDI_MAPPING.md](docs/MIDI_MAPPING.md)). Octave buttons transpose by 12, so pad 1 is 24 at −1 and 48 at +1. Out-of-range pads are disabled. Pads are coloured by the GM family of the note they currently play: kick red, snare orange, hi-hat yellow, toms blue, rim purple, clap pink, cymbals teal, other percussion green. The internal sound comes from track 8's drum engine; KamotKeys keeps that track on a drum source so channel 10 never plays the melodic synth.

## ARP and Note Repeat

**ARP** (short press of ARP). One clock, the shared BPM.

- Normal and Key Scale: arpeggiates held keys, including generated chords.
- Pad Scale: arpeggiates held pads (channel 1). Drum: arpeggiates held drum pads (channel 10).
- Parameters (ARP page, or the main-screen knobs): **Mode** NORMAL/LATCH, **Direction** UP, DOWN, UP-DOWN, RANDOM, ORDER (as played), **Division** 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 (default 1/8), **Gate** 10–100 % (default 75 %), **Swing** 0–50 %, **Octaves** 1–4.
- Toggling ARP while notes are held moves them safely between direct play and the arpeggiator.

**Note Repeat** (short press of NOTE REPEAT) retriggers every held key or pad that is not being arpeggiated, at the Repeat division (default 1/16) with a 50 % gate.

KamotKeys turns off the SMK37 OPEN engine's own per-track ARP so that notes are never doubled.

## Knobs (encoders)

Encoders are numbered E1–E8, left to right; SMK37 OPEN calls them K1–K4, SELECT, PAGE, PRESET, MASTER.

| Knob | Main screen | ARP page | Patch page | BT menu |
|---|---|---|---|---|
| E1 | BPM (30–300) | Mode | Patch | Bluetooth on/off |
| E2 | Patch (DX7 1–128) | Direction | Internal synth on/off | BT audio on/off |
| E3 | ARP division | Division | MIDI OUT ports | BLE role DEVICE/HOST |
| E4 | ARP gate | Gate | | Clock INT/MIDI |
| E5 | ARP direction | Swing | | Host list: device |
| E6 | ARP swing | Octaves | | |
| E7 | Note Repeat division | | | Action |
| E8 | **Master volume** (always) | | | |

Changes on the main screen appear as a short message in the status line.

## Screens

| | |
|---|---|
| ![Aurora boot](docs/screenshots/00_boot_600ms.png) | **Aurora boot** (~1.1 s): flowing teal, cyan, purple and pink ribbons, the KamotKeys wordmark, the version and a start-up line, drawn in a fixed band and never full-screen per frame. The pads show an Aurora Flow gated by Fader 4, then fade into the normal colours. |
| ![Chord](docs/screenshots/07_main_chord_G7.png) | **Main screen:** version label, mode, octave, BPM, the chord or note name, root and scale, the piano, and ARP/Repeat/velocity status. The piano follows the **actual MIDI note list**: a single note lights one key (green); in a chord the root is cyan and the other tones orange. The view re-centres with a ~110 ms ease-out pixel scroll that can redirect mid-move. Key Scale Mode adds dots for scale degrees. |
| ![Pad scale](docs/screenshots/04_pad_scale.png) | **Pad Scale / Drum:** a 2 × 8 pad map with note names or drum labels and live highlights. |
| ![BT](docs/screenshots/12_bluetooth_menu.png) | **Bluetooth menu:** SMK37 OPEN's standard menu (see below). |

More: [ARP page](docs/screenshots/11_arp_page.png), [Patch page](docs/screenshots/10_patch_page.png), [Diagnostics](docs/screenshots/13_diagnostics.png) (no KamotKeys branding), [dimmed LCD](docs/screenshots/14_main_dimmed.png), [light-show patterns](docs/screenshots/15_lightshows.png).

## RGB and button lighting

A single renderer ([`kk_led.c`](app/kamotkeys/kk_led.c)) decides every pad's colour and level (mode colour, then idle/pressed level, then the Fader 4 multiplier) and hands it to the board driver. Levels: chord pads 12 % idle, spare pads 4 %, scale/drum pads 10 %, 100 % when pressed or sounding. The peak is 160/255, scaled by Fader 4, and every channel value is rounded to a multiple of 4 (see below).

**How the pads are driven (lessons from hardware testing on an SMK-37 Pro):**
- Root cause (0.41): the gap in front of each pad frame (SMK37 OPEN: 36 zero bytes, about 45 us) was shorter than the LEDs' reset time (about 280 us), so a stray pulse when the shared SPI line switched from the LCD to the pads shifted the whole frame by a bit. Pad 1 blinked green, and colour bits spilled between pads at the start and end of the chain. KamotKeys drives the line low before switching and sends 320 zero bytes (about 400 us) before every frame.
- Channel values are still rounded to multiples of 4 (a lit channel is at least 4), which made the remaining errors harmless before the root cause was found.
- A low peak brightness (64) left the dim idle colours too few steps to keep their hue, so the peak is 160.
- Changed colours go out at once and the whole frame is resent every 25 ms (at most 50 frames per second).
- The LED bit timing can be chosen on the Diagnostics page (E2, 8 presets, saved; 0 = SMK37 OPEN original, and all presets worked on the test unit).

Button LEDs are single-colour on/off LEDs. KamotKeys adds a **dim** level (1/8 duty in the existing LED multiplex interrupt):

- enabled buttons are dim when idle and bright while pressed;
- ARP, NOTE REPEAT, SCALE, SEQ, FX and CHORD stay bright while their function or mode is active, and **SCALE shows only Key Scale, SEQ shows only Pad Scale**;
- KNOB BANK lights only while pressed; SEQ REC while the light show runs; BT while its menu is open;
- PARA is completely dark.

**Light show** (hold SEQ REC; each new press selects the next pattern): Rainbow sweep, Vertical pulse, Spiral/perimeter chase, Checkerboard, Diagonal wave, Breathing colours, Opposing chases, Colour wave. While SEQ REC is held the pads send no MIDI, Fader 1 sets the speed (about 0.03–2 cycles per second; 0 % = 1 per second) and Fader 4 the brightness. Changing the speed never restarts the pattern.

## Bluetooth and USB MIDI

KamotKeys uses SMK37 OPEN's native transports unchanged: USB MIDI and BLE MIDI. In DEVICE (peripheral) mode the device advertises as `SMK37 Pro Open_BLE` / `SMK37 Elite Open_BLE`.

**BT button → the standard SMK37 OPEN Bluetooth menu:**

- E1 Bluetooth on/off, E2 BT audio, E3 BLE role DEVICE/HOST, E4 clock INT/MIDI;
- E7 selects the action (`MIDI DEVICE LIST (HOST)` / `FORGET MIDI PAIRS`); **RIGHT ▶ = OK (SAVE)**, **◀ LEFT = back (HOME)**, BT = exit;
- in the host list: E5 selects a device, E7 chooses CONNECT / SCAN / DISCONNECT / SETTINGS, ▶ confirms;
- forgetting pairs asks for confirmation (▶ yes, ◀ no). Remove the old entry on the computer too.

While the menu is open, ◀ and ▶ do not send Undo/Redo, and their releases are swallowed as well.

**Pairing on a Mac:** open *Audio MIDI Setup → Window → Show MIDI Studio → Bluetooth* (the BLE configuration icon), find `SMK37 … Open_BLE` and press **Connect**.

**Output ports (Patch page, E3 MIDI OUT):**

| Setting | Behaviour |
|---|---|
| **AUTO** (default) | USB if USB MIDI is connected, otherwise BLE. A Mac connected both ways never receives each command twice. |
| ALL | USB and BLE (and DIN where fitted). |
| USB / BLE | That port only. |

Note Offs always go to every port, so a port change between Note On and Note Off cannot leave a stuck note. The router also releases notes on a port that disconnects. The engine's MIDI clock output follows SMK37 OPEN's settings.

## Ableton Live setup

1. *Settings → Link, Tempo & MIDI.* Enable **Track** for the KamotKeys input you use (USB *or* Bluetooth).
2. Install the [KamotKeys Remote Script](docs/ABLETON_REMOTE_SCRIPT.md) and select it as a Control Surface with Input and Output on the keyboard's port. It handles Clip Mode and these buttons without any MIDI mapping:

   | Live action | Button | CC (ch. 15) |
   |---|---|---|
   | Play | SEQ PLAY | 20 |
   | Stop | GLOBE | 21 |
   | Capture MIDI | FADER BANK | 22 |
   | Undo | ◀ | 24 |
   | Session Record | KNOB BANK | 25 |
   | Redo | ▶ | 27 |

   Buttons send 127 on press and 0 on release. Remove old manual mappings of these buttons, or each press acts twice. Without the script you can still MIDI-map the buttons (⌘M).
3. **Avoid duplicates:** with MIDI OUT on AUTO, only one port carries data. If you choose ALL, enable Track/Remote for **only one** of the USB and Bluetooth inputs in Live, otherwise every note and command arrives twice.
4. Keys and scale/spare pads use channel 1 and drums channel 10. Set a track's MIDI From to *All Channels* or to the specific channel.
5. To hear only Live, turn **Internal synth** off on the Patch page (E2), or turn MASTER (E8) down.

## Saved settings

Saved automatically, about 1.5 s after the last change and only while nothing is playing, in SMK37 OPEN's journaled settings sector: **BPM, ARP on/off, mode, direction, division, gate, swing, octaves, Note Repeat division, DX7 patch, MIDI OUT ports, internal synth on/off, pad LED timing**. The **octave is not saved**: keys start at −2 and pads at 0 on every power-on. Scale, root, velocity and brightness come from the physical faders. Bluetooth settings are stored by SMK37 OPEN as before.

## Build

Windows x64 with PowerShell and the [JieLi 2.5.3 Windows toolchain](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) (PI32v2 clang 4.0.1, `r3`), exactly as SMK37 OPEN. The SDK manifest verifies the toolchain and SDK hashes; do not modify `sdk/`.

```powershell
cd C:\path\to\kamotkeys-0.37-smk37_open          # a local C: folder, not \\Mac\... (cmd cannot run there)
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL = 'https://localhost/kamotkeys-0.37-source.zip'   # any https URL; publish the real source if you share builds
.\build.bat pro --package
```

Needs the x64 Visual C++ Redistributable (for the bundled web compiler) and, on first run, downloads WASI SDK 27 (set `FM1_WASI_SDK` to reuse it). A copy-and-paste version for Parallels is in [docs/BUILD_FLASH_RECOVERY.md](docs/BUILD_FLASH_RECOVERY.md).

The output is `build\official-web\pro\SMK37-Open-Source.html`, the offline workbench with the KamotKeys firmware embedded. The build log prints the image, VM and user sizes and stops if the package exceeds 1 MiB. For the firmware image alone: `.\sdk\Build.ps1 -Model pro -FirmwareOnly -Toolchain 'C:\JL\pi32'`. Full details: [docs/BUILD_FLASH_RECOVERY.md](docs/BUILD_FLASH_RECOVERY.md).

PC-side tests (no toolchain needed): `tests/host/run.sh`; see [docs/TESTING.md](docs/TESTING.md).

## Flash

1. Back up projects, voices and samples with the official SMK37 OPEN workbench first.
2. Open `SMK37-Open-Source.html` for **your** model in desktop Chrome or Edge, connect USB and allow MIDI/SysEx.
3. Use **Device update**: **Start update** when the keyboard already runs SMK37 OPEN/KamotKeys; tick **Force flash** only when coming from M-VAVE stock V16. Firmware based on other projects (e.g. ChoralRoot) must be returned to stock V16 first. Keep USB connected until it finishes. Never flash a Pro package onto an Elite or the reverse.

## Recovery

- **Back to official SMK37 OPEN:** flash the official workbench from <https://open.m-vave.com/smk37-workbench> (Device update). KamotKeys uses the same update identity, so the switch is a normal update.
- **Back to factory V16:** in the workbench (official, or the KamotKeys build), use **Restore factory V16**. It installs the unmodified M-VAVE V016 package (hash-verified in `sdk/firmware/`). Power-cycle after resource verification.
- **If the firmware does not start:** the SMK37 loader remains in place (KamotKeys does not touch boot, VM/BTIF or calibration areas). Reconnect and let the workbench find the loader, confirm the physical model, and flash again. See [docs/BUILD_FLASH_RECOVERY.md](docs/BUILD_FLASH_RECOVERY.md).

## Hardware validation

| Area | Status |
|---|---|
| pi32v2 build, link, package (1 MiB limit, SRAM/BLE/USB checks) | ✅ built by the user with the JieLi toolchain |
| Flashing via the workbench (Force flash from stock V16; Start update afterwards) | ✅ on an SMK-37 Pro |
| Music logic, MIDI ownership (no stuck notes), routing | ✅ PC tests and fuzz; ✅ engine integration |
| LCD screens, font, boot | ✅ on hardware |
| **Pad LEDs** | ✅ **verified on an SMK-37 Pro (0.41):** steady, correct colours in every mode after the full-length LED reset fix |
| **Clip Mode in Live** | ⚠️ the keyboard sends the clip notes and receives clip states; launching in Live not yet confirmed. The Remote Script reports every pad in Live's status bar |
| Remote Script inside Ableton | loads and sends clip states (pads show green/blinking); actions not yet confirmed |
| Button map, faders, octave, LOCK | used on hardware; no issues reported |

## Project layout

| Path | Contents |
|---|---|
| `app/kamotkeys/kk_config.h` | version constant, channels, CC numbers, button ids |
| `app/kamotkeys/kk_theory.*` | shared scale/chord engine, key/pad/drum maps, chord naming |
| `app/kamotkeys/kk_midi.*` | central owner-counted MIDI note engine and port policy |
| `app/kamotkeys/kk_led.*` | pad/button LED renderer, light shows, Aurora pads |
| `app/kamotkeys/kk_perform.h` | performance state shared by the modules |
| `app/smk37_controls.c` | performance core: modes, sources, chord pads, ARP, Repeat, faders, buttons, settings (keeps the SDK build script's flags for this file) |
| `app/screen.c` | LCD: renderer, pages, Bluetooth menu, boot; SDK entry points (kept native by the SDK build script) |
| `app/kamotkeys/kk_web_compat.c` | shims so the browser workbench still builds (web builds only) |
| `app/midi_router.c`, `app/project.c`, `app/board/*` | SMK37 OPEN, with small marked KamotKeys additions |
| `tests/` | host tests, integration test |
| `docs/upstream/` | the original SMK37 OPEN README (EN/中文) |

Removed: SMK37 OPEN's sequencer/editor UI (track, step, FM edit, mixer, effects, project and preset pages) and its panel control layer. The underlying engine library, project storage, sample bank, DX7 library and workbench transfer code remain, so the official workbench's backup and restore features keep working.

## License and attribution

KamotKeys is a modification of **SMK37 OPEN V01** (© 2026 珠海笙科智能科技有限公司 / M-VAVE and contributors), distributed under the same terms: application sources under [GPLv3 with the SDK linking exception](LICENSE.md). The native SDK and factory firmware are binary components under their own terms; see [THIRD_PARTY.md](THIRD_PARTY.md). Keep these files when redistributing, and publish the matching source archive (the `FM1_SOURCE_URL` the build requires). KamotKeys is not affiliated with or endorsed by M-VAVE.

# Changelog

## KamotKeys 0.44 (development snapshot)

- **Pad sensitivity** (Patch page, E4, saved): 1 = harder ... 3 = linear (as before) ... 5 = light hits give high velocity. A velocity curve on the pads only; keys are unaffected and the pad trigger threshold (factory calibration) is unchanged. Fader 1 above 10 % still overrides velocity.

## KamotKeys 0.43 (development snapshot)

- **Spare pads 5-8 and 14-16 are mappable buttons:** CC 102-108 on channel 15 (127 press, 0 release) instead of notes 84-90. They light while held, never use ARP or Note Repeat, and are released cleanly on mode changes.
- Resting pads slightly dimmer than 0.42: chord 25 %, spare 18 %, scale/drum 22 %, Pad Scale roots 50 %.

## KamotKeys 0.42 (development snapshot)

### Look and feel
- Resting pads much brighter: chord 35 %, spare 25 %, scale/drum 30 % (were 12 / 4 / 10 %); Clip Mode states brighter too.
- Chord pad colours by meaning: MAJ warm amber, MIN blue, DIM purple, SUS4 teal; extensions 7, MAJ7, 6, 9 as one magenta-to-pink family; LOCK white.
- Pad Scale roots rest at 65 % in a whiter cyan, clearly different from the other notes at rest.
- SEQ REC light show crossfades in (~200 ms) and out (~150 ms) instead of jumping in one frame (seen as a flash).

### LOCK
- While locked, tapping a pad switches within its row (pads 1-4: MAJ/MIN/DIM/SUS4; pads 9-12: 7/MAJ7/6/9). Pads combine only when pressed while another pad of the same row is held. Tapping the active pad clears its row.

## KamotKeys 0.41 (development snapshot)

### Pad LEDs: root cause
- Hardware report (0.40): colours correct, but pressing chord pads made other pads flicker, pads 1-4 and 9-12 were miscoloured and pad 1 blinked green, while spare pads were fine. Those pads sit at the start and end of the LED chain.
- Cause: the reset gap before each pad frame was ~45 us (SMK37 OPEN's 36 zero bytes); these LEDs need ~280 us. A stray pulse when SPI1 is switched from the LCD to the pads therefore became an extra leading bit, shifting the frame (first LED's green MSB = pad 1 green; low bits spilling into the next channel's top bit = "crazy" colours, hidden by the multiple-of-4 rounding).
- Fix: clock a zero byte with the LCD deselected before switching SPI1 to the pads, and lead every frame with 320 zero bytes (~400 us). Multiple-of-4 rounding stays as a safety margin.
- **Verified on hardware:** pad behaviour correct on an SMK-37 Pro.

## KamotKeys 0.40 (clean rebuild)

One change on top of the unmodified SMK37 OPEN V01 again, with the pad LED settings the hardware has proven:

- **Pad values rounded to multiples of 4** (restored). Hardware comparison: 0.37 (no rounding) flashed, 0.38 (rounding) was calm, 0.39 (no rounding) flashed on key presses again. BIT TEST could not show this because the affected values (bits 0-1) are practically invisible.
- **Peak brightness 160** (kept from 0.39), so dim idle colours keep their hue.
- Everything else as 0.39.

## KamotKeys 0.39 (development snapshot)

### Pad LEDs
- Hardware **BIT TEST** (0.38): every value 1-128 displayed correctly on every pad, so the LED data path is fine. 0.38's multiple-of-4 rounding is removed.
- Peak brightness back to 160. At 64 (0.34-0.38) the dim idle colours had so few steps that they lost their hue (the blue MIN pad became (4,4,8), nearly grey; orange became yellow), which looked like wrong colours.

## KamotKeys 0.38 (development snapshot)

### Pad LEDs
- Hardware videos (0.37): the Diagnostics test bars were steady and correct, but normal colours showed wrong, bright colours. The test bars only use channel values that are multiples of 4; normal colours used low values with low bits set (7, 3, 2 ...). Every pad channel value is now rounded to a multiple of 4 (lit channels at least 4).
- Diagnostics **BIT TEST** (E1): top row red 1, 2, 4 ... 128 and bottom row green 1 ... 128, sent raw. Should ramp evenly left to right; anything else shows exactly which bits are misread.

### Remote Script
- Pad presses never reached the script in Live (status bar empty), although its clip states reached the keyboard. Input now uses the standard `_Framework` ButtonElement controls (as Ableton's own controller scripts) instead of hand-made MIDI forwarding. The script logs "first message received from the keyboard" to Log.txt.

## KamotKeys 0.37 (consolidated)

One clean change on top of the unmodified SMK37 OPEN V01, containing everything from 0.30–0.36:

- Performance: Normal, Key Scale, Pad Scale, Drum and Clip modes; chord pads with LOCK; spare MIDI pads; ARP and Note Repeat; four dedicated faders; keyboard octave from −2.
- Ableton: Start/Stop/Capture/Undo/Redo/Session Record on channel 15; 8 × 2 Clip Mode on channel 16 with the KamotKeys Remote Script (clip states on the pads, actions in Live's status bar).
- Display: main performance screen with animated piano, pad/drum/clip maps, Patch/ARP/Repeat pages, standard Bluetooth menu, diagnostics, Aurora boot.
- Pad LEDs: centralized renderer, continuous resend, selectable LED timing, RGB tests and frame counter.
- Clean-up: removed an unused field; documentation rewritten for the current state; the release diff is now correctly against V01.

Known open issues: pad LEDs not fully steady in normal play; Clip Mode launching not yet confirmed in Live.

## KamotKeys 0.36 (development snapshot)

### Pad LEDs: refreshed continuously
- Hardware test result on 0.35: **BARS RESENT** (the same frame resent continuously) was steady with every timing preset, including SMK37 OPEN's original. So the LED data and timing are fine; what goes wrong is that a frame sent only on change stays garbled after the LCD traffic every key press causes.
- The pad frame is now resent continuously, about 40 times a second, so any disturbance is overwritten within ~25 ms.
- Diagnostics: STATIC BARS now shows what an un-refreshed frame does (for checking the LCD-disturbance theory).

## KamotKeys 0.35 (development snapshot)

### Pad LEDs
- Diagnosis from 0.34 on hardware: pads garble even when only Fader 4 moves, so **every pad frame is received wrongly**, not just frames sent during screen traffic. The likely cause is the LED bit timing of SMK37 OPEN's pad driver (marked not hardware-validated upstream).
- New **E2 RGB TIMING** on the Diagnostics page: 8 presets for the pad LED bit timing (0 = SMK37 OPEN original). Choose the one where **BARS RESENT** shows steady colours; it is saved.
- Removed 0.34's "resend after every screen redraw" (it sent more of the garbled frames).

### Remote Script
- Logs its start-up, every Clip pad it receives and sync requests to Live's Log.txt, to make setup problems visible.

## KamotKeys 0.34 (development snapshot)

### Changed: pad LED output rewritten (pads still flashed on key presses in 0.33)
- Pads are sent a new frame only when a colour actually changes; moving content (boot, light show, Clip Mode blink) is limited to ~30 frames/s. The fade filter is gone.
- Peak pad brightness lowered from 160 to 64 (SMK37 OPEN itself never exceeds 48). Fader 4 still scales it.
- After every completed screen redraw the pad frame is resent once, so any disturbance of the LED chain during LCD traffic is corrected within milliseconds.
- Host test proves that in Normal Mode, playing keys and chords changes no pad colour (KamotKeys sends the pads nothing on a key press).

### Diagnostics (hold BT 1.5 s)
- **E1 RGB TEST:** STATIC BARS, BARS RESENT (~30/s), ALL OFF.
- **RGB FRAMES SENT** counter. If pads flash while this number does not change, the flashing does not come from KamotKeys' pad data.

## KamotKeys 0.33 (development snapshot)

### Fixed
- **Pad colours garbled in every mode (0.31/0.32).** The pads and the LCD share one SPI data line. 0.31 refreshed the pads in the gap right after each LCD band, and the DMA "done" flag can come before the last bits have left the shift register, so LCD bits spilled onto the LED chain whenever the screen redrew. Pads are no longer refreshed between LCD bands (as in 0.30), and the pad driver waits 20 us for the line to drain before and after every pad frame.

### Added
- Diagnostics page (hold BT 1.5 s), **E1 RGB TEST**: STATIC BARS (fixed colours), BARS RESENT (same colours resent every 20 ms), NO FADE (normal colours without the fade filter). Use it to tell a data/timing problem from a colour-logic problem. Resets to OFF at power-on.

## KamotKeys 0.32 (development snapshot)

### Fixed
- **Pads flashed randomly / SEQ REC strobed all pads with no visible pattern** (reported on 0.31 hardware). The pad LEDs (WS2812-type) need ~280 us of idle line to latch a frame, but the driver's preamble gives ~45 us; 0.31 sent frames back to back (animations, fades, refresh between LCD bands), so the LEDs merged frames into garbage. The pad driver now keeps at least 8 ms between frames (max 125 fps).
- Light-show speed range lowered to about 0.03-2 Hz (Fader 1 at 0 % = 1 Hz); 4 Hz read as strobing on the 2 x 8 grid.

## KamotKeys 0.31 (development snapshot)

### Fixed
- **SEQ REC light shows ran 1000× too slowly** (phase-step unit error), so they looked frozen. They now run at the speed Fader 1 sets.
- Pads could freeze while the LCD was redrawing (they share SPI1); the pads are now refreshed between every LCD band.
- Spare pads lit up "randomly" whenever a chord from the keys reached notes 84–90; spare pads now light only for their own presses.
- The big note/chord name was heavy and oversized; it now uses a lighter stroke at about 60 % of the size.

### Changed
- Keyboard octave starts at **−2**. Pad Scale and Drum pads keep their own octave starting at 0 (Drum pad 1 = 36); OCT buttons move the pad octave in those modes.
- **Pad 13 = chord LOCK** (replaces the SCALE modifier): tap to latch the held chord modifiers; while locked, tapping a modifier pad toggles it; tap LOCK again to release.
- **KNOB BANK** is momentary Session Record (CC 25: 127 press / 0 release).
- **CHORD** is no longer disabled: it toggles Clip Mode.

### Added
- **Clip Mode** (CHORD): 8 tracks × 2 clips launcher on channel 16 with Ableton-state LEDs (empty, stopped, playing, queued, recording).
- **Ableton Remote Script** (`remote_script/KamotKeys`): clip launching, recording into empty armed slots, live LED feedback, Session Record, Capture MIDI, Play/Stop/Undo/Redo without manual mapping, resync on reconnect.
- **Move-style pad feedback**: in Pad Scale/Drum Mode, pads light for every sounding note, including ARP steps and notes played back from Ableton; pads fade out smoothly.
- Smoother light shows: 16-bit phase, interpolated colour, gliding comets and fading trails.
- LIVE indicator when the Remote Script is connected; LOCK shown in the status line.

## KamotKeys 0.30 (development snapshot, not hardware-verified)

First KamotKeys build on **SMK37 OPEN V01** (update identity unchanged: `V01`, `SMK-37 Pro_8000001` / `SMK-37 Elite_8000001`).

### Added
- Central owner-counted MIDI note engine: one Note On per sounding note, Note Off only when the last owner releases, stored notes per key/pad so octave, scale and mode changes never leave stuck notes.
- MIDI OUT port policy AUTO (USB when connected, else BLE), ALL, USB, BLE. Note Offs go to every port.
- Shared scale engine with 23 scales; Fader 2 scale and Fader 3 root with hysteresis.
- Key Scale Mode (SCALE): anchored on the 2nd physical C, ascending and descending, black keys silent.
- Pad Scale Mode (SEQ), Drum Mode (FX, channel 10, vertical map, octave transpose, GM-family colours).
- Chord pads (MAJ, MIN, DIM, SUS4, 7, MAJ7, 6, 9, SCALE) with diatonic chords and live re-voicing; spare MIDI pads 84–90.
- Arpeggiator for keys, chords, scale pads and drum pads: mode, direction, division, gate, swing, octaves. Note Repeat.
- Fader 1 velocity override (> 10 %), Fader 4 global pad brightness with LCD half-dimming at ≤ 10 %.
- Ableton controls on channel 15: Start 20, Stop 21, Capture 22, Undo 24, Record/Overdub 25 (toggle), Redo 27.
- New LCD UI: main performance screen, auto-centring piano with a 110 ms ease-out scroll, chord naming, pad/drum maps, ARP, Note Repeat, Patch and diagnostics pages, banded nonblocking rendering.
- Aurora boot (LCD and pads, gated by Fader 4); eight procedural light shows on SEQ REC with Fader 1 speed.
- Dim/bright button-LED levels; disabled buttons dark.
- Saved settings: BPM, ARP parameters, Note Repeat division, patch, MIDI OUT, internal synth on/off. Octave always starts at 0.
- Host test suite (unit, behaviour, 20 000-event fuzz, LCD screenshots) and a WASI integration test against the SMK37 OPEN engine.

### Kept from SMK37 OPEN
- SDK, engine, hardware drivers, USB/BLE MIDI, Bluetooth audio, flash layout, project/sample/DX7 storage, workbench transfers, the update path and factory restore. The BT button opens SMK37 OPEN's standard Bluetooth menu.

### Removed
- SMK37 OPEN's sequencer/editor UI (tracks, steps, FM/tone editors, mixer, effects, project/preset browser) and its panel control layer. The engine library itself is binary and unchanged.

### Known limitations
- Not compiled with the pi32v2 toolchain and not run on hardware (see docs/TESTING.md).
- The browser workbench's panel preview is not a KamotKeys emulator (shims only).
- No channel aftertouch from pad pressure.

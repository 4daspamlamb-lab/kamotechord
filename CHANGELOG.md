# Changelog

## KamoteKeys 0.65 (development snapshot)

Clip Mode (CHORD), SMK-37:
- Lowest black keys: **C# COPY, D# PASTE, F# select ↔ play, G# nothing, A# DELETE**; scenes moved to the top: **F# launch, G# up, A# down**. Overdub-by-key is gone on the SMK-37 (select + KNOB BANK records).
- **A plain pad press selects** (slot + arm) by default; F# toggles pads to play, saved across power-off.
- **Copy / paste clips** (Live's Duplicate into another slot).

Copy / paste everywhere (Clip and Step Mode): tap or hold COPY, then press what to copy (replaces the clipboard); tap PASTE = the next target, hold PASTE = several. Step Mode targets: a step pad (all its notes), a key (that sound on the page) or a bar in the BARS view. **The screen shows the clipboard.**

## KamoteKeys 0.64 (development snapshot)

Step Mode (SEQ REC), SMK-37 lowest octave rearranged: white C = CLEAR, D = chord toggle, F = LENGTH, G = DRUM, A = RESOLUTION, B = FOLLOW; black C# COPY, D# PASTE, F# BARS, G# / A# = page ◀ / ▶. DOUBLE removed from the keyboard.
- **CLEAR + step** clears every note in that step.
- **Chord toggle** (D): a locked chord's keys pick whole chords or single notes (screen: CHORD / SINGLE).
- **Leaving Step Mode plays the edited clip**; hold PATCH while leaving to leave quietly.
- New full diagrams: docs/diagrams/SMK37-Controls.png, SMK37-Clip-Mode.png, SMK37-Step-Mode.png.

## KamoteKeys 0.63 (development snapshot)

Step Mode (SEQ REC), SMK-37:
- **Chords into the sequencer:** lock a chord on the main screen; in Step Mode with scale keys (SEQ, toggles without leaving Step Mode) or on a track without a Drum Rack, a key picks that chord and a pad writes the whole chord.
- **Note length:** hold the top C, tap pad 1–16 = 1/16 … 1 bar for new steps and chords.
- **Velocity on top F G A B: 32 / 64 / 96 / 127.**
- **Key zone runs C2–E3** (17 keys); the "plays notes" keys are gone.
- **BARS (lowest F#) is a toggle:** tap for the bar view, pick a bar to return to the pattern.

## KamoteKeys 0.62 (development snapshot)

Step Mode (SEQ REC), SMK-37, closer to Ableton Move:
- **OCT −/+ moves the drum keys** by an octave and **SEQ turns them into scale keys** (melodic step sequencing: a key plays its scale note and selects it). Both are remembered **per track** (saved in the Live set); tracks without a Drum Rack start with scale keys.
- **LENGTH + pad n = n bars**; growing a clip repeats its pattern into the new (empty) bars. Pads show the bars while LENGTH is held.
- **BARS** (lowest F#, hold): pads show the clip's bars, tap one to show it.
- **Velocity = the last five white keys** (40 / 60 / 80 / 100 / 120); the **top three black keys = 1× / 2× / 3× repeats** (hold + a pad).
- **Other sounds on a step** show as a violet tint on the selected sound's empty steps, stronger with more of them.
- **New colours**, one hue per meaning; the beat marker is gone (it made pad 5 look lit) and steps past the clip end are off.
- The screen shows the **bar the clip is playing** (BAR 2/4) or its length when stopped, plus the selected note.
- Key-zone pitches reach the script on channel 13.

Everywhere:
- **The chord LOCK survives every mode**; the keys keep playing the locked chord in Drum, Pad Scale, Clip and Step Mode. Only the LOCK pad releases it.

## KamoteKeys 0.61 (development snapshot)

- **Step Mode COPY / PASTE / CLEAR on the lowest octave's black keys (SMK-37):** C# = COPY, D# = PASTE, A# = CLEAR. COPY tap copies the shown page, PASTE tap replaces the shown page with it; with a drum key held they copy / paste just that sound (paste onto another sound works too). CLEAR as before.
- **Fix: a pad after a drum key did nothing in Live.** The drum key now reaches the Remote Script as a channel 14 note (80–95), the same path as the step pads, instead of a CC (the kick's CC value was 0). The script logs `KamoteKeys step: drum key ...` to Log.txt.
- Needs the updated Remote Script.

## KamoteKeys 0.60 (development snapshot)

- **Step Mode laid out like Ableton Move (SMK-37):** the second octave is 12 **drum keys** (C = kick, the first 12 sounds of the Drum Rack bank): a key plays its sound and selects it for the steps. COPY and CLEAR move to C# / D# of the lowest octave, so that octave holds all the controls.
- **CLEAR + drum key** deletes that sound from the whole clip; a CLEAR tap still clears the page (now on release).
- The sequenced track is **armed automatically** while Step Mode is on (and disarmed after), so the drum keys are heard.
- Needs the updated Remote Script (protocol: ch 14 CC 103 bank, CC 104 drum key).

## KamoteKeys 0.59 (development snapshot)

- Maintenance: `kk_controls.c` and `screen.c` split into feature parts (`core/kamotkeys/kk_ctl_*.inc`, `kk_scr_*.inc`), which compile to exactly the same firmware; host tests split by feature; `tools/release.sh` makes the release zip and `BUILD.txt`; `CLAUDE.md` file map; docs keep each fact in one place.

- **SEQ REC = Step Mode** on/off (holding CHORD still works too).
- **Light show moved to KNOB BANK + FADER BANK held together**: each new press = next pattern, Fader 1 = speed. **Hold the pair 3 s = ripple lights on/off** (replaces the SEQ REC triple tap).
- KNOB BANK (Session Record) and FADER BANK (Capture MIDI) now send on release, so pressing them together never records.
- **Chord LOCK survives Key Scale:** lock a chord, switch SEQ (Key Scale) on and off and the lock stays, so scale keys play the locked chord. FM-1: also Pad Scale on keys. Other modes still release the lock.

## KamoteKeys 0.58 (development snapshot)

- **Step Mode: drum step sequencer** (hold CHORD; FM-1: hold LFO). Writes straight into the selected session clip with the Live 12 note API (creates a named 1-bar clip in an empty slot), follows mouse edits, multi-page clips of any length, LENGTH (hold + step), DOUBLE, resolution 1/16, 1/8, 1/32, 1/16T, 1/8T, optional page follow, DRUM (hold) to pick the Drum Rack sound (loaded / empty / active colours), velocity per step as brightness, ties, copy page, clear page. New channel 14 protocol; new `StepSequencer.py` in the Remote Script folder.
- CHORD now acts on release (tap = Clip Mode, hold = Step Mode).

## KamoteKeys 0.57 (development snapshot)

- **Highlighted clip in violet:** the pad of Live's selected (highlighted) slot turns violet; if it is playing, queued or recording it pulses between its state colour and violet. FM-1: the key blinks slowly. Needs the updated Remote Script (CC 105).
- **Ripple lights (SMK-37):** three quick taps on SEQ REC toggle them. Pads and keys keep working; every press sends a coloured ring across the pads from where you pressed (keys map left to right). Holding SEQ REC still runs the light shows.
- **Pad sensitivity 1-7, much stronger:** new velocity curves; 3 is unchanged (linear), 5 is far more sensitive than before, 7 makes a light tap a strong note (Patch page, E4).
- **Understated boot:** a thin teal-to-violet line drawn out on a dark screen, the name fading in, one dim wave across the pads.

## KamoteKeys 0.56 (development snapshot)

- **Launch selected scene moves to the fourth black key** (SMK-37 G#, FM-1 C#); scene up / down stay on the last two black keys; the top F# (FM-1 top C#) plays its note again.
- Remote Script: SELECT now arms the selected track (Live returns a new object for a track on every access, so the old identity test never matched).

## KamoteKeys 0.55 (development snapshot)

- **Clip Mode SELECT:** hold the third black key (SMK-37 F#, FM-1 A#) and press a clip: select that slot and arm its track (others disarmed); the next Session Record (KNOB BANK / FM-1 REC) records there.
- **Scene keys:** the last three black keys (SMK-37 top F# G# A#, FM-1 top C# D# F#) launch the selected scene, scene up, scene down. The grid scrolls with the selected scene and the pads are labelled track.scene.
- Update the Remote Script folder.

## KamoteKeys 0.54 (development snapshot)

- **Sustain holds until the next key:** released notes keep sounding until the next key or pad is pressed; keys pressed together (or added while others are held) are held together. No CC 64 from the button (a pedal still sends it). With ARP on the same rule latches the arpeggio.
- **ARP follows Ableton's tempo** (Ableton has priority): the Remote Script sends Live's tempo (CC 102/103 on ch 16); the BPM turns green and the tempo knob is ignored while Live is connected.
- **Overdub off with a plain press:** pressing a clip that is being overdubbed turns overdub off (no need to hold the OVERDUB key).
- **PATCH + PARA = metronome** (PATCH is SMK37 OPEN's HOME); ◀ + PARA still works. PATCH now opens its page on release.
- Update the Remote Script folder for the tempo and overdub changes.

## KamoteKeys 0.53 (development snapshot)

- **Sustain holds the ARP** (both devices): with ARP on, the Sustain button (SMK-37 NOTE REPEAT, FM-1 SEL) or a sustain pedal keeps the arpeggio playing after the keys are released, and notes played meanwhile join the pattern; Sustain off ends them. No CC 64 is sent while ARP is on.

## KamoteKeys 0.52 (development snapshot)

- **FM-1 Pad Scale keeps the chord keys:** the lowest nine keys stay MAJ…LOCK, and the 18 keys above (white and black) play consecutive scale notes from the root, with chords. The screen shows the piano and chord names. SMK-37 unchanged.

## KamoteKeys 0.51 (development snapshot)

- **PARA = Ableton Play/Pause** (CC 23: stops, or continues from where it stopped). FM-1: tap SAVE (same position).
- **HOME + PARA = metronome on/off** (CC 26). SMK-37: ◀ (HOME) + PARA; ◀ alone still Undo, now sent on release. FM-1: HOME + SAVE.
- Remote Script handles both (update its folder).

## KamoteKeys 0.50 (development snapshot)

### Both devices
- **Clip Mode overdub and delete:** hold the first black key (SMK-37 C#, FM-1 F#) and press a clip to overdub into it (an empty slot: arm and record); hold the second black key (D#, G#) and press a clip to delete it. The screen shows OVERDUB / DELETE. New channel 16 notes 52-67 / 68-83; the Remote Script handles them (update its folder).

### FM-1
- **Compact chord keys:** MAJ MIN DIM SUS4 7 MAJ7 6 9 LOCK on the lowest nine keys, white and black (F..C#); the playing zone grows to 18 keys (D..G). The chord-zone CC keys are gone.
- **Buttons match the SMK-37 by position:** FX = ARP, SEL = Sustain, ENV = Pad Scale, LFO = Clip, EDIT = Stop, GLO = Bluetooth (hold: diagnostics), HOME = settings / back, ARP = Drum, SEQ = Key Scale, PLAY = Start, REC = Session Record (record / overdub).
- **HOME combinations:** HOME + REC = Capture MIDI, HOME + OCT- = Undo, HOME + OCT+ = Redo.
- **SAVE held:** K1 velocity, K2 scale, K3 root (the SMK-37 faders). Without SAVE the main-screen knobs work as on the SMK-37 (tempo, patch, ARP division, gate, direction, swing).

## KamoteKeys 0.49 (development snapshot)

- **One codebase for the SMK-37 Pro, SMK-37 Elite and FM-1.** Shared KamoteKeys code in `core/`; SMK37 OPEN in `devices/smk37`, FM1 OPEN in `devices/fm1`, each with its device glue and a `kk_device.h` describing the hardware. `build.bat smk37-pro | smk37-elite | fm1 [--package]` copies the core into the device tree and runs its own SDK build. `tools/test-all.sh` runs every test for every device.
- FM-1 edition included (previously branch `kamotekeys-fm1`): split keyboard with chord keys, white keys as pads in Pad Scale/Drum/Clip, FM-1 button and knob mapping. See docs/FM1.md.
- No functional change on the SMK-37 (same tests pass); saved settings are kept.

## KamoteKeys 0.48 (development snapshot)

- Spare (CC) pads 5-8 and 14-16 rest at **half** the chord pads' brightness: 12.5 % (chord 25 %).

## KamoteKeys 0.47 (development snapshot)

- Spare (CC) pads 5-8 and 14-16 rest **dimmer** than the chord pads again: 20 % (chord 25 %). 0.45/0.46 had them brighter by mistake.

## KamoteKeys 0.46 (development snapshot)

- Renamed to **KamoteKeys** (was spelled "KamotKeys"): boot screen, version label, docs, package names and the Ableton Remote Script (now `KamoteKeys`; reselect it in Live's Control Surface list). Saved settings are kept.

## KamoteKeys 0.45 (development snapshot)

- **NOTE REPEAT button is now SUSTAIN:** tap to latch CC 64 = 127 (channel 1, internal synth and MIDI out), tap again for 0. LED bright and "SUSTAIN" in the status line while on. Combined with a sustain pedal through one shared state (no duplicate or contradictory CC 64); released on panic/reset. Note Repeat, its page and the E7 Repeat knob are removed.
- **SCALE and SEQ swapped:** SCALE = Pad Scale Mode, SEQ = Key Scale Mode; their LEDs follow.
- Spare (CC) pads 5-8 and 14-16 rest at 30 % (reverted in 0.47: they should be dimmer).

## KamoteKeys 0.44 (development snapshot)

- **Pad sensitivity** (Patch page, E4, saved): 1 = harder ... 3 = linear (as before) ... 5 = light hits give high velocity. A velocity curve on the pads only; keys are unaffected and the pad trigger threshold (factory calibration) is unchanged. Fader 1 above 10 % still overrides velocity.

## KamoteKeys 0.43 (development snapshot)

- **Spare pads 5-8 and 14-16 are mappable buttons:** CC 102-108 on channel 15 (127 press, 0 release) instead of notes 84-90. They light while held, never use ARP or Note Repeat, and are released cleanly on mode changes.
- Resting pads slightly dimmer than 0.42: chord 25 %, spare 18 %, scale/drum 22 %, Pad Scale roots 50 %.

## KamoteKeys 0.42 (development snapshot)

### Look and feel
- Resting pads much brighter: chord 35 %, spare 25 %, scale/drum 30 % (were 12 / 4 / 10 %); Clip Mode states brighter too.
- Chord pad colours by meaning: MAJ warm amber, MIN blue, DIM purple, SUS4 teal; extensions 7, MAJ7, 6, 9 as one magenta-to-pink family; LOCK white.
- Pad Scale roots rest at 65 % in a whiter cyan, clearly different from the other notes at rest.
- SEQ REC light show crossfades in (~200 ms) and out (~150 ms) instead of jumping in one frame (seen as a flash).

### LOCK
- While locked, tapping a pad switches within its row (pads 1-4: MAJ/MIN/DIM/SUS4; pads 9-12: 7/MAJ7/6/9). Pads combine only when pressed while another pad of the same row is held. Tapping the active pad clears its row.

## KamoteKeys 0.41 (development snapshot)

### Pad LEDs: root cause
- Hardware report (0.40): colours correct, but pressing chord pads made other pads flicker, pads 1-4 and 9-12 were miscoloured and pad 1 blinked green, while spare pads were fine. Those pads sit at the start and end of the LED chain.
- Cause: the reset gap before each pad frame was ~45 us (SMK37 OPEN's 36 zero bytes); these LEDs need ~280 us. A stray pulse when SPI1 is switched from the LCD to the pads therefore became an extra leading bit, shifting the frame (first LED's green MSB = pad 1 green; low bits spilling into the next channel's top bit = "crazy" colours, hidden by the multiple-of-4 rounding).
- Fix: clock a zero byte with the LCD deselected before switching SPI1 to the pads, and lead every frame with 320 zero bytes (~400 us). Multiple-of-4 rounding stays as a safety margin.
- **Verified on hardware:** pad behaviour correct on an SMK-37 Pro.

## KamoteKeys 0.40 (clean rebuild)

One change on top of the unmodified SMK37 OPEN V01 again, with the pad LED settings the hardware has proven:

- **Pad values rounded to multiples of 4** (restored). Hardware comparison: 0.37 (no rounding) flashed, 0.38 (rounding) was calm, 0.39 (no rounding) flashed on key presses again. BIT TEST could not show this because the affected values (bits 0-1) are practically invisible.
- **Peak brightness 160** (kept from 0.39), so dim idle colours keep their hue.
- Everything else as 0.39.

## KamoteKeys 0.39 (development snapshot)

### Pad LEDs
- Hardware **BIT TEST** (0.38): every value 1-128 displayed correctly on every pad, so the LED data path is fine. 0.38's multiple-of-4 rounding is removed.
- Peak brightness back to 160. At 64 (0.34-0.38) the dim idle colours had so few steps that they lost their hue (the blue MIN pad became (4,4,8), nearly grey; orange became yellow), which looked like wrong colours.

## KamoteKeys 0.38 (development snapshot)

### Pad LEDs
- Hardware videos (0.37): the Diagnostics test bars were steady and correct, but normal colours showed wrong, bright colours. The test bars only use channel values that are multiples of 4; normal colours used low values with low bits set (7, 3, 2 ...). Every pad channel value is now rounded to a multiple of 4 (lit channels at least 4).
- Diagnostics **BIT TEST** (E1): top row red 1, 2, 4 ... 128 and bottom row green 1 ... 128, sent raw. Should ramp evenly left to right; anything else shows exactly which bits are misread.

### Remote Script
- Pad presses never reached the script in Live (status bar empty), although its clip states reached the keyboard. Input now uses the standard `_Framework` ButtonElement controls (as Ableton's own controller scripts) instead of hand-made MIDI forwarding. The script logs "first message received from the keyboard" to Log.txt.

## KamoteKeys 0.37 (consolidated)

One clean change on top of the unmodified SMK37 OPEN V01, containing everything from 0.30–0.36:

- Performance: Normal, Key Scale, Pad Scale, Drum and Clip modes; chord pads with LOCK; spare MIDI pads; ARP and Note Repeat; four dedicated faders; keyboard octave from −2.
- Ableton: Start/Stop/Capture/Undo/Redo/Session Record on channel 15; 8 × 2 Clip Mode on channel 16 with the KamoteKeys Remote Script (clip states on the pads, actions in Live's status bar).
- Display: main performance screen with animated piano, pad/drum/clip maps, Patch/ARP/Repeat pages, standard Bluetooth menu, diagnostics, Aurora boot.
- Pad LEDs: centralized renderer, continuous resend, selectable LED timing, RGB tests and frame counter.
- Clean-up: removed an unused field; documentation rewritten for the current state; the release diff is now correctly against V01.

Known open issues: pad LEDs not fully steady in normal play; Clip Mode launching not yet confirmed in Live.

## KamoteKeys 0.36 (development snapshot)

### Pad LEDs: refreshed continuously
- Hardware test result on 0.35: **BARS RESENT** (the same frame resent continuously) was steady with every timing preset, including SMK37 OPEN's original. So the LED data and timing are fine; what goes wrong is that a frame sent only on change stays garbled after the LCD traffic every key press causes.
- The pad frame is now resent continuously, about 40 times a second, so any disturbance is overwritten within ~25 ms.
- Diagnostics: STATIC BARS now shows what an un-refreshed frame does (for checking the LCD-disturbance theory).

## KamoteKeys 0.35 (development snapshot)

### Pad LEDs
- Diagnosis from 0.34 on hardware: pads garble even when only Fader 4 moves, so **every pad frame is received wrongly**, not just frames sent during screen traffic. The likely cause is the LED bit timing of SMK37 OPEN's pad driver (marked not hardware-validated upstream).
- New **E2 RGB TIMING** on the Diagnostics page: 8 presets for the pad LED bit timing (0 = SMK37 OPEN original). Choose the one where **BARS RESENT** shows steady colours; it is saved.
- Removed 0.34's "resend after every screen redraw" (it sent more of the garbled frames).

### Remote Script
- Logs its start-up, every Clip pad it receives and sync requests to Live's Log.txt, to make setup problems visible.

## KamoteKeys 0.34 (development snapshot)

### Changed: pad LED output rewritten (pads still flashed on key presses in 0.33)
- Pads are sent a new frame only when a colour actually changes; moving content (boot, light show, Clip Mode blink) is limited to ~30 frames/s. The fade filter is gone.
- Peak pad brightness lowered from 160 to 64 (SMK37 OPEN itself never exceeds 48). Fader 4 still scales it.
- After every completed screen redraw the pad frame is resent once, so any disturbance of the LED chain during LCD traffic is corrected within milliseconds.
- Host test proves that in Normal Mode, playing keys and chords changes no pad colour (KamoteKeys sends the pads nothing on a key press).

### Diagnostics (hold BT 1.5 s)
- **E1 RGB TEST:** STATIC BARS, BARS RESENT (~30/s), ALL OFF.
- **RGB FRAMES SENT** counter. If pads flash while this number does not change, the flashing does not come from KamoteKeys' pad data.

## KamoteKeys 0.33 (development snapshot)

### Fixed
- **Pad colours garbled in every mode (0.31/0.32).** The pads and the LCD share one SPI data line. 0.31 refreshed the pads in the gap right after each LCD band, and the DMA "done" flag can come before the last bits have left the shift register, so LCD bits spilled onto the LED chain whenever the screen redrew. Pads are no longer refreshed between LCD bands (as in 0.30), and the pad driver waits 20 us for the line to drain before and after every pad frame.

### Added
- Diagnostics page (hold BT 1.5 s), **E1 RGB TEST**: STATIC BARS (fixed colours), BARS RESENT (same colours resent every 20 ms), NO FADE (normal colours without the fade filter). Use it to tell a data/timing problem from a colour-logic problem. Resets to OFF at power-on.

## KamoteKeys 0.32 (development snapshot)

### Fixed
- **Pads flashed randomly / SEQ REC strobed all pads with no visible pattern** (reported on 0.31 hardware). The pad LEDs (WS2812-type) need ~280 us of idle line to latch a frame, but the driver's preamble gives ~45 us; 0.31 sent frames back to back (animations, fades, refresh between LCD bands), so the LEDs merged frames into garbage. The pad driver now keeps at least 8 ms between frames (max 125 fps).
- Light-show speed range lowered to about 0.03-2 Hz (Fader 1 at 0 % = 1 Hz); 4 Hz read as strobing on the 2 x 8 grid.

## KamoteKeys 0.31 (development snapshot)

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
- **Ableton Remote Script** (`remote_script/KamoteKeys`): clip launching, recording into empty armed slots, live LED feedback, Session Record, Capture MIDI, Play/Stop/Undo/Redo without manual mapping, resync on reconnect.
- **Move-style pad feedback**: in Pad Scale/Drum Mode, pads light for every sounding note, including ARP steps and notes played back from Ableton; pads fade out smoothly.
- Smoother light shows: 16-bit phase, interpolated colour, gliding comets and fading trails.
- LIVE indicator when the Remote Script is connected; LOCK shown in the status line.

## KamoteKeys 0.30 (development snapshot, not hardware-verified)

First KamoteKeys build on **SMK37 OPEN V01** (update identity unchanged: `V01`, `SMK-37 Pro_8000001` / `SMK-37 Elite_8000001`).

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
- The browser workbench's panel preview is not a KamoteKeys emulator (shims only).
- No channel aftertouch from pad pressure.

# KamoteKeys on the M-VAVE FM-1

KamoteKeys runs on the M-VAVE **FM-1** from the same code as the SMK-37 edition (`core/`), built on **FM1 OPEN V14** (`devices/fm1`, original readme in [devices/fm1/README.FM1-OPEN.md](../devices/fm1/README.FM1-OPEN.md)): chord pads, Key Scale, Pad Scale, Drum and Clip modes, ARP, Sustain, Ableton Live buttons, an animated piano display and BLE/USB MIDI. It keeps FM1 OPEN's drivers, synth engine, storage and browser updater.

The FM-1 has no pads and no faders, so **everything the SMK-37 pads did is on the keys**, and the SMK-37 fader settings are set with knobs.

> **Status: untested on hardware.** This build has been checked in the FM1 OPEN browser simulator (39 integration checks with the real engine, MIDI router and LED path) and compiles cleanly, but has never run on a real FM-1. Back up first (the updater offers it) and keep **Restore factory V15** in mind.

## The keyboard (27 keys, F to G)

The white keys are labelled 01–16 on the FM-1.

### Normal and Key Scale Mode: split keyboard

```
 black:   CC102 CC103 CC104   CC105 CC106   CC107
 white:  01   02   03   04   05   06   07   08   09  | 10 ... 16 (+ black keys)
         MAJ  MIN  DIM  SUS4 7    MAJ7 6    9    LOCK|  playing zone: G# to G
         └──────────── chord zone (F..G) ──────────┘ |  (key 11 = C = middle C)
```

- **Chord zone** (white 01–09 and the six black keys between them):
  - Hold **MAJ / MIN / DIM / SUS4** (01–04) and/or **7 / MAJ7 / 6 / 9** (05–08) while playing in the playing zone: every note becomes that chord. Combine a type and an extension (e.g. MIN + 7 = m7).
  - **LOCK** (09): latches the chord keys, so you don't have to hold them. While locked, tapping a key switches within its row; tap LOCK again to release.
  - **Black keys**: mappable momentary buttons, **CC 102–107 on channel 15** (127 press / 0 release), for MIDI-mapping in Live.
- **Playing zone** (the 12 keys from G# to G): plays notes on **channel 1**. In **Key Scale Mode** the white keys play the scale (the C key = scale root) and black keys are silent.
- Every note uses the fixed **velocity** set with the PRESETS knob (FM-1 keys have no velocity).

### Pad Scale, Drum and Clip Mode: white keys = pads 1–16

White keys 01–08 are pads 1–8 and 09–16 are pads 9–16. Black keys are silent in these modes.

- **Pad Scale (SEL):** consecutive scale notes from pad 1 (C3 for root C), channel 1. Root keys stay lit.
- **Drum (FX):** drum notes on **channel 10**: pad 1 = 36, pad 9 = 37, pad 2 = 38, … OCT −/+ transposes by 12.
- **Clip Mode (EDIT):** pads launch clip 1 (pads 1–8) and clip 2 (pads 9–16) of Live tracks 1–8 (notes 36–51 on channel 16). Needs the [KamoteKeys Remote Script](ABLETON_REMOTE_SCRIPT.md). Key LEDs: playing = on, queued = blinking, recording = fast blink; the screen shows every clip's state in colour.

Key LEDs light while a pad or key sounds, including ARP steps and notes played back from Live.

## Buttons

| FM-1 button | KamoteKeys | Light |
|---|---|---|
| OCT − / OCT + | octave of the playing zone (or of the pads in Pad Scale / Drum) | while pressed |
| FX | Drum Mode on/off | in Drum Mode |
| SEL | Pad Scale Mode on/off | in Pad Scale |
| SEQ | Key Scale Mode on/off | in Key Scale |
| EDIT | Clip Mode on/off | in Clip Mode |
| ENV | **Sustain** on/off (CC 64 on channel 1, latching) | while sustain is on |
| ARP | tap: ARP on/off; hold 0.6 s: ARP settings | while ARP is on |
| LFO | tap: **settings** (patch, synth, MIDI out, velocity); hold 0.6 s: **Bluetooth menu**; hold 2 s: diagnostics | while a page is open |
| PLAY | Ableton **Start** (CC 20, channel 15) | while pressed |
| GLO | Ableton **Stop** (CC 21) | while pressed |
| REC | Ableton **Session Record** (CC 25, momentary) | while pressed |
| SAVE | Ableton **Capture MIDI** (CC 22) | while pressed |
| HOME | Ableton **Undo** (CC 24, on release); leaves any page | while pressed |
| HOME + SAVE | Ableton **Redo** (CC 27); no Undo is sent | |

In the Bluetooth menu, HOME and SAVE are its back/confirm keys (as in FM1 OPEN).

## Knobs

| Knob | Main screen | ARP page | Settings (LFO) | Bluetooth menu |
|---|---|---|---|---|
| K1 | Tempo (30–300 BPM) | Mode | Patch (DX7 1–128) | Bluetooth on/off |
| K2 | Patch | Direction | Internal synth on/off | BT audio on/off |
| K3 | ARP division | Division | MIDI OUT ports (AUTO/ALL/USB/BLE) | BLE role |
| K4 | ARP gate | Gate | Velocity | Clock INT/MIDI |
| SELECT | **Scale** (23 scales) | Swing | | device list |
| ALGORITHM | **Root** (C–B) | Octaves | | |
| PRESETS | **Velocity** (1–127, default 100) | | | action |
| MASTER | master volume (always) | | | |

Saved automatically (about 1.5 s after a change): tempo, ARP settings, scale, root, velocity, patch, MIDI OUT ports, internal synth. The octave starts at 0 on every power-on.

## MIDI

- **USB and Bluetooth LE MIDI**, used interchangeably: MIDI OUT **AUTO** sends to USB when it is connected, otherwise to BLE, and Note Offs always go everywhere so nothing sticks.
- Channel 1 keys/chords/Pad Scale, channel 10 Drum, channel 15 Ableton CCs (20 Start, 21 Stop, 22 Capture, 24 Undo, 25 Record, 27 Redo, 102–107 chord-zone black keys), channel 16 Clip Mode, CC 64 channel 1 Sustain.
- The internal synth (DX7 on track 1, drums on track 8) plays everything unless switched off in settings.

## Ableton Live

Install `remote_script/KamoteKeys` as described in [ABLETON_REMOTE_SCRIPT.md](ABLETON_REMOTE_SCRIPT.md). On the FM-1, choose the **FM-1's own USB (or Bluetooth) MIDI port** as the Control Surface Input and Output; it is not called "SMK37 Open". Turn on Track and Remote for that port.

## Build and flash

From the repository root (same toolchain as the SMK-37: JieLi 2.5.3, `C:\JL\pi32`):

```powershell
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL = 'https://localhost/kamotekeys-0.49-source.zip'
.\build.bat fm1 --package
```

Output: `devices\fm1\build\official-web\FM1-Official-Open-Source.html`. Open it in desktop Chrome, connect the FM-1 by USB, allow MIDI/SysEx, tick the backup box and use **Device update**. To go back: **Restore factory V15** in the same page, or the official FM1 OPEN workbench. Never flash an FM-1 package onto an SMK-37 or the reverse.

## FM-1 specific code

- `devices/fm1/app/kamotkeys/kk_device.h`: 27 keys, split point, key anchor, fixed velocity, knob labels.
- `devices/fm1/app/kamotkeys/kk_fm1_input.c`: keys/buttons/knobs to KamoteKeys events, split keyboard, white keys as pads, key and button LEDs.
- `devices/fm1/app/midi_router.c`, `project.c`: FM1 OPEN with the KamoteKeys MIDI output hook and settings record.
- Tests: `tests/integration/fm1_sim_test.mjs` (browser simulator with the real FM-1 engine).

## Licence

See the main [README](../README.md#license-and-attribution).


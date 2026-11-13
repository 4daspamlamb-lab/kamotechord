# KamoteKeys on the M-VAVE FM-1

KamoteKeys runs on the M-VAVE **FM-1** from the same code as the SMK-37 edition (`core/`), built on **FM1 OPEN V14** (`devices/fm1`, original readme in [devices/fm1/README.FM1-OPEN.md](../devices/fm1/README.FM1-OPEN.md)): chord pads, Key Scale, Pad Scale, Drum and Clip modes, ARP, Sustain, Ableton Live buttons, an animated piano display and BLE/USB MIDI. It keeps FM1 OPEN's drivers, synth engine, storage and browser updater.

The FM-1 has no pads and no faders, so **everything the SMK-37 pads did is on the keys**, and the SMK-37 fader settings are set with knobs.

> **Status:** flashed and running on an FM-1 (0.49). 0.50–0.52 change the key and button layout as described below; they have been checked in the FM1 OPEN browser simulator (52 integration checks with the real engine, MIDI router and LED path). Back up first (the updater offers it).

## The keyboard (27 keys, F to G)

### Normal, Key Scale and Pad Scale Mode: compact chord keys

```
 keys:  F    F#   G    G#   A    A#   B    C    C#  | D ........................ G
        MAJ  MIN  DIM  SUS4 7    MAJ7 6    9    LOCK|  playing zone (18 keys, middle C = the C key)
        └───────── chord keys (9 keys, white and black) ┘
```

- **Chord keys** (the lowest nine keys, white and black):
  - Hold **MAJ / MIN / DIM / SUS4** and/or **7 / MAJ7 / 6 / 9** while playing in the playing zone: every note becomes that chord. Combine a type and an extension (e.g. MIN + 7 = m7).
  - **LOCK** (C#): latches the chord keys, so you don't have to hold them. While locked, tapping a key switches within its group; tap LOCK again to release.
- **Playing zone** (the 18 keys from D to G, an octave and a half): plays notes on **channel 1**.
  - **Normal Mode:** every key plays its own note.
  - **Key Scale Mode (SEQ):** the white keys play the scale (the C key = scale root); black keys are silent.
  - **Pad Scale Mode (ENV):** **every key, white and black, plays the next scale note**, like the SMK-37's scale pads: the lowest playing key (D) is the root (C3 for root C), then up the scale for 18 notes. Root keys stay lit. OCT −/+ moves it by an octave.
  - The chord keys work in all three.
- Every note uses the fixed **velocity** (SAVE + K1; FM-1 keys have no velocity).

### Drum and Clip Mode: white keys = pads 1–16

White keys 01–08 are pads 1–8 and 09–16 are pads 9–16.

- **Drum (ARP button):** drum notes on **channel 10**: pad 1 = 36, pad 9 = 37, pad 2 = 38, … OCT −/+ transposes by 12. Black keys silent.
- **Clip Mode (LFO):** pads launch clip 1 (pads 1–8) and clip 2 (pads 9–16) of Live tracks 1–8. Needs the [KamoteKeys Remote Script](ABLETON_REMOTE_SCRIPT.md).
  - Hold the **first black key (F#)** and press a clip: **overdub** into it (an empty slot: arm and record a new clip). The same again stops overdubbing.
  - Hold the **second black key (G#)** and press a clip: **delete** it.
  - The screen shows OVERDUB / DELETE while the key is held. Key LEDs: playing = on, queued = blinking, recording = fast blink.

Key LEDs light while a pad or key sounds, including ARP steps and notes played back from Live.

## Buttons: the same as the SMK-37, by position

The FM-1's right-hand block of 2 × 6 buttons matches the SMK-37's, so each button does what the SMK-37 button in the same place does:

```
 FM-1:     FX      SEL       ENV        LFO    EDIT    GLO
 SMK-37:   ARP     SUSTAIN   SCALE      CHORD  GLOBE   BT
           ARP     Sustain   Pad Scale  Clip   Stop    Bluetooth

 FM-1:     HOME    SAVE      ARP        SEQ        PLAY    REC
 SMK-37:   PATCH   PARA      FX         SEQ        PLAY    REC
           Settings Play/Pause Drum     Key Scale  Start   Record / overdub
```

| FM-1 button | KamoteKeys | Light |
|---|---|---|
| OCT − / OCT + | octave of the playing zone (Pad Scale and Drum keep their own octave) | while pressed |
| FX | tap: **ARP** on/off; hold 0.6 s: ARP settings | while ARP is on |
| SEL | **Sustain** on/off: released notes keep sounding until the next key; keys pressed together are held together; with ARP on it latches the arpeggio | while sustain is on |
| ENV | **Pad Scale** Mode on/off | in Pad Scale |
| LFO | **Clip Mode** on/off | in Clip Mode |
| EDIT | Ableton **Stop** (CC 21) | while pressed |
| GLO | **Bluetooth menu**; hold 1.5 s: diagnostics | while a menu is open |
| HOME | tap: **settings** page (patch, synth, MIDI out, velocity); on any other page: back to the main screen | while pressed / on the settings page |
| SAVE | tap: Ableton **Play/Pause** (CC 23; it sits where the SMK-37 has PARA); hold: **K1 velocity, K2 scale, K3 root** (the SMK-37 faders); turning a knob cancels the Play/Pause | while held |
| ARP | **Drum** Mode on/off | in Drum Mode |
| SEQ | **Key Scale** Mode on/off | in Key Scale |
| PLAY | Ableton **Start** (CC 20) | while pressed |
| REC | Ableton **Session Record**: starts recording, or overdubs the playing clips; again to stop (CC 25) | while pressed |
| HOME + REC | Ableton **Capture MIDI** (CC 22) | |
| HOME + OCT − | Ableton **Undo** (CC 24) | |
| HOME + OCT + | Ableton **Redo** (CC 27) | |
| HOME + SAVE | Ableton **metronome** on/off (CC 26) | |

In the Bluetooth menu, HOME and SAVE are its back / confirm keys (as in FM1 OPEN). HOME used in a combination does not open the settings page.

## Knobs

| Knob | Main screen | Main screen, **SAVE held** | ARP page | Settings (HOME) | Bluetooth menu |
|---|---|---|---|---|---|
| K1 | Tempo (30–300 BPM) | **Velocity** (1–127, default 100) | Mode | Patch (DX7 1–128) | Bluetooth on/off |
| K2 | Patch | **Scale** (23 scales) | Direction | Internal synth on/off | BT audio on/off |
| K3 | ARP division | **Root** (C–B) | Division | MIDI OUT ports (AUTO/ALL/USB/BLE) | BLE role |
| K4 | ARP gate | | Gate | Velocity | Clock INT/MIDI |
| SELECT | ARP direction | | Swing | | device list |
| ALGORITHM | ARP swing | | Octaves | | |
| PRESETS | | | | | action |
| MASTER | master volume (always) | | | | |

Saved automatically (about 1.5 s after a change): tempo, ARP settings, scale, root, velocity, patch, MIDI OUT ports, internal synth. The octave starts at 0 on every power-on.

## MIDI

- **USB and Bluetooth LE MIDI**, used interchangeably: MIDI OUT **AUTO** sends to USB when it is connected, otherwise to BLE, and Note Offs always go everywhere so nothing sticks.
- Channel 1 keys/chords/Pad Scale, channel 10 Drum, channel 15 Ableton CCs (20 Start, 21 Stop, 22 Capture, 23 Play/Pause, 24 Undo, 25 Record, 26 Metronome, 27 Redo), channel 16 Clip Mode (36–51 launch, 52–67 overdub, 68–83 delete), CC 64 channel 1 Sustain. (The SMK-37's mappable CC pads 102–108 have no key on the FM-1.)
- The internal synth (DX7 on track 1, drums on track 8) plays everything unless switched off in settings.

## Ableton Live

Install `remote_script/KamoteKeys` as described in [ABLETON_REMOTE_SCRIPT.md](ABLETON_REMOTE_SCRIPT.md). On the FM-1, choose the **FM-1's own USB (or Bluetooth) MIDI port** as the Control Surface Input and Output; it is not called "SMK37 Open". Turn on Track and Remote for that port.

## Build and flash

From the repository root (same toolchain as the SMK-37: JieLi 2.5.3, `C:\JL\pi32`):

```powershell
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
$env:FM1_SOURCE_URL = 'https://localhost/kamotekeys-0.54-source.zip'
.\build.bat fm1 --package
```

Output: `devices\fm1\build\official-web\FM1-Official-Open-Source.html`. Open it in desktop Chrome, connect the FM-1 by USB, allow MIDI/SysEx, tick the backup box and use **Device update**. To go back: **Restore factory V15** in the same page, or the official FM1 OPEN workbench. Never flash an FM-1 package onto an SMK-37 or the reverse.

## FM-1 specific code

- `devices/fm1/app/kamotkeys/kk_device.h`: 27 keys, split point, key anchor, fixed velocity, knob labels.
- `devices/fm1/app/kamotkeys/kk_fm1_input.c`: keys/buttons/knobs to KamoteKeys events, chord keys, white keys as pads, the SMK-37 button positions with the HOME/SAVE combinations, key and button LEDs.
- `devices/fm1/app/midi_router.c`, `project.c`: FM1 OPEN with the KamoteKeys MIDI output hook and settings record.
- Tests: `tests/integration/fm1_sim_test.mjs` (browser simulator with the real FM-1 engine).

## Licence

See the main [README](../README.md#license-and-attribution).


# How to use KamoteKeys (SMK-37 Pro / Elite)

A hands-on guide. The full reference is in [README.md](README.md); the pictures are in `docs/diagrams/`:

| Diagram | Shows |
|---|---|
| `SMK37-Controls.png` | every button, fader and knob |
| `SMK37-Modes-Table.png` | what each control does in each mode |
| `SMK37-Play-Modes.png` | Normal, Key Scale, Pad Scale and Drum Mode |
| `SMK37-Clip-Mode.png` | Clip Mode (CHORD) |
| `SMK37-Step-Mode.png` | Step Mode (SEQ REC) |
| `SMK37-Boot.gif` | the startup animation |

---

## 1. Set up once

### Flash the firmware
1. Build the page on Windows (see `BUILD.txt` in the release zip) or use a page someone built for you.
2. On the Mac, open `SMK37-Pro-KamoteKeys-<version>.html` (or `-Elite-`) in **Chrome or Brave**.
3. Connect the keyboard by **USB**, allow MIDI and SysEx, click **Device update**, keep USB connected until it finishes, then power-cycle.
4. You should see the bubbles startup and **KamoteKeys <version>** on the screen. Only flash the Pro file onto a Pro and the Elite file onto an Elite.

### Install the Ableton Remote Script (Live 12)
1. Copy the folder `Ableton/KamoteKeys` from the zip to
   `~/Music/Ableton/User Library/Remote Scripts/KamoteKeys` (the folder must contain `__init__.py` directly).
2. Restart Live. In **Settings → Link, Tempo & MIDI**, pick **KamoteKeys** as a Control Surface.
3. Set its **Input and Output to the same port**: the keyboard's USB port, or its Bluetooth port when you play wirelessly. For both, add KamoteKeys twice (one row USB, one row Bluetooth).
4. In the MIDI ports list turn on **Track** and **Remote** for that input, **Remote** for that output.
5. Check on the keyboard: hold **BT 1.5 s** (Diagnostics). The green line `LIVE USB` / `LIVE BLE` means the script answers. In Clip Mode the pads show your clips.

Each time you update KamoteKeys, also replace the Remote Script folder and restart Live.

---

## 2. Everyday controls

| Control | Does |
|---|---|
| **PARA** | Start / Stop, like the spacebar |
| **SEQ PLAY** | Finish the clip being recorded and go to the next track (armed) |
| **GLOBE** | Stop |
| **KNOB BANK** | Session Record (record / overdub) |
| **FADER BANK** | Capture MIDI |
| **◀ / ▶** | Undo / Redo (◀ + PARA or PATCH + PARA = metronome) |
| **OCT − / +** | Octave of the keys (or the pads in Pad Scale / Drum) |
| **Fader 1** | Velocity (bottom 10 % = play dynamics, above = fixed) |
| **Fader 2 / 3** | Scale / root key |
| **Fader 4** | **Master volume** (the keyboard and Live's master) |
| **E1–E6** | Tempo, patch, ARP division, gate, direction, swing |
| **E8** | Keyboard master volume |
| **PATCH** | Patch page: patch, internal synth on/off, MIDI out, pad sensitivity (E4), **pad brightness (E5)** |
| **ARP** | Arpeggiator on/off (hold: ARP page) |
| **NOTE REPEAT** | Sustain: notes hold until the next key |

---

## 3. Play

### Chords (Normal Mode)
- Hold a chord pad (MAJ, MIN, DIM, SUS4 on the top row; 7, MAJ7, 6, 9 below) and play a key: you get that chord on the key. Combine a type and an extension (MIN + 7 = m7).
- **LOCK** (pad 13): hold your chord pads and tap LOCK. Now every key plays that chord, in **every mode**, until you tap LOCK again.
- Pads 5–8 and 14–16 are free buttons (CC 102–108) for MIDI-mapping in Live.

### Play in a scale (Key Scale, SEQ)
- Press **SEQ**. White keys now play the scale from Fader 2 with the root from Fader 3; the 2nd C is the root. Black keys are silent, so you can't play a wrong note.
- Chord pads still work and stay in the scale. A locked chord stays on.

### Pads as notes (Pad Scale, SCALE) and drums (Drum, FX)
- **SCALE**: the 16 pads play the scale (root pads cyan).
- **FX**: the pads are GM drums on channel 10 (kick top left, rim below it, snare next …), coloured by drum type. OCT moves them by an octave.

### Lights
- **Light show**: hold **KNOB BANK + FADER BANK** together. Press one of them again for the next pattern; Fader 1 sets the speed.
- **Ripple lights**: start the light show **three times quickly** to turn ripples on (again to turn them off). Every press then sends a coloured ring across the pads.

---

## 4. Launch clips (Clip Mode, CHORD)

Press **CHORD**. The pads are 8 tracks × 2 scenes of Live's Session View, coloured by state (green playing, blue stopped, red recording, violet = Live's selected slot).

| To … | Do |
|---|---|
| select a slot (and arm its track) | press its pad |
| play / stop clips | hold **F#** and press them (or tap F#, then press one clip) |
| stop clips | hold **G#** and press them (tap G#: stop the selected clip) |
| overdub into a clip | hold the **lowest C** and press it (an empty slot: record a new clip) |
| record a new clip | select an empty slot, press **KNOB BANK** |
| copy / paste a clip | tap **C#**, press the clip; tap **D#**, press where it goes (hold D# for several) |
| delete a clip | hold **A#** and press it |
| scroll scenes / launch a scene | top **G# / A#** / top **F#** |
| move the selected clip up / down | hold the **top F** and press top G# / A# |
| see other tracks | top **A / B** |

**Build a song from loops (Scene Arrange):** press the low **D**. The two pad rows are two scenes; tap pads to choose which tracks play in each scene (green on, dim off). Launch a scene with the top F#: exactly those tracks play, each with its one clip, so editing a clip changes every scene. Press D again to leave.

**Record a band one track at a time:** select the first track's slot (pad), press **KNOB BANK** to record, play, then press **SEQ PLAY**: the clip keeps looping and the next track is armed and selected. Press KNOB BANK again to record it, and so on.

---

## 5. Step sequencer (Step Mode, SEQ REC)

Select a track and a slot in Live (or a pad in Clip Mode), then press **SEQ REC**. The track is armed and a 1-bar clip is made if the slot is empty. The 16 pads are the 16 steps of the shown bar.

### Program a beat
1. Press a **drum key** in the 2nd octave (C = kick, D = snare, F# = closed hat …): you hear it and the pads now edit that sound.
2. Tap pads to add / remove steps (amber = note, brighter = louder). Other sounds on a step show as a lilac tint.
3. Pick another drum key and add its steps. **PARA** plays the clip (and stops it).
4. Press **SEQ REC** again to go back to where you came from. An empty clip that Step Mode made is removed.

### Keys of the lowest octave
| Key | Does |
|---|---|
| C# / D# | COPY / PASTE (a step pad, a drum key, or a bar in the bar view) |
| D / E | RESOLUTION (1/16, 1/8, 1/32, triplets) / FOLLOW (pages follow the playhead) |
| F (hold) | DRUM: the pads become Drum Mode pads — tap one to hear and select it |
| G (hold) + pad n | LENGTH: the clip becomes n bars (the pattern is repeated into new bars) |
| F# / G# (or ◀ / ▶) | page left / right |
| A | BARS: the pads show the bars; tap one to show it |
| A# | DELETE: tap = clear the page; hold + step = clear that step; hold + drum key = remove that sound; in the bar view hold + bar = delete the bar (hold the pad 3 s to only clear it) |
| B | DOUBLE: the clip twice as long, everything copied |

### More
- **Velocity**: top F G A B = 32 / 64 / 96 / 127 (hold a step + one of them to change that step).
- **Note length**: hold the top C and tap pad 1–16 (1/16 … 1 bar).
- **Repeats**: hold the top F# / G# / A# and press a pad: that step plays 1 / 2 / 3 times (you hear it).
- **Tie**: hold a step and press a later one.
- **Melodies**: press **SEQ** in Step Mode: the 2nd-octave keys become scale keys; **OCT** moves them. Both are remembered per track.
- **Chords**: lock a chord on the main screen first; with scale keys a key picks the whole chord and a pad writes it.

---

## 6. Bluetooth
- On the Mac, connect the keyboard in **Audio MIDI Setup → MIDI Studio → Bluetooth**, then set the KamoteKeys Control Surface Input and Output to that Bluetooth port.
- Patch page → **MIDI OUT**: AUTO (USB when connected, else Bluetooth), ALL, USB or BLE.

## 7. If something doesn't work
| Symptom | Check |
|---|---|
| Clip / Step Mode pads stay grey, keys just play notes | the Remote Script isn't on this port: Control Surface Input **and** Output = the port you use; Diagnostics shows `LIVE NONE` |
| no sound from the keyboard | Fader 4 (master volume) and E8; Patch page → internal synth ON |
| pads too dark / too bright | Patch page → E5 pad brightness |
| something odd after an update | replace the Remote Script folder and restart Live |
| want the factory firmware back | open the official SMK37 OPEN workbench → Restore factory V16 |

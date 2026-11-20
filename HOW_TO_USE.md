# How to use KamoteKeys (SMK-37 Pro / Elite)

A hands-on guide. The full reference is in [README.md](README.md); the pictures are in `docs/diagrams/`:

| Diagram | Shows |
|---|---|
| `SMK37-Controls.png` | every button, fader and knob |
| `SMK37-Knobs.png` | what the knobs do on each screen |
| `SMK37-Modes-Table.png` | what each control does in each mode |
| `SMK37-Play-Modes.png` | Normal, Key Scale, Pad Scale and Drum Mode |
| `SMK37-Clip-Mode.png` | Clip Mode (CHORD) |
| `SMK37-Step-Mode.png` | Step Mode (SEQ REC) |
| `SMK37-Live-Keys.png` | the top-octave Live keys, recording and overdub |
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
3. Set its **Input and Output to the same port**: the keyboard's USB port, or its Bluetooth port when you play wirelessly. For both, add KamoteKeys twice (one row USB, one row Bluetooth): the Live commands (play, record, capture, undo …) only work on a port that has its own KamoteKeys row.
4. In the MIDI ports list turn on **Track** and **Remote** for that input, **Remote** for that output.
5. Check on the keyboard: hold **BT 1.5 s** (Diagnostics). The green line `LIVE USB` / `LIVE BLE` means the script answers. In Clip Mode the pads show your clips.

Each time you update KamoteKeys, also replace the Remote Script folder and restart Live.

---

## 2. Everyday controls

| Control | Does |
|---|---|
| **PARA** | Play / stop, like the space bar: stop halts Live and its clips, the next press starts them again |
| **GLOBE** | Light show while held (press again: next pattern; hold 3 s: it stays on; 3 quick presses: ripple lights) |
| **SEQ PLAY** | AUTO on / off (step automation in Step Mode) |
| **KNOB BANK** | Session Record (record / overdub); again while a new clip records: it ends on the next bar line |
| **FADER BANK** | Capture MIDI |
| **◀ / ▶** | Undo / Redo (◀ + PARA or PATCH + PARA = metronome). One press undoes one keyboard action |
| **OCT − / +** | Octave of the keys (or the pads in Pad Scale / Drum) |
| **Fader 1** | Velocity (bottom 10 % = play dynamics, above = fixed) |
| **Fader 2** | Live: volume of the selected track |
| **Fader 3** | Live: Macro 1 of the selected track's rack (no rack: Send A) |
| **Fader 4** | **Master volume** (the keyboard and Live's master) |
| **E1 – E8** | The **8 macros** of Live's selected track (like Move); the screen shows the name and value |
| **SHIFT (top C) + E1 – E5** | BPM, then the selected track's **filter**, **delay**, **reverb**, **reverb decay** |
| **PATCH** | Patch page: **E1 key**, **E2 scale**, **E3 brightness**, E4 internal synth, E5 MIDI out, **E6 voicing**, **E7 CHORD KEYS** |
| **ARP** | Arpeggiator on/off. **Hold**: ARP settings (type: arpeggio, strum, chord repeat, humanize; turn while holding: closes on release) |
| **NOTE REPEAT** | Tap: Sustain (notes hold until the next key; with ARP: holds the arpeggio). **Hold**: Note Repeat (E1 = rate) |
| **CHORD** | Clip Mode. **Hold**: Clip Mode only while held |
| **SCALE / SEQ** | Pad Scale / Key Scale; both can be on together |

Every knob shows what it changes on the screen as you turn it. Pad sensitivity is in Diagnostics (hold BT 1.5 s, E3).

Macros and effects need the Remote Script; the effects need an Auto Filter / Delay (or Echo) / Reverb on the track (the screen says `NO REVERB` etc. if one is missing).

**Top-octave Live strip** (the same in every mode), for the track / clip selected in Live:

| Key | Does | Hold top C (SHIFT) + key |
|---|---|---|
| top F / F# | quantize clip / launch-stop clip | track ◀ / launch scene |
| top G / G# | arm only this track (PATCH + G: add) / finish & next track | track ▶ / scene ▲ |
| top A / A# | mute track / delete clip | duplicate clip / scene ▼ |
| top B | solo only this track | double the loop |
| top E | (plays a note) | fixed record length OFF / 1 / 2 / 4 / 8 bars |

Key and scale are remembered after power-off.

### Record and overdub
| To … | Do |
|---|---|
| record a new clip | select an empty slot on an armed track (top G arms), press **KNOB BANK**; again = it records on to the end of the bar, then loops |
| record exactly 1 / 2 / 4 / 8 bars | **SHIFT + top E** until the screen shows the length (`REC 4 BARS`), then **KNOB BANK**: it stops by itself and loops |
| **overdub** (add notes to a clip) | select a track whose clip is **playing**, arm it, press **KNOB BANK**; old notes stay; again = stop |
| overdub in Clip Mode | hold the **lowest C** and press the clip's pad |
| keep something you played without recording | **FADER BANK** (Capture) |
| undo a bad take / tidy the timing | **◀** / top **F** (quantize) |

---

## 3. Play

### Chords (Normal Mode)
- Hold a chord pad (MAJ, MIN, DIM, SUS4 on the top row; 7, MAJ7, 6, 9 below) and play a key: you get that chord on the key. Combine a type and an extension (MIN + 7 = m7).
- **LOCK** (pad 13): hold your chord pads and tap LOCK. Now every key plays that chord, in **every mode**, until you tap LOCK again.
- Pads 5–8 and 14–16 are free buttons (CC 102–108) for MIDI-mapping in Live.
- **One key, one lush chord (CHORD KEYS):** PATCH page **E7**. Pick **m9** and every key plays a minor 9th on its note (A → A C E G B); also maj9, 9, m11, maj7#11, 9sus4, 6/9, m6/9, add9, madd9, quartal, power, or **SCALE 7TH / 9TH / 11TH / SUS2** (chords from your key and scale). **E6** voicing: CLOSE, SPREAD (root an octave down), DROP 2. Chord pads still override it; turn E7 to OFF for single notes.

### Play in a scale (Key Scale, SEQ)
- Press **SEQ**. White keys now play the scale from E3 with the key from E2; the 2nd C is the root. Black keys are silent, so you can't play a wrong note.
- Chord pads still work and stay in the scale. A locked chord stays on.

### Pads as notes (Pad Scale, SCALE) and drums (Drum, FX)
- **SCALE**: the 16 pads play the scale (root pads cyan). A pad also lights when the playing clip on Live's selected track plays its note. A key you play shows on the screen's keyboard; a pad brings the pad map back.
- **Both at once:** turn on Key Scale (SEQ), then press SCALE: `PAD+KEY SCALE`. The keys play the scale and light their note on the pads.
- **FX**: the pads are GM drums on channel 10 (kick top left, rim below it, snare next …), coloured by drum type. OCT moves them by an octave.

### Lights
- **Light show**: hold **GLOBE**. Press it again for the next pattern; Fader 1 sets the speed. **Hold GLOBE 3 s** and it stays on after you let go (3 s again: off). While it stays on, hold a **top pad and the pad under it** to change pattern by itself: pads 1 + 9 every 5 s, 2 + 10 every 10 s … 8 + 16 every 40 s.
- **Ripple lights**: press **GLOBE three times quickly** to turn ripples on (again to turn them off). Turning them on sends a big rainbow ring from the middle; every press then flashes and sends a coloured ring across the pads.

---

## 4. Launch clips (Clip Mode, CHORD)

Press **CHORD**. The pads are 8 tracks × 2 scenes of Live's Session View, coloured by state (green playing, blue stopped, red recording, violet = Live's selected slot).

| To … | Do |
|---|---|
| play / stop a clip (and select it); select an empty slot and arm its track | press its pad (NORMAL, the default; low **G** returns to it) |
| launch every clip you press | tap **F#** (PLAY mode) |
| stop clips | tap **G#** (STOP mode), press them |
| mute / unmute tracks | tap **F** (MUTE mode), press their clips |
| **launch scenes** | tap the low **A** (SCENE mode): pads 1–8 = scenes 1–8; the scene you press plays next. The screen shows the scene names |
| overdub into a clip | hold the **lowest C** and press it (an empty slot: record a new clip) |
| record a new clip | select an empty slot, press **KNOB BANK** |
| copy / paste a clip | tap **C#**, press the clip; tap **D#**, press where it goes (hold D# for several) |
| delete a clip | hold **A#** and press it |
| scroll scenes / launch a scene | hold top **C** (SHIFT) + top **G# / A#** / top **F#** |
| move the selected clip up / down | hold the **low E** (MOVE) and press top G# / A# |
| select the previous / next track | SHIFT + top **F / G** (hold: one more every 0.5 s; the 8 tracks shown follow; the screen shows their numbers and names) |
| use clips quickly from any mode | **hold CHORD**, press pads, let go |

**Build a song from loops (Scene Arrange):** press the low **D**. The two pad rows are two scenes; tap pads to choose which tracks play in each scene (green on, dim off). Launch a scene with SHIFT + top F#: exactly those tracks play, each with its one clip, so editing a clip changes every scene. Press D again to leave.

**Record a band one track at a time:** select the first track's slot (pad), press **KNOB BANK** to record, play, then press top **G#**: the clip keeps looping and the next track is armed and selected. Press KNOB BANK again to record it, and so on.

---

## 5. Step sequencer (Step Mode, SEQ REC)

Select a track and a slot in Live (or a pad in Clip Mode), then press **SEQ REC**. The track is armed. If the slot is empty, the pads show an empty 1-bar pattern and the clip is made when you press the first step. The 16 pads are the 16 steps of the shown bar.

### Program a beat
1. Press a **drum key** in the 2nd octave (C = kick, D = snare, F# = closed hat …): you hear it and the pads now edit that sound.
2. Tap pads to add / remove steps (amber = note, brighter = louder). Other sounds on a step show as a lilac tint.
3. Pick another drum key and add its steps. **PARA** plays the clip; while Live plays, PARA stops it (space bar).
4. Press **SEQ REC** again to go back to where you came from. An empty clip that Step Mode made is removed (also when you move to another track or scene).

### Keys of the lowest octave
| Key | Does |
|---|---|
| C (hold) + pad n | NOTE LENGTH: new steps are n sixteenths long; hold steps too and they get that length |
| C# / D# | COPY / PASTE (a step pad, a drum key, or a bar in the bar view) |
| D / E | RESOLUTION (1/16, 1/8, 1/32, triplets) / FOLLOW (pages follow the playhead) |
| F | DOUBLE: the clip twice as long, everything copied |
| G (hold) + pad n | LENGTH: the clip becomes n bars (the pattern is repeated into new bars) |
| F# / G# (or ◀ / ▶) | page left / right |
| A | BARS: the pads show the bars; tap one to show it |
| A# | DELETE: tap = clear the page; hold + step = clear that step; hold + drum key = remove that sound; in the bar view hold + bar = delete the bar (hold the pad 3 s to only clear it) |
| B (hold) | DRUM: the pads become Drum Mode pads — tap one to hear and select it |

### More
- **Hold a step and turn a knob**: **E1** velocity, **E2** length, **E3** repeats (1× / 2× / 3×).
- **New steps** take the pad's velocity (Fader 1 above 10 % = fixed); **E2** with no step held = length of new steps.
- The key zone is **16 drum keys (C2–D#3)**; the top octave is the Live strip, as everywhere.
- **Tie**: hold a step and press a later one.
- **Automation without recording**: turn **AUTO** on (SEQ PLAY), hold a step and turn E1–E8: that macro's value is written at that step.
- **Melodies**: press **SEQ** in Step Mode: the 2nd-octave keys become scale keys; **OCT** moves them. Both are remembered per track.
- **Chords**: lock a chord on the main screen first; with scale keys a key picks the whole chord and a pad writes it.

---

## 6. Bluetooth
- On the Mac, connect the keyboard in **Audio MIDI Setup → MIDI Studio → Bluetooth**, then add a KamoteKeys Control Surface row with Input and Output = that Bluetooth port (keep the USB row too). Without it, notes play over Bluetooth but play / record / capture do nothing.
- Not connecting: in the BT menu E1 must be ON and E3 (role) DEVICE; FORGET MIDI PAIRS on the keyboard (E7, ▶ ▶) and remove the keyboard on the Mac, power-cycle, connect again.
- Patch page → **MIDI OUT**: AUTO (the port Live talks on; a USB cable that only charges does not count), ALL, USB or BLE.

## 7. If something doesn't work
| Symptom | Check |
|---|---|
| Clip / Step Mode pads stay grey, keys just play notes | the Remote Script isn't on this port: Control Surface Input **and** Output = the port you use; Diagnostics shows `LIVE NONE` |
| no sound from the keyboard | Fader 4 (master volume) and E8; Patch page → internal synth ON |
| pads too dark / too bright | Patch page → E3 brightness |
| pads too sensitive / not enough | Diagnostics (hold BT 1.5 s) → E3 pad sensitivity |
| play / record / capture work on USB but not Bluetooth | add a KamoteKeys Control Surface row for the Bluetooth port (Input and Output) |
| something odd after an update | replace the Remote Script folder and restart Live |
| want the factory firmware back | open the official SMK37 OPEN workbench → Restore factory V16 |

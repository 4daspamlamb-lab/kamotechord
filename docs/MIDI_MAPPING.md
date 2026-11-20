# KamoteKeys MIDI mapping

All values are identical on USB and Bluetooth. Which port(s) carry them depends on **MIDI OUT** (Patch page): AUTO (default: USB when connected, else BLE), ALL, USB or BLE. Note Offs are always offered to every port; the router drops the ones a port never had a Note On for.

## Channels

| Source | Channel |
|---|---|
| Keyboard (Normal Mode) | 1 |
| Keyboard (Key Scale Mode) | 1 |
| Chord-generated notes | 1 |
| Pad Scale Mode | 1 |
| Spare pads (CC buttons) | 15 |
| Drum Mode | 10 |
| Ableton control buttons | 15 |
| Clip Mode launcher and feedback | 16 (see [ABLETON_REMOTE_SCRIPT.md](ABLETON_REMOTE_SCRIPT.md)) |
| Pitch wheel, modulation (CC 1), sustain pedal (CC 64) | 1 |

## Ableton control changes (channel 15)

| Function | CC | Button | Values |
|---|---|---|---|
| Stop | 21 | FM-1 only (GLO); on the SMK-37 PARA stops | 127 press / 0 release |
| Capture MIDI | 22 | FADER BANK | 127 press / 0 release |
| Play/Pause | 23 | PARA (FM-1: SAVE tap) | 127 press / 0 release; like Live's space bar: stop (transport and clips), or start again (the clips resume) |
| Undo | 24 | ◀ LEFT (FM-1: HOME + OCT −) | 127 then 0 on release (not after ◀ + PARA, not in the BT menu) |
| Metronome | 26 | PATCH + PARA or ◀ + PARA (FM-1: HOME + SAVE) | 127 press / 0 release; the script toggles the metronome |
| Session Record | 25 | KNOB BANK | 127 press / 0 release (the Remote Script toggles Session Record on press; while a new clip records it ends that clip on the next bar line instead) |
| Redo | 27 | RIGHT ▶ | 127 press / 0 release (not while the BT menu is open) |

These CCs are sent to the external ports only, never to the internal synth.

## Keyboard

37 keys, lowest key C. Octave −4 … +4; the keyboard starts at **−2** on every power-on (lowest key = 24).

- **Normal:** note = 48 + key index + 12 × octave (C3 … C6 at octave 0).
- **Key Scale:** the 2nd physical C (key index 12) = tonic in the C4–B4 octave (60 + root) + 12 × octave. White keys step through scale degrees above and below it; black keys are silent.
- Out-of-range results (below 0 or above 127) are silent.
- **CHORD KEYS** (Patch page E7): a key plays a chord on its note, on channel 1, unless chord pads are held. Semitones above the note: m9 0 3 7 10 14 · maj9 0 4 7 11 14 · 9 0 4 7 10 14 · m11 0 3 7 10 14 17 · maj7#11 0 4 7 11 18 · 9sus4 0 5 7 10 14 · 6/9 0 4 7 9 14 · m6/9 0 3 7 9 14 · add9 0 4 7 14 · madd9 0 3 7 14 · quartal 0 5 10 15 19 · power 0 7 12. SCALE 7TH / 9TH / 11TH / SUS2 stack scale steps 0 2 4 6 (8, 10) or 0 1 4 from the note's degree; a note outside the scale uses 0 4 7 10 (14, 17) or 0 2 7. Voicing (E6): CLOSE as listed, SPREAD = the lowest note 12 down, DROP 2 = the second note from the top 12 down. At most 6 notes.

## Chord pads (Normal / Key Scale Mode)

| Pad | Adds | Pad | Adds |
|---|---|---|---|
| 1 MAJ | 0 4 7 | 9 "7" | ♭7 (10); 9 = dim7 with DIM; diatonic 7th when no quality pad |
| 2 MIN | 0 3 7 | 10 MAJ7 | 11 (diatonic 7th when no quality pad) |
| 3 DIM | 0 3 6 | 11 "6" | 9 (diatonic 6th when no quality pad) |
| 4 SUS4 | 0 5 7 | 12 "9" | 14 (diatonic 9th when no quality pad) |
| | | 13 LOCK | latches the current modifiers (no notes of its own) |

Only extension pads held → the chord is built diatonically when the played note is in the scale. Off-scale notes fall back to a major triad. Voicing: root position, ascending, at most 6 notes, notes above 127 dropped.

## Pad Scale Mode (channel 1)

Pad *n* (1–16) = 48 + root + scale degree (*n* − 1) + 12 × pad octave (starts at 0). Root pads are lit cyan, others green.

## Drum Mode (channel 10)

| Pad | Note at octave 0 | GM sound | Colour |
|---|---|---|---|
| 1 | 36 | Bass Drum 1 | red (kick) |
| 2 | 38 | Acoustic Snare | orange (snare) |
| 3 | 40 | Electric Snare | orange |
| 4 | 42 | Closed Hi-Hat | yellow (hi-hat) |
| 5 | 44 | Pedal Hi-Hat | yellow |
| 6 | 46 | Open Hi-Hat | yellow |
| 7 | 48 | Hi-Mid Tom | blue (tom) |
| 8 | 50 | High Tom | blue |
| 9 | 37 | Side Stick | purple (rim) |
| 10 | 39 | Hand Clap | pink (clap) |
| 11 | 41 | Low Floor Tom | blue |
| 12 | 43 | High Floor Tom | blue |
| 13 | 45 | Low Tom | blue |
| 14 | 47 | Low-Mid Tom | blue |
| 15 | 49 | Crash Cymbal 1 | teal (cymbal) |
| 16 | 51 | Ride Cymbal 1 | teal |

The pad octave (starts at 0, set with OCT while in Drum Mode) adds or subtracts 12 on every pad (pad 1: 24 at −1, 48 at +1). The colours follow the note actually played, e.g. at +1 pad 1 (48) turns blue (tom). Notes 54–81 that are not kick/snare/hat/tom/cymbal are green (percussion); notes outside GM are grey.

## Spare pads (Normal / Key Scale Mode, channel 15)

Mappable momentary buttons (since 0.43): Control Change, value 127 on press and 0 on release. CC 102–108 are undefined in the MIDI specification, so they do not collide with standard controls. Map them in Ableton with ⌘M (the Remote Script leaves them free).

| Pad | 5 | 6 | 7 | 8 | 14 | 15 | 16 |
|---|---|---|---|---|---|---|---|
| CC | 102 | 103 | 104 | 105 | 106 | 107 | 108 |
| Colour | red | orange | yellow | green | cyan | blue | violet |

## Velocity

Natural velocity comes from the keybed/pad sensors (1–127). Fader 1 above 10 % overrides it at Note On (≈13 at just above 10 %, 127 at 100 %). Note Off velocity is 0.

## ARP and Sustain output

ARP notes use the channel of the source they arpeggiate (1, or 10 in Drum Mode). Each step is a complete Note On/Note Off pair; gate 10–100 %.

Sustain (NOTE REPEAT button, latching) and a sustain pedal share one state and send **CC 64 on channel 1**: 127 when either turns sustain on, 0 when both are off. No duplicates.

## Note ownership

Every note has an owner count per channel. If two sources play the same note (for example two held chords that share a note), one Note On is sent, and the Note Off goes out only when the last owner releases. Mode, octave, scale and root changes never change the pitch of a note that is already sounding: its release always stops exactly what was started.

## Clip Mode (channel 16)

| Pad | 1–8 | 9–16 |
|---|---|---|
| Note | 36–43 (Track 1–8, Clip 1) | 44–51 (Track 1–8, Clip 2) |
| with OVERDUB held (1st black key) | 52–59 | 60–67 |
| with DELETE held (2nd black key) | 68–75 | 76–83 |
| with SELECT held (3rd black key) | 84–91 | 92–99 |

Scene keys (Clip Mode: fourth black key = launch, last two black keys = up / down): CC 28 scene down, CC 29 scene up, CC 30 launch selected scene (channel 15, 127 press / 0 release). The script sends CC 104 on channel 16 = first scene shown on the grid.

Tempo from the Remote Script: CC 102 / 103 = Live's tempo × 10 (MSB, LSB). While it arrives, ARP and the internal engine follow Live, and turning E1 changes Live's tempo (channel 15 CC 37, value = 64 + BPM change; the screen shows "LIVE TEMPO").

Feedback from the Remote Script: CC 80–95 = state 0 empty, 1 stopped, 2 playing, 3 queued, 4 recording, 5 queued to record, 6 empty on an armed track; CC 126 = 1 script connected. KamoteKeys sends CC 127 = 127 to request a full update. Channel 16 is never sent to the internal synth.

## Step Mode (channel 14, 0.58)

| Direction | Message | Meaning |
|---|---|---|
| keyboard → Live | Note 36–51 | step pads 1–16 (press / release) |
| keyboard → Live | Note 60–76 | DRUM, page ◀, page ▶, LENGTH, DOUBLE, RESOLUTION, FOLLOW, COPY, CLEAR, velocity 1–6, velocity down, velocity up |
| keyboard → Live | Note 77–85 | PASTE, BARS, OCT −, OCT +, SEQ (scale keys), repeat 1× / 2× / 3×, NOTE LENGTH (hold); the SMK-37 no longer sends velocity, repeat and note-length notes: hold a step + E1 / E2 / E3 instead (CC 20–22) |
| keyboard → Live | Notes on **channel 13** | key-zone key pressed: select that pitch, or a locked chord (all note-ons, then all note-offs) |
| keyboard → Live | Note on channel 10 (drum keys) or 1 (scale keys) | the key's own sound, for the armed track and the internal synth |
| keyboard → Live | CC 127 | 127 = Step Mode started (repeated every 2 s), 0 / 64 = ended |
| keyboard → Live | Note 87 | PARA in Step Mode: while Live plays, stop (like the space bar); otherwise make the clip if needed and play it |
| keyboard → Live | Note 85 | NOTE LENGTH (bottom C, hold): + step pad n = n sixteenths, for new steps and for steps held at the same time |
| keyboard → Live | CC 30–37 | AUTO on, step held: macro 1–8 automation for the held steps, relative (64 + knob steps) |
| keyboard → Live | CC 38 | AUTO on (127) / off (0), sent on SEQ PLAY and on entering Step Mode |
| Live → keyboard | CC 80–95 | pad colour code |
| Live → keyboard | CC 96–106 | page, page count, resolution, follow, status, drum view, selected pitch, key-zone octave (64 = default), scale keys on, playing bar (0 = stopped), clip length in bars |

Channel 14 never reaches the internal synth.

**0.77:** channel 15 CC 23 (PARA) = Start / Stop like the spacebar; CC 35 = master volume (Fader 4, value 0–127 → Live master 0 … 0 dB); CC 36 = finish & next track (top G#; FM-1: PLAY); CC 33 / 34 = move the selected clip up / down; channel 16 note 103 = Clip Mode STOP key (press / release).

**Copy / paste (0.65).** Keyboard → Live: channel 16 notes 100 / 101 = Clip Mode COPY / PASTE (press / release); in Step Mode COPY / PASTE are the channel 14 function notes 67 / 77. Live → keyboard, channel 16: CC 106 = clipboard kind (0 none, 1 clip, 2 step, 3 sound, 4 bar; +32 copy waiting, +64 paste waiting), CC 107 / 108 = its numbers (clip: track, scene; step: step, page; sound: note, page; bar: bar).

**0.66:** channel 16 note 102 = Scene Arrange on / off; channel 15 CC 31 / 32 = previous / next track (selects the track; the 8-track view follows when the selection leaves it); Live → keyboard channel 16 CC 109 = first track shown, CC 110 = Scene Arrange on, clip state codes 7 / 8 / 9 = arranged track on / off / no clip. Step Mode CC 127 = 127 is repeated every 2 s on every port (heartbeat).

**Selected track and tempo, channel 15 (keyboard → Live):**

| CC | Control | Value |
|---|---|---|
| 37 | SHIFT + E1 with Live connected | 64 + BPM change (relative) |
| 38 | Fader 2 | selected track volume, 0–127 (127 = 0 dB) |
| 39 | Fader 3 | Macro 1 of the selected track's first rack, else its Send A, 0–127 |
| 40 / 41 / 42 / 43 | SHIFT (top C) + E2 / E3 / E4 / E5 | selected track's filter / delay amount / reverb amount / reverb decay, relative: 64 + knob steps |
| 44 | top B | solo the selected track only, other solos off (127 press / 0 release, also below) |
| 45 | top F# | launch / stop the selected clip |
| 47 | top A# | delete the selected clip |
| 48 | top A | mute the selected track |
| 49 | top G | arm only the selected track (others disarmed) |
| 51 | PATCH + top G | arm the selected track, others stay armed |
| 52 | SHIFT + top A | duplicate the selected clip into the next empty slot below |
| 53 | SHIFT + top B | double the selected clip's loop |
| 54 | KNOB BANK (sent just before CC 25) | fixed record length in bars, 0 = off |
| 50 | top F | quantize the selected clip |
| 55–62 | E1–E8 (main screen) | macro 1–8 of the selected track's first rack (else its first device), relative: 64 + knob steps; the script answers on channel 16 CC 118 / 119 and the name in slot 9 |
| 63 | Clip Mode low G / F# / G# / F / A | pad mode: 0 normal (play / stop + select), 1 play, 2 stop, 3 mute, 4 scenes (pads 1–8 launch scenes; the script sends scene states on CC 80–87 and scene names in slots 0–7) |

Top G# sends CC 36 (finish & next track). Top C is SHIFT and sends nothing; while it is held, top G# / A# send CC 29 / 28 (scene up / down), top F# CC 30 (launch scene) and top F / G CC 31 / 32 (track left / right); SHIFT + top E changes the fixed length on the keyboard (nothing is sent). In Clip Mode, low E (MOVE) held + top G# / A# send CC 33 / 34 (move the clip). The top keys send these in every mode.

**Step Mode knobs:** with a step pad held, E1 / E2 / E3 send channel 14 CC 20 / 21 / 22 = velocity / length / repeats of the held steps, relative (64 + knob steps).

**Selected clip notes:** channel 16 Note On / Off from Live = a note the playing clip on Live's selected track sounds now; it lights the Pad Scale pad of that note and never sounds.

**Track names and effect values, channel 16 (Live → keyboard):**

| CC | Meaning |
|---|---|
| 111 | start a name: 0–7 = the 8 tracks shown (Scene Mode: the 8 scenes), 8 = Live's selected track, 9 = the macro just turned |
| 112 | next character of that name (ASCII 32–126, at most 12) |
| 113 | Live's selected track number (1 based, 0 = none) |
| 114–117 | new position of the effect just changed, 0–126 (filter: 63 = open, lower = low-pass, higher = high-pass); 127 = the track has no such device |
| 118 / 119 | macro just turned: index 0–7 / value 0–126 (127 = no device); its name arrives in name slot 9 |

## Notes received from the host

Note On/Off on channels 1 and 10 coming from the computer light the matching Pad Scale/Drum pads (Move-style). They also play the internal synth unless *Internal synth* is off.

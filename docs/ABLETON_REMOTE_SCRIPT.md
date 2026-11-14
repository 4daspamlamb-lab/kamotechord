# KamoteKeys Remote Script and Clip Mode

> **FM-1:** everything below applies; where it says "SMK37 Open", choose the **FM-1's own USB or Bluetooth MIDI port** instead. Clip pads are white keys 01-16, Clip Mode is the LFO button, REC is Session Record and HOME + REC is Capture MIDI (see [FM1.md](FM1.md)).

The Remote Script turns KamoteKeys into a two-way Ableton Live controller: an 8 × 2 clip launcher with live LED feedback, plus Start/Stop/Capture/Undo/Redo/Session Record without manual MIDI mapping.

> Tested against a mock Live API (`tests/remote_script`); **not yet tested inside Ableton Live.** Uses the standard `_Framework` API shipped with Live 10.1 – 12.

## Install

1. Copy the folder `remote_script/KamoteKeys` (the folder itself, containing `__init__.py` and `KamoteKeys.py`) to
   - macOS: `~/Music/Ableton/User Library/Remote Scripts/KamoteKeys`
   - Windows: `\Users\<you>\Documents\Ableton\User Library\Remote Scripts\KamoteKeys`
   Create `Remote Scripts` if it does not exist.
2. Restart Live.
3. *Settings → Link, Tempo & MIDI*:
   - **Control Surface:** `KamoteKeys`; **Input** and **Output:** the KamoteKeys port you use (USB `SMK37 Pro Open…` *or* Bluetooth `SMK37 Pro Open_BLE`), the same port for both.
   - In *MIDI Ports*, enable **Track** (and **Remote**) on that input so keys and pads still play your tracks.
4. Press **CHORD** on KamoteKeys: the header shows **LIVE** when the script answers, and the pads show your clips.

Remove any manual MIDI mappings of the KamoteKeys transport buttons; the script now handles them (otherwise actions happen twice).

## Checking that it works

- When Live starts, its status bar (bottom of the window) briefly shows **KamoteKeys Remote Script loaded**.
- In Clip Mode, every pad press shows what happened, e.g. `KamoteKeys pad 3: Bass 3 / clip 1 - launch`, `... - stop`, `... empty slot, track not armed: nothing to do` or `... track 7 does not exist`.
- On the keyboard, the header shows **LIVE** and the pads show clip colours (grey only = no script answering).
- **Most common problem:** the Control Surface row shows Input/Output **None**. That happens when the keyboard's port name changed (for example from "Felucca" under older firmware to "SMK37 Open"). Select **SMK37 Open** for both, and tick **Remote** on the SMK37 Open output port. Live's `Log.txt` shows the current setting as `MidiRemoteScript N [Control Surface="KamoteKeys" Input=... Output=...]`.
- Nothing in the status bar when you press a pad: the script is not receiving the pads (since 0.38 the script uses Live's standard control objects; Log.txt shows `KamoteKeys: first message received from the keyboard` once input arrives). Check that the Control Surface's Input is the port the keyboard sends on (with MIDI OUT = AUTO, that is **USB** whenever USB is connected), and look for `KamoteKeys` in Live's `Log.txt` (macOS: `~/Library/Preferences/Ableton/Live <version>/Log.txt`).
- A launched clip blinks until Live's global launch quantization point (e.g. the next bar), then plays.

Without the script, clip slots can still be MIDI-mapped by hand (⌘M, click a slot, press a pad), without LED feedback.

## Clip Mode (CHORD button)

```
 ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
 │ T1 C1│ T2 C1│ T3 C1│ T4 C1│ T5 C1│ T6 C1│ T7 C1│ T8 C1│  pads 1-8 : Clip 1 (scene 1)
 ├──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┤
 │ T1 C2│ T2 C2│ T3 C2│ T4 C2│ T5 C2│ T6 C2│ T7 C2│ T8 C2│  pads 9-16: Clip 2 (scene 2)
 └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
```

| Press | Result (always follows Live's global launch quantization) |
|---|---|
| a stopped clip | launches it (switches clips on that track) |
| the playing or queued clip | stops it |
| the recording clip | finishes recording (Live keeps playing it) |
| an empty slot on an **armed** track | starts recording a new clip |
| an empty slot on an unarmed track | nothing (never stops a track by accident) |

**Overdub and delete (0.50).** Hold a key and press a clip:

| Hold | + press a clip | Result |
|---|---|---|
| the **first black key** (SMK-37: C#, FM-1: F#) | a clip | **overdub**: launches it if stopped and turns on Session Record, so you record into it; the same again turns overdub off |
| | an empty slot | arms the track and records a new clip |
| the **second black key** (SMK-37: D#, FM-1: G#) | a clip | **deletes** it |
| the **third black key** (SMK-37: F#, FM-1: A#) | a clip | **selects** the slot and **arms its track** (disarms the others): the next Session Record (KNOB BANK / FM-1 REC) records there |

**Scenes (0.56).** The **fourth black key** (SMK-37: G#, FM-1: C#) **launches the selected scene**; the **last two black keys** (SMK-37: top G#, A#; FM-1: top D#, F#) are **scene up** and **scene down**. Scene up/down moves Live's selected scene and scrolls the 8 × 2 grid with it (pads 1–8 = that scene, 9–16 = the next); the pads show track.scene, e.g. T3.5.

The screen shows OVERDUB or DELETE while the key is held; in Clip Mode these two keys play no note. A plain press on a clip that is being overdubbed turns overdub off (the clip keeps playing); the next press stops it as usual.

**Highlighted slot (0.57).** The pad of Live's highlighted (selected) slot turns violet; a playing, queued or recording clip pulses between its colour and violet. On the FM-1 its key blinks slowly.

**Tempo (0.54).** The script sends Live's tempo to the keyboard. While Live is connected, ARP follows Live (Ableton has priority): the BPM on the screen turns green and the tempo knob shows TEMPO FOLLOWS ABLETON. Without Live, the keyboard uses its own tempo again. Update the Remote Script folder to the 0.50 version: older scripts ignore these presses.

| Clip state | Pad |
|---|---|
| Empty | very dim |
| Empty, track armed | very dim red |
| Stopped | dim blue |
| Playing | bright green |
| Queued | blinking amber |
| Queued to record | blinking red |
| Recording | bright red |

Tracks are Live's visible tracks 1–8 (as in Session View); scenes 1–2. The LEDs follow Live's real state, including changes made with the mouse or other controllers. The script polls Live ~10 times a second, sends only changes, and resends everything every ~5 s and whenever KamoteKeys asks (entering Clip Mode, USB plugged in, Bluetooth connected). Keys, ARP and the faders keep working in Clip Mode; Clip pads never use ARP.

## Recording and transport (no mapping needed with the script)

| Button | CC (ch. 15) | Script action (on press) |
|---|---|---|
| SEQ PLAY | 20 | Play |
| GLOBE | 21 | Stop |
| FADER BANK | 22 | Capture MIDI |
| PARA (FM-1: SAVE tap) | 23 | **Play/Pause**: stops, or continues from where it stopped |
| ◀ | 24 | Undo |
| PATCH + PARA or ◀ + PARA (FM-1: HOME + SAVE) | 26 | **Metronome** on/off |
| KNOB BANK | 25 | **Session Record** on/off: records new clips on armed tracks at the selected scene, or overdubs playing clips |
| ▶ | 27 | Redo |

All buttons send 127 on press and 0 on release (KNOB BANK and FADER BANK send on release since 0.59, because holding both runs the light show).

## Step Mode: drum step sequencer (0.58)

**Press SEQ REC** (or hold CHORD for 0.6 s; FM-1: hold **LFO**) to enter Step Mode; press SEQ REC again (or hold CHORD) to leave, tap CHORD to go to Clip Mode. It works on Live's **selected track** (ideally with a Drum Rack) and its **selected clip slot**:

- Every step is written straight into that **session clip** (Live 12 note API), so the result is an ordinary MIDI clip you can edit in the piano roll. Notes you add or remove with the mouse show up on the pads.
- An **empty slot gets a new 1-bar clip** named after the track.
- One Drum Rack sound is **active** at a time; steps edit that sound.

**Pads:** the 16 pads are 16 steps of the shown page (1 bar at 1/16). Tap = note on/off.

Colours: see the table below.

**Keyboard (SMK-37), like Move's drum pads and buttons** ([diagram](diagrams/SMK37-Step-Mode.png))

| Keys | Do |
|---|---|
| lowest octave, white | DRUM (hold), page ◀, page ▶, LENGTH (hold), DOUBLE, RESOLUTION, FOLLOW |
| lowest octave, black | C# = COPY, D# = PASTE, F# = BARS (hold), A# = CLEAR; G# plays |
| second octave = **key zone** | drum keys: C … B = 12 sounds from C1 (kick, rim, snare, clap …); with SEQ: scale keys |
| top five white keys (F G A B C) | velocity 40 / 60 / 80 / 100 / 120 |
| top three black keys (F# G# A#) | hold + a pad: the step plays 1× / 2× / 3× |
| OCT − / OCT + | moves the key zone by an octave (remembered per track) |
| SEQ | key zone = drum keys / **scale keys** (remembered per track; tracks without a Drum Rack start with scale keys) |

- **Key zone**: a key **plays its sound and selects it** for the steps, like tapping a drum pad on Move. With SEQ the white keys play the scale set with Faders 2 / 3 (black keys silent), so melodic tracks can be step-sequenced too. Sounds 13–16 and other Drum Rack banks: hold DRUM and use the pads / page keys.
- **Other sounds on a step**: an empty step of the selected sound shows a violet tint when other sounds play there, stronger with 2 and 3+ of them.
- **LENGTH (hold) + pad n** sets the clip to **n bars** (pads show the bars while LENGTH is held). Growing a clip repeats its pattern into the new bars (if they are empty).
- **BARS (hold F#)**: pads show the clip's bars (teal = in the clip, white = the bar on the pads, green = playing); tap one to show it.
- **Repeats**: pick a sound, hold the top F# / G# / A# and press a pad: that step plays 1 / 2 / 3 times (pink).
- **COPY / PASTE** (C# / D#): tap COPY to copy the shown page (all sounds), go to another page and tap PASTE to replace it. **COPY + key** copies just that sound's steps; **PASTE + key** pastes them onto that sound.
- **CLEAR + key** (A#) deletes every note of that sound in the clip (Move: Delete + pad). A plain **CLEAR tap** clears the selected sound on the shown page.
- **Auto-arm**: entering Step Mode arms the sequenced track (if it was not armed) so the keys are heard; leaving disarms it again.
- **Screen**: the bar the clip is playing (BAR 2/4; when stopped its length, 4 BARS), page, resolution, FOLLOW, the selected note and SCALE when the key zone is scale keys.

| Pad colour | Meaning |
|---|---|
| amber (brighter = louder) | step with a note of the selected sound |
| dim amber | inside a longer (tied) note |
| pink | step repeated 2× / 3× |
| violet tint (stronger = more) | empty step where other sounds play |
| dim blue | empty step |
| green / white | playhead on a note / on an empty step |
| cyan | step held |
| off | past the end of the clip |

FM-1 keys (white keys = steps): DRUM F#, page G# / A#, LENGTH C#, DOUBLE D#, RESOLUTION F# (2nd), FOLLOW G# (2nd), COPY A# (2nd), CLEAR C# (2nd), velocity down / up D# / F# (top). Tie: hold a step and press a later one (pads; FM-1 white keys).

On the SMK-37 the remaining keys (lowest G#, third-octave C–E) still play notes. The screen also shows NO DRUM RACK (steps then use C1) or SELECT A MIDI TRACK.

**Edge cases:** no Drum Rack → steps still work on C1; the clip or track deleted, the selection or the clip length changed with the mouse → the mode rebinds itself (deferred, never inside a Live notification). Messages go to Live's `Log.txt` as `KamoteKeys step: ...` and to the status bar.

## Move-style note feedback from Ableton

In Pad Scale and Drum Mode, pads light whenever their note sounds: your own pads, ARP steps, the keyboard, **and notes played back by Live**. To see a clip's notes on the pads, set that MIDI track's *MIDI To* to the KamoteKeys port, channel 1 (melodic) or 10 (drums). Turn **Internal synth** off (Patch page) if you do not want KamoteKeys to sound those notes too.

## Protocol (firmware ⇄ script)

| Direction | Message | Meaning |
|---|---|---|
| KamoteKeys → Live | ch 16 Note On/Off 36–51 | Clip pads 1–16 pressed / released |
| KamoteKeys → Live | ch 16 CC 127 = 127 | request full state |
| KamoteKeys → Live | ch 15 CC 20/21/22/24/25/27 | transport / recording buttons |
| Live → KamoteKeys | ch 16 CC 80–95 = 0…6 | clip state of pads 1–16 |
| Live → KamoteKeys | ch 16 CC 126 = 1 | script connected (protocol 1); 0 on disconnect |

The script and firmware stay compatible across updates as long as this protocol (version 1) is unchanged.

# KamotKeys Remote Script and Clip Mode

The Remote Script turns KamotKeys into a two-way Ableton Live controller: an 8 × 2 clip launcher with live LED feedback, plus Start/Stop/Capture/Undo/Redo/Session Record without manual MIDI mapping.

> Tested against a mock Live API (`tests/remote_script`); **not yet tested inside Ableton Live.** Uses the standard `_Framework` API shipped with Live 10.1 – 12.

## Install

1. Copy the folder `remote_script/KamotKeys` (the folder itself, containing `__init__.py` and `KamotKeys.py`) to
   - macOS: `~/Music/Ableton/User Library/Remote Scripts/KamotKeys`
   - Windows: `\Users\<you>\Documents\Ableton\User Library\Remote Scripts\KamotKeys`
   Create `Remote Scripts` if it does not exist.
2. Restart Live.
3. *Settings → Link, Tempo & MIDI*:
   - **Control Surface:** `KamotKeys`; **Input** and **Output:** the KamotKeys port you use (USB `SMK37 Pro Open…` *or* Bluetooth `SMK37 Pro Open_BLE`), the same port for both.
   - In *MIDI Ports*, enable **Track** (and **Remote**) on that input so keys and pads still play your tracks.
4. Press **CHORD** on KamotKeys: the header shows **LIVE** when the script answers, and the pads show your clips.

Remove any manual MIDI mappings of the KamotKeys transport buttons; the script now handles them (otherwise actions happen twice).

## Checking that it works

- When Live starts, its status bar (bottom of the window) briefly shows **KamotKeys Remote Script loaded**.
- In Clip Mode, every pad press shows what happened, e.g. `KamotKeys pad 3: Bass 3 / clip 1 - launch`, `... - stop`, `... empty slot, track not armed: nothing to do` or `... track 7 does not exist`.
- On the keyboard, the header shows **LIVE** and the pads show clip colours (grey only = no script answering).
- **Most common problem:** the Control Surface row shows Input/Output **None**. That happens when the keyboard's port name changed (for example from "Felucca" under older firmware to "SMK37 Open"). Select **SMK37 Open** for both, and tick **Remote** on the SMK37 Open output port. Live's `Log.txt` shows the current setting as `MidiRemoteScript N [Control Surface="KamotKeys" Input=... Output=...]`.
- Nothing in the status bar when you press a pad: the script is not receiving the pads (since 0.38 the script uses Live's standard control objects; Log.txt shows `KamotKeys: first message received from the keyboard` once input arrives). Check that the Control Surface's Input is the port the keyboard sends on (with MIDI OUT = AUTO, that is **USB** whenever USB is connected), and look for `KamotKeys` in Live's `Log.txt` (macOS: `~/Library/Preferences/Ableton/Live <version>/Log.txt`).
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

| Clip state | Pad |
|---|---|
| Empty | very dim |
| Empty, track armed | very dim red |
| Stopped | dim blue |
| Playing | bright green |
| Queued | blinking amber |
| Queued to record | blinking red |
| Recording | bright red |

Tracks are Live's visible tracks 1–8 (as in Session View); scenes 1–2. The LEDs follow Live's real state, including changes made with the mouse or other controllers. The script polls Live ~10 times a second, sends only changes, and resends everything every ~5 s and whenever KamotKeys asks (entering Clip Mode, USB plugged in, Bluetooth connected). Keys, ARP and the faders keep working in Clip Mode; Clip pads never use ARP or Note Repeat.

## Recording and transport (no mapping needed with the script)

| Button | CC (ch. 15) | Script action (on press) |
|---|---|---|
| SEQ PLAY | 20 | Play |
| GLOBE | 21 | Stop |
| FADER BANK | 22 | Capture MIDI |
| ◀ | 24 | Undo |
| KNOB BANK | 25 | **Session Record** on/off: records new clips on armed tracks at the selected scene, or overdubs playing clips |
| ▶ | 27 | Redo |

All buttons send 127 on press and 0 on release (KNOB BANK is now momentary; its LED lights only while pressed).

## Move-style note feedback from Ableton

In Pad Scale and Drum Mode, pads light whenever their note sounds: your own pads, ARP steps, Note Repeat, the keyboard, **and notes played back by Live**. To see a clip's notes on the pads, set that MIDI track's *MIDI To* to the KamotKeys port, channel 1 (melodic) or 10 (drums). Turn **Internal synth** off (Patch page) if you do not want KamotKeys to sound those notes too.

## Protocol (firmware ⇄ script)

| Direction | Message | Meaning |
|---|---|---|
| KamotKeys → Live | ch 16 Note On/Off 36–51 | Clip pads 1–16 pressed / released |
| KamotKeys → Live | ch 16 CC 127 = 127 | request full state |
| KamotKeys → Live | ch 15 CC 20/21/22/24/25/27 | transport / recording buttons |
| Live → KamotKeys | ch 16 CC 80–95 = 0…6 | clip state of pads 1–16 |
| Live → KamotKeys | ch 16 CC 126 = 1 | script connected (protocol 1); 0 on disconnect |

The script and firmware stay compatible across updates as long as this protocol (version 1) is unchanged.

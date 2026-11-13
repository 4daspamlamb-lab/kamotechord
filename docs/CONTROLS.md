# Button and pad layout

## Physical buttons

```
 LEFT BLOCK (3 × 2)                 RIGHT BLOCK (2 × 6)
 ┌──────────────┬──────────────┐    ┌───────┬─────────────┬────────┬────────┬────────┬────────┐
 │ [0] KNOB BANK│[1] FADER BANK│    │ [6]   │ [7]         │ [8]    │ [9]    │ [10]   │ [11]   │
 │ Rec/Overdub  │ Capture MIDI │    │ ARP   │ NOTE REPEAT │ SCALE  │ CHORD  │ GLOBE  │ BT     │
 │ CC25 mom.    │ CC22         │    │       │ SUSTAIN     │ PadScl │ Clip   │ Stop   │ menu   │
 ├──────────────┼──────────────┤    ├───────┼─────────────┼────────┼────────┼────────┼────────┤
 │ [2] ◀ LEFT   │ [3] RIGHT ▶  │    │ [12]  │ [13]        │ [14]   │ [15]   │ [16]   │ [17]   │
 │ Undo CC24    │ Redo CC27    │    │ PATCH │ PARA Play/Ps│ FX     │ SEQ    │ PLAY   │ REC    │
 ├──────────────┼──────────────┤    │       │             │ Drum   │ KeyScl │ CC20   │ lights │
 │ [4] OCT −    │ [5] OCT +    │    └───────┴─────────────┴────────┴────────┴────────┴────────┘
 └──────────────┴──────────────┘
 [n] = scanner button index in app/board/scan.c (btnmap order)
```

### How the index → label mapping was verified

The scanner lists its 18 buttons "left to right" (`btnmap`, 左到右), in pairs for the left block and two rows of six for the right block. SMK37 OPEN's own panel model (`sdk/Simulator.js`, `labels[]`) labels the same indexes SHIFT, FADER BANK, HOME, SAVE, OCT −, OCT +, ARP, PAD/STEP, SCL, ENV/LFO, MIX, BT, HOME/SRC, EDIT, FX, SEQ, PLAY, REC, puts indexes 0–5 in the left block and 6–17 in the right block, and its firmware uses index 1 for FADER BANK, 11 for BT, 4/5 for OCT and 16/17 for PLAY/REC. Lined up with the SMK-37 front-panel legends (ARP, NOTE REPEAT, SCALE, CHORD, GLOBE, BT / PATCH, PARA, FX, SEQ, SEQ PLAY, SEQ REC; KNOB BANK, FADER BANK, ◀, ▶, OCT −, OCT +), every fixed point agrees. The constants live in `app/kamotkeys/kk_config.h` (`KK_BTN_*`). **This mapping still needs a one-time check on hardware:** press each button and confirm the action in the table below and the LED that lights.

### Behaviour

| Button | Action | LED |
|---|---|---|
| ARP | tap: ARP on/off; hold 0.6 s: ARP page | bright while ARP is on |
| NOTE REPEAT | **SUSTAIN** latch: tap on (CC 64 = 127, ch 1), tap off (CC 64 = 0); combined with a pedal | bright while sustain is latched |
| SCALE | Pad Scale Mode on/off | bright **only** in Pad Scale Mode |
| CHORD | Clip Mode on/off | bright in Clip Mode |
| GLOBE | Ableton Stop (CC 21) | dim; bright while pressed |
| BT | Bluetooth menu on/off; hold 1.5 s: diagnostics | bright while a menu is open |
| PATCH | Patch page on/off | bright while open |
| PARA | Ableton Play/Pause (CC 23); with ◀ held: metronome (CC 26) | dim; bright while pressed |
| FX | Drum Mode on/off | bright in Drum Mode |
| SEQ | Key Scale Mode on/off | bright **only** in Key Scale Mode |
| SEQ PLAY | Ableton Start (CC 20) | dim; bright while pressed |
| SEQ REC | light show (next pattern per press, runs while held) | bright while running |
| KNOB BANK | Ableton Session Record (CC 25, momentary) | dim; bright **only while pressed** |
| FADER BANK | Ableton Capture MIDI (CC 22) | dim; bright while pressed |
| ◀ / ▶ | Undo (CC 24, on release; not when used with PARA) / Redo (CC 27); HOME/SAVE inside the BT menu | dim; bright while pressed |
| OCT − / OCT + | keyboard octave (starts −2); pad octave in Pad Scale/Drum (starts 0); −4 … +4 | dim; bright while pressed |

Mode buttons are mutually exclusive. Pressing the active mode's button returns to Normal Mode.

## Pads

```
 Pad numbering (scanner pad id + 1; top row 1–8, bottom row 9–16)
 ┌────┬────┬────┬────┬────┬────┬────┬────┐
 │  1 │  2 │  3 │  4 │  5 │  6 │  7 │  8 │
 ├────┼────┼────┼────┼────┼────┼────┼────┤
 │  9 │ 10 │ 11 │ 12 │ 13 │ 14 │ 15 │ 16 │
 └────┴────┴────┴────┴────┴────┴────┴────┘

 Normal / Key Scale Mode
 ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 │ MAJ │ MIN │ DIM │SUS4 │CC102│CC103│CC104│CC105│   chord modifiers | CC buttons (ch. 15)
 ├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
 │  7  │MAJ7 │  6  │  9  │LOCK │CC106│CC107│CC108│
 └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

 Drum Mode (octave 0, channel 10) — vertical pairs
 ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 │ 36  │ 38  │ 40  │ 42  │ 44  │ 46  │ 48  │ 50  │
 │KICK │ SNR │SNR2 │ CHH │ PHH │ OHH │ HMT │HTOM │
 ├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
 │ 37  │ 39  │ 41  │ 43  │ 45  │ 47  │ 49  │ 51  │
 │ RIM │CLAP │ LFT │ HFT │LTOM │ LMT │CRS1 │RIDE │
 └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘

 Pad Scale Mode: pads 1→16 = scale degrees 0→15 from the root (root pads cyan)

 Clip Mode: pads 1–8 = Clip 1 of tracks 1–8, pads 9–16 = Clip 2 (channel 16 notes 36–51)
            hold C# + pad = overdub (notes 52–67), hold D# + pad = delete (notes 68–83)
```

The pad numbers follow SMK37 OPEN's convention (pad *n* = scanner pad id *n* − 1, as in its workbench and README: "SHIFT + Pads 1–8 …"). The RGB chain order (`rgb_map` in `app/board/panel.c`) is handled by the board driver, so every mode addresses logical pad numbers only. The assumption that pads 1–8 are the **top** row comes from the LED chain's serpentine order and the SMK37 OPEN workbench grid; confirm it on hardware (pad 1 should light red in Drum Mode).

## Faders, wheels, pedal

| Control | Function |
|---|---|
| Fader 1 | velocity (> 10 %) / light-show speed while SEQ REC is held |
| Fader 2 | scale |
| Fader 3 | root |
| Fader 4 | pad brightness; LCD half-dim at ≤ 10 % |
| Pitch wheel | pitch bend, channel 1 |
| Mod wheel | CC 1, channel 1 |
| Sustain pedal | CC 64, channel 1 |
| E1–E8 | see README "Knobs"; E8 is always master volume |

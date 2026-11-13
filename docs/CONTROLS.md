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

What each button does (tap, hold, combos, LEDs) is described once, in the README: [Controls at a glance](../README.md#controls-at-a-glance) and [RGB and button lighting](../README.md#rgb-and-button-lighting). FM-1: [FM1.md](FM1.md). This file only covers the physical layout and indexes.

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
            hold C# + pad = overdub (notes 52–67), hold D# + pad = delete (notes 68–83),
            hold F# + pad = select + arm (notes 84–99); G# (4th black) = launch selected scene;
            top G# / A# = scene up / scene down
```

The pad numbers follow SMK37 OPEN's convention (pad *n* = scanner pad id *n* − 1, as in its workbench and README: "SHIFT + Pads 1–8 …"). The RGB chain order (`rgb_map` in `app/board/panel.c`) is handled by the board driver, so every mode addresses logical pad numbers only. The assumption that pads 1–8 are the **top** row comes from the LED chain's serpentine order and the SMK37 OPEN workbench grid; confirm it on hardware (pad 1 should light red in Drum Mode).

## Faders, wheels, pedal

Faders: see [README → Faders](../README.md#faders). Pitch wheel = pitch bend, mod wheel = CC 1, sustain pedal = CC 64, all on channel 1. Knobs E1–E8: [README → Knobs](../README.md#knobs-encoders).

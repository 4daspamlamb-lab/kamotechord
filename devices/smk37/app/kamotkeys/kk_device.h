/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys device description: M-VAVE SMK-37 Pro / Elite.
 * Shared code (core/) reads these values; see core/kamotkeys/kk_config.h. */
#ifndef KK_DEVICE_H
#define KK_DEVICE_H

#define KK_DEVICE_SMK37 1
#define KK_DEVICE_NAME "SMK-37"
#define KK_DEVICE_SUFFIX ""
#define KK_FIRMWARE_BASE "SMK37 OPEN"

/* 37 velocity keys, 16 pressure pads, 4 faders, 8 encoders (E8 = MASTER). */
enum { KK_KEYS = 37 };
/* Lowest physical key (scan id 0) is C; the second physical C is id 12. */
enum { KK_KEY_BASE_NOTE = 48, KK_ANCHOR_KEY = 12, KK_ANCHOR_NOTE = 60 };
/* Keyboard octave starts at -2 (0.31). */
enum { KK_KEY_OCTAVE_DEFAULT = -2 };
/* Real velocity: Fader 1 can override it (0-10 % = natural). */
enum { KK_VELOCITY_DEFAULT = 100 };

/* Clip Mode action keys (key indexes): the first black key (C#) held
 * while pressing a clip = OVERDUB, the second (D#) = DELETE. */
/* 0.65 Clip Mode keys: lowest black keys C# COPY, D# PASTE, F# select <-> play,
 * G# nothing, A# DELETE (hold + pad); top F# / G# / A# = scene launch / up / down.
 * A plain pad press selects (slot + arm) unless toggled to play. */
#define KK_CLIP_PRESS_SELECT 1
/* 0.93 Clip Mode pad modes (latched): F# PLAY, G# STOP, F MUTE, G back to NORMAL
 * (play / stop + select). */
#define KK_CLIP_PAD_MODES 1
enum { KK_CLIP_MUTE_KEY = 5, KK_CLIP_NORMAL_KEY = 7, KK_CLIP_SCENE_KEY = 9 /* 0.99: A = Scene Mode (pads 1-8 = scenes) */ };
enum { KK_CLIP_COPY_KEY = 1, KK_CLIP_PASTE_KEY = 3, KK_CLIP_TOGGLE_KEY = 6 /* F#: PLAY (hold / tap, 0.75) */, KK_CLIP_ARRANGE_KEY = 2, /* D: Scene Arrange (0.77) */
       KK_CLIP_STOP_KEY = 8,   /* G#: STOP (hold + clips, or tap: the selected clip), 0.77 */
       KK_TRACKS_LEFT_KEY = 255, KK_TRACKS_RIGHT_KEY = 255, KK_CLIP_MOVE_KEY = 4,   /* 0.91: tracks via SHIFT + F / G; MOVE = low E (hold + G# / A#) */  /* top F held + top G# / A#: move the clip (0.76) */                                        /* top A / B: 8-track view (0.67) */
       KK_CLIP_DELETE_KEY = 10, KK_CLIP_OVERDUB_KEY = 0 };   /* 0.72: lowest C held + clip = OVERDUB */
/* The third black key (F#) held + clip = SELECT (select the slot, arm its
 * track); the fourth (G#) = launch the selected scene; the last two black
 * keys (top G#, A#) = scene up, scene down. */
enum { KK_CLIP_SELECT_KEY = 255, KK_SCENE_LAUNCH_KEY = 255, KK_SCENE_UP_KEY = 255, KK_SCENE_DOWN_KEY = 255 };   /* 0.91: scenes via SHIFT */

/* Knob labels as printed on the screen (E1..E7). */
#define KK_K1 "E1"
#define KK_K2 "E2"
#define KK_K3 "E3"
#define KK_K4 "E4"
#define KK_K5 "E5"
#define KK_K6 "E6"
#define KK_K7 "E7"
/* Navigation hints. */
#define KK_TXT_PATCH_BACK "PATCH: BACK"
#define KK_TXT_BT_FORGET "FORGET MIDI PAIRS? > YES  < NO"
#define KK_TXT_BT_FOOTER "E7 ACTION  > OK  < BACK  BT EXIT"
#define KK_TXT_DIAG_EXIT "< / BT EXIT"
/* Last BLE error code this SDK reports. */
#define KK_BLE_LAST_ERROR FM1_BLE_BOND_SAVE_FAILED

/* Step Mode function keys (0.58): lowest octave white keys and six white
 * keys in the middle octave; every other key plays normally. -1 = none. */
static inline int kk_step_key_function(unsigned key)
{
    switch (key) {
    /* 0.73 lowest octave: D RESOLUTION, E FOLLOW, F DOUBLE, G LENGTH, A BARS, B DRUM (0.80);
     * black: C# COPY, D# PASTE, F# / G# page, A# DELETE (hold + step / bar / key).
     * 0.99: C = NOTE LENGTH (hold + pad n = n sixteenths; on held steps too). */
    case 0: return KK_STEP_NOTE_LEN;    /* C */
    case 2: return KK_STEP_RESOLUTION;  /* D */
    case 4: return KK_STEP_FOLLOW;      /* E */
    case 5: return KK_STEP_DOUBLE;      /* F  0.80: double everything (length and notes) */
    case 7: return KK_STEP_LENGTH;      /* G  hold + pad n = n bars */
    case 9: return KK_STEP_BARS;        /* A  tap: bar view (DELETE + bar deletes it) */
    case 11: return KK_STEP_DRUM;       /* B  0.80 hold: pads = Drum Mode pads */
    case 1: return KK_STEP_COPY;        /* C# tap: copy the next step / key / bar */
    case 3: return KK_STEP_PASTE;       /* D# tap: paste onto the next one; hold: several */
    case 6: return KK_STEP_PAGE_PREV;   /* F# */
    case 8: return KK_STEP_PAGE_NEXT;   /* G# */
    case 10: return KK_STEP_CLEAR;      /* A# tap: the page; hold + step / bar / key: delete it */
    /* 0.91: the top octave is the Live strip; velocity, length and repeats
     * are hold a step + E1 / E2 / E3. */
    default: return -1;
    }
}

/* Step Mode drum keys (0.60, like Move's drum pads): the second octave,
 * C = the first drum of the bank (36 = kick). Each plays its sound and
 * selects it for the steps. -1 = not a drum key. */
static inline int kk_step_drum_key(unsigned key) { return key >= 12 && key <= 27 ? (int)key - 12 : -1; }   /* 0.91: C2..D#3 = 16 drum keys */
/* 0.86: top-octave Live keys outside Clip and Step Mode (the selected track /
 * clip in Live): -1 = a normal key, 0 = silent, else the channel 15 CC sent
 * (127 on press, 0 on release). Black from the right: A# DELETE, G# FINISH &
 * NEXT, F# PLAY / STOP; white from the right: C = SHIFT (0.88, silent),
 * B SOLO, A MUTE, G ARM, F QUANTIZE; E plays notes. With C held: G# / A# =
 * scene up / down, F / G = track left / right, F# launch scene, A duplicate
 * clip, B double loop, E fixed record length (0.91; kk_top_shift_cc). The strip
 * is the same in every mode (0.91). */
enum { KK_TOP_SHIFT_KEY = 36 };
static inline int kk_top_key_cc(unsigned key)
{
    switch (key) {
    case 34: return KK_CC_CLIP_DELETE;
    case 32: return KK_CC_FINISH;
    case 30: return KK_CC_CLIP_PLAY;
    case 36: return 0;                       /* SHIFT */
    case 35: return KK_CC_SOLO;
    case 33: return KK_CC_MUTE;
    case 31: return KK_CC_ARM;
    case 29: return KK_CC_QUANTIZE;
    default: return -1;
    }
}
static inline int kk_top_shift_cc(unsigned key)
{
    switch (key) {
    case 32: return KK_CC_SCENE_UP;
    case 34: return KK_CC_SCENE_DOWN;
    case 29: return KK_CC_TRACKS_LEFT;
    case 31: return KK_CC_TRACKS_RIGHT;
    case 30: return KK_CC_SCENE_LAUNCH;        /* 0.91 */
    case 33: return KK_CC_CLIP_DUPLICATE;
    case 35: return KK_CC_LOOP_DOUBLE;
    case 28: return KK_CC_REC_LENGTH;          /* handled on the keyboard: fixed-length cycle */
    default: return -1;
    }
}

/* 16 RGB pads (ripple lights, light shows). */
#define KK_HAS_RGB_PADS 1

/* Board event interface (scan.c) and SMK-37 hooks. */
#include "smk37_controls.h"
#endif

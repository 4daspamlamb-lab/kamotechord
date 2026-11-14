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
enum { KK_CLIP_COPY_KEY = 1, KK_CLIP_PASTE_KEY = 3, KK_CLIP_TOGGLE_KEY = 6 /* F#: PLAY (hold / tap, 0.75) */, KK_CLIP_ARRANGE_KEY = 2, /* D: Scene Arrange (0.77) */
       KK_CLIP_STOP_KEY = 8,   /* G#: STOP (hold + clips, or tap: the selected clip), 0.77 */
       KK_TRACKS_LEFT_KEY = 33, KK_TRACKS_RIGHT_KEY = 35, KK_CLIP_MOVE_KEY = 29,  /* top F held + top G# / A#: move the clip (0.76) */                                        /* top A / B: 8-track view (0.67) */
       KK_CLIP_DELETE_KEY = 10, KK_CLIP_OVERDUB_KEY = 0 };   /* 0.72: lowest C held + clip = OVERDUB */
/* The third black key (F#) held + clip = SELECT (select the slot, arm its
 * track); the fourth (G#) = launch the selected scene; the last two black
 * keys (top G#, A#) = scene up, scene down. */
enum { KK_CLIP_SELECT_KEY = 255, KK_SCENE_LAUNCH_KEY = 30, KK_SCENE_UP_KEY = 32, KK_SCENE_DOWN_KEY = 34 };

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
    /* 0.73 lowest octave: D RESOLUTION, E FOLLOW, F DRUM, G LENGTH, A BARS;
     * black: C# COPY, D# PASTE, F# / G# page, A# DELETE (hold + step / bar / key).
     * C and B play notes. */
    case 2: return KK_STEP_RESOLUTION;  /* D */
    case 4: return KK_STEP_FOLLOW;      /* E */
    case 5: return KK_STEP_DRUM;        /* F  hold: pads = Drum Mode pads */
    case 7: return KK_STEP_LENGTH;      /* G  hold + pad n = n bars */
    case 9: return KK_STEP_BARS;        /* A  tap: bar view (DELETE + bar deletes it) */
    case 11: return KK_STEP_DOUBLE;     /* B  0.77: double everything (length and notes) */
    case 1: return KK_STEP_COPY;        /* C# tap: copy the next step / key / bar */
    case 3: return KK_STEP_PASTE;       /* D# tap: paste onto the next one; hold: several */
    case 6: return KK_STEP_PAGE_PREV;   /* F# */
    case 8: return KK_STEP_PAGE_NEXT;   /* G# */
    case 10: return KK_STEP_CLEAR;      /* A# tap: the page; hold + step / bar / key: delete it */
    case 29: return KK_STEP_VEL_1;      /* 0.63: top F G A B = velocity 32 / 64 / 96 / 127 */
    case 31: return KK_STEP_VEL_2;
    case 33: return KK_STEP_VEL_3;
    case 35: return KK_STEP_VEL_4;
    case 36: return KK_STEP_NOTE_LEN;   /* top C hold: pads = note length (1-16 sixteenths of a bar) */
    case 30: return KK_STEP_REPEAT_1;   /* top F# G# A#: hold + a pad = 1 / 2 / 3 hits per step */
    case 32: return KK_STEP_REPEAT_2;
    case 34: return KK_STEP_REPEAT_3;
    default: return -1;
    }
}

/* Step Mode drum keys (0.60, like Move's drum pads): the second octave,
 * C = the first drum of the bank (36 = kick). Each plays its sound and
 * selects it for the steps. -1 = not a drum key. */
static inline int kk_step_drum_key(unsigned key) { return key >= 12 && key <= 28 ? (int)key - 12 : -1; }   /* 0.63: C2..E3 */

/* 16 RGB pads (ripple lights, light shows). */
#define KK_HAS_RGB_PADS 1

/* Board event interface (scan.c) and SMK-37 hooks. */
#include "smk37_controls.h"
#endif

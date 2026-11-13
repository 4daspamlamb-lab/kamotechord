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

/* Board event interface (scan.c) and SMK-37 hooks. */
#include "smk37_controls.h"
#endif

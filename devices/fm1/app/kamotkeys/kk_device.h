/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys device description: M-VAVE FM-1.
 * Shared code (core/) reads these values; see core/kamotkeys/kk_config.h. */
#ifndef KK_DEVICE_H
#define KK_DEVICE_H
#include <stdint.h>

#define KK_DEVICE_FM1 1
#define KK_DEVICE_NAME "FM-1"
#define KK_DEVICE_SUFFIX "-FM1"
#define KK_FIRMWARE_BASE "FM1 OPEN"

/* 27 keys (F to G, on/off only), 14 buttons, 7 encoders and an analog
 * MASTER knob. No pads, no faders: pads are keys (kk_fm1_input.c) and the
 * fader settings are knobs. */
enum { KK_KEYS = 27 };
/* Split keyboard (Normal and Key Scale Mode): keys 0..8 (F..C#, white and
 * black) are the chord keys; keys 9..26 (D..G) play. Key 19 (C) sounds
 * middle C at octave 0. */
enum { KK_SPLIT_KEY = 9, KK_KEY_BASE_NOTE = 41, KK_ANCHOR_KEY = 19, KK_ANCHOR_NOTE = 60 };
/* Pad Scale is played on the keys (0.52): the chord keys stay, and keys
 * 9..26, white and black, play consecutive scale notes (key 9 = the root,
 * C3 for root C at pad octave 0). */
#define KK_PAD_SCALE_ON_KEYS 1
/* The playing zone is one octave, so the keyboard starts at octave 0. */
enum { KK_KEY_OCTAVE_DEFAULT = 0 };
/* Keys only switch: every note uses this fixed, saved velocity (PRESETS). */
#define KK_FIXED_VELOCITY 1
enum { KK_VELOCITY_DEFAULT = 100 };

/* Clip Mode action keys (key indexes): the first black key (F#) held
 * while pressing a clip = OVERDUB, the second (G#) = DELETE. */
enum { KK_CLIP_OVERDUB_KEY = 1, KK_CLIP_DELETE_KEY = 3 };
/* The third black key (A#) held + clip = SELECT (select the slot, arm its
 * track); the last three black keys (top C#, D#, F#) = launch the selected
 * scene, scene up, scene down. */
enum { KK_CLIP_SELECT_KEY = 5, KK_SCENE_LAUNCH_KEY = 20, KK_SCENE_UP_KEY = 22, KK_SCENE_DOWN_KEY = 25 };

/* Knob labels as printed on the screen (K1-K4, SELECT, ALGORITHM, PRESETS). */
#define KK_K1 "K1"
#define KK_K2 "K2"
#define KK_K3 "K3"
#define KK_K4 "K4"
#define KK_K5 "SEL"
#define KK_K6 "ALG"
#define KK_K7 "PRE"
/* Navigation hints. */
#define KK_TXT_PATCH_BACK "HOME: BACK"
#define KK_TXT_BT_FORGET "FORGET MIDI PAIRS? SAVE YES HOME NO"
#define KK_TXT_BT_FOOTER "PRE ACTION SAVE OK HOME BACK GLO EXIT"
#define KK_TXT_DIAG_EXIT "HOME / GLO EXIT"
/* Last BLE error code this SDK reports. */
#define KK_BLE_LAST_ERROR FM1_BLE_LINK_LOST

/* FM-1 input adapter and LEDs (app/kamotkeys/kk_fm1_input.c). */
struct fm1_controls;
void kk_fm1_input(const struct fm1_controls *controls);
void kk_fm1_input_reset(void);
unsigned kk_fm1_key_led(unsigned key, uint32_t boot_ms);   /* key 0..26 */
unsigned kk_fm1_button_led(unsigned button);               /* button 0..13 */
#endif

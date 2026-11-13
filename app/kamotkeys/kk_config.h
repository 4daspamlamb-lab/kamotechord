/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys for the M-VAVE FM-1: central identity and fixed configuration.
 *
 * KAMOTKEYS_VERSION is the ONLY place the user-visible version lives. Bump it
 * (0.31, 0.32, ...) for each functional release. The FM-1 updater identity
 * (FM1_RELEASE_VERSION / FM1_UPDATE_PRODUCT_* in app/version.h) is deliberately
 * independent and must stay a valid FM1 OPEN identity so the official
 * workbench keeps recognising the device. */
#ifndef KK_CONFIG_H
#define KK_CONFIG_H

#define KAMOTKEYS_NAME "KamoteKeys"
#define KAMOTKEYS_VERSION "0.48-FM1"
#define KAMOTKEYS_TITLE KAMOTKEYS_NAME " " KAMOTKEYS_VERSION

/* MIDI channels, zero based (channel 1 = 0). */
enum {
    KK_CH_MELODIC = 0,   /* keyboard, Key Scale, Pad Scale, spare pads */
    KK_CH_DRUM = 9,      /* Drum Mode                                  */
    KK_CH_ABLETON = 14,  /* Ableton control CCs (channel 15)           */
    KK_CH_CLIP = 15      /* Clip Mode launcher and feedback (ch. 16)   */
};

/* Clip Mode protocol (channel 16), shared with remote_script/KamoteKeys.
 *  device -> host: Note 36+pad (pad 0..15) on press (vel 127) / Note Off
 *                  CC 127 = 127: "send me the full clip state" (sync)
 *  host -> device: CC 80+pad = clip state (enum below)
 *                  CC 126 = protocol version: Remote Script is connected */
enum {
    KK_CLIP_NOTE_BASE = 36, KK_CLIP_STATE_CC = 80, KK_CLIP_HELLO_CC = 126,
    KK_CLIP_SYNC_CC = 127, KK_CLIP_PROTOCOL = 1
};
enum {
    KK_CLIP_EMPTY = 0, KK_CLIP_STOPPED = 1, KK_CLIP_PLAYING = 2, KK_CLIP_QUEUED = 3,
    KK_CLIP_RECORDING = 4, KK_CLIP_QUEUED_RECORD = 5, KK_CLIP_EMPTY_ARMED = 6,
    KK_CLIP_UNKNOWN = 255
};

/* Ableton control change numbers (channel 15). */
enum {
    KK_CC_START = 20, KK_CC_STOP = 21, KK_CC_CAPTURE = 22,
    KK_CC_UNDO = 24, KK_CC_RECORD = 25, KK_CC_REDO = 27,
    /* Spare pads 5-8, 14-16 (0.43): mappable momentary buttons, CC 102-108
     * (undefined in the MIDI spec), 127 press / 0 release. */
    KK_CC_SPARE_BASE = 102
};

/* Logical KamoteKeys buttons (the SMK-37 set the performance core is written
 * for). The FM-1 has 14 physical buttons; app/kamotkeys/kk_fm1_input.c maps
 * each of them onto one of these, see docs/CONTROLS_FM1.md. */
enum {
    KK_BTN_KNOB_BANK = 0, KK_BTN_FADER_BANK = 1, KK_BTN_LEFT = 2, KK_BTN_RIGHT = 3,
    KK_BTN_OCT_DOWN = 4, KK_BTN_OCT_UP = 5,
    KK_BTN_ARP = 6, KK_BTN_NOTE_REPEAT = 7, KK_BTN_SCALE = 8, KK_BTN_CHORD = 9,
    KK_BTN_GLOBE = 10, KK_BTN_BT = 11,
    KK_BTN_PATCH = 12, KK_BTN_PARA = 13, KK_BTN_FX = 14, KK_BTN_SEQ = 15,
    KK_BTN_PLAY = 16, KK_BTN_REC = 17,
    KK_BUTTONS = 18
};

/* Fixed sizes. The FM-1 has 27 keys (F to G, no velocity), 7 encoders and
 * an analog MASTER knob; the 16 "pads" and 4 "faders" are virtual: pads are
 * keys (see kk_fm1_input.c) and the fader values are set with the knobs. */
enum { KK_KEYS = 27, KK_PADS = 16, KK_FADERS = 4, KK_ENCODERS = 8 };

/* FM-1 split keyboard (Normal and Key Scale Mode): keys 0..14 (F..G, the
 * lowest nine white keys and the six black keys between them) are the chord
 * pads and spare CC pads; keys 15..26 (G#..G) play. Key 19 is the C of the
 * playing zone and sounds middle C at octave 0. */
enum { KK_SPLIT_KEY = 15, KK_KEY_BASE_NOTE = 41, KK_ANCHOR_KEY = 19, KK_ANCHOR_NOTE = 60 };

/* FM-1 keys only switch: every key and pad plays at the fixed velocity set
 * on the main screen (PRESETS knob), default 100. */
enum { KK_VELOCITY_DEFAULT = 100 };

/* The playing zone is one octave, so the keyboard starts at octave 0 here;
 * Pad Scale/Drum pads keep their own octave starting at 0 (pad 1 = 36). */
enum { KK_OCTAVE_MIN = -4, KK_OCTAVE_MAX = 4, KK_KEY_OCTAVE_DEFAULT = 0, KK_PAD_OCTAVE_DEFAULT = 0 };

/* Colour peak used only for the LCD pad map (the FM-1 key LEDs are on/off). */
enum { KK_RGB_PEAK = 160 };

#endif

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamotKeys: central identity and fixed configuration.
 *
 * KAMOTKEYS_VERSION is the ONLY place the user-visible version lives. Bump it
 * (0.31, 0.32, ...) for each functional release. The SMK-37 updater identity
 * (FM1_RELEASE_VERSION / FM1_UPDATE_PRODUCT_* in app/version.h) is deliberately
 * independent and must stay a valid SMK37 OPEN identity so the official
 * workbench keeps recognising the device. */
#ifndef KK_CONFIG_H
#define KK_CONFIG_H

#define KAMOTKEYS_NAME "KamotKeys"
#define KAMOTKEYS_VERSION "0.45"
#define KAMOTKEYS_TITLE KAMOTKEYS_NAME " " KAMOTKEYS_VERSION

/* MIDI channels, zero based (channel 1 = 0). */
enum {
    KK_CH_MELODIC = 0,   /* keyboard, Key Scale, Pad Scale, spare pads */
    KK_CH_DRUM = 9,      /* Drum Mode                                  */
    KK_CH_ABLETON = 14,  /* Ableton control CCs (channel 15)           */
    KK_CH_CLIP = 15      /* Clip Mode launcher and feedback (ch. 16)   */
};

/* Clip Mode protocol (channel 16), shared with remote_script/KamotKeys.
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

/* Physical button indexes reported by app/board/scan.c (btnmap order).
 * Verified against the scanner's left-to-right matrix order and the SMK37
 * OPEN panel model: the left block is three rows of two, the right block is
 * two rows of six. */
enum {
    KK_BTN_KNOB_BANK = 0, KK_BTN_FADER_BANK = 1, KK_BTN_LEFT = 2, KK_BTN_RIGHT = 3,
    KK_BTN_OCT_DOWN = 4, KK_BTN_OCT_UP = 5,
    KK_BTN_ARP = 6, KK_BTN_NOTE_REPEAT = 7, KK_BTN_SCALE = 8, KK_BTN_CHORD = 9,
    KK_BTN_GLOBE = 10, KK_BTN_BT = 11,
    KK_BTN_PATCH = 12, KK_BTN_PARA = 13, KK_BTN_FX = 14, KK_BTN_SEQ = 15,
    KK_BTN_PLAY = 16, KK_BTN_REC = 17,
    KK_BUTTONS = 18
};

/* Fixed hardware sizes. */
enum { KK_KEYS = 37, KK_PADS = 16, KK_FADERS = 4, KK_ENCODERS = 8 };

/* Lowest physical key (scan id 0) is C; the second physical C is id 12. */
enum { KK_KEY_BASE_NOTE = 48, KK_ANCHOR_KEY = 12, KK_ANCHOR_NOTE = 60 };

/* Keyboard octave starts at -2 (0.31); Pad Scale/Drum pads keep their own
 * octave starting at 0 so Drum pad 1 stays MIDI 36 at power-on. */
enum { KK_OCTAVE_MIN = -4, KK_OCTAVE_MAX = 4, KK_KEY_OCTAVE_DEFAULT = -2, KK_PAD_OCTAVE_DEFAULT = 0 };

/* Pad RGB peak value for "100 % programmed brightness" before Fader 4.
 * 160: at 64 the dim idle colours had too few steps and lost their hue.
 * Channel values are always rounded to multiples of 4 (see kk_led.c). */
enum { KK_RGB_PEAK = 160 };

#endif

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys: central identity and fixed configuration, shared by every
 * device. Device-specific sizes and labels come from kk_device.h, which each
 * device provides in devices/<device>/app/kamotkeys/.
 *
 * KAMOTKEYS_VERSION is the ONLY place the user-visible version lives (one
 * version for all devices; kk_device.h adds a suffix such as "-FM1"). The
 * updater identity (FM1_RELEASE_VERSION / FM1_UPDATE_PRODUCT* in each
 * device's app/version.h) is deliberately independent and must stay valid
 * for that device's official workbench. */
#ifndef KK_CONFIG_H
#define KK_CONFIG_H

#define KAMOTKEYS_NAME "KamoteKeys"
#define KAMOTKEYS_VERSION "0.71" KK_DEVICE_SUFFIX
#define KAMOTKEYS_TITLE KAMOTKEYS_NAME " " KAMOTKEYS_VERSION

/* MIDI channels, zero based (channel 1 = 0). */
enum {
    KK_CH_MELODIC = 0,   /* keyboard, Key Scale, Pad Scale, spare pads */
    KK_CH_DRUM = 9,      /* Drum Mode                                  */
    KK_CH_ABLETON = 14,  /* Ableton control CCs (channel 15)           */
    KK_CH_STEP = 13,     /* Step Mode sequencer (ch. 14)               */
    KK_CH_STEP_SELECT = 12, /* Step Mode: key zone pitch picked (ch. 13) */
    KK_CH_CLIP = 15      /* Clip Mode launcher and feedback (ch. 16)   */
};

/* Clip Mode protocol (channel 16), shared with remote_script/KamoteKeys.
 *  device -> host: Note 36+pad (pad 0..15) on press (vel 127) / Note Off
 *                  Note 52+pad: the same with OVERDUB held (first black key,
 *                  KK_CLIP_OVERDUB_KEY): overdub / record into that clip
 *                  Note 68+pad: with DELETE held (second black key): delete it
 *                  Note 84+pad: with SELECT held (third black key): select the
 *                  slot and arm its track (Session Record then records there)
 *                  CC 127 = 127: "send me the full clip state" (sync)
 *  host -> device: CC 80+pad = clip state (enum below)
 *                  CC 126 = protocol version: Remote Script is connected
 *                  CC 102/103 = Live tempo x 10 (MSB, LSB) */
enum {
    KK_CLIP_NOTE_BASE = 36, KK_CLIP_OVERDUB_BASE = 52, KK_CLIP_DELETE_BASE = 68, KK_CLIP_SELECT_BASE = 84,
    KK_CLIP_STATE_CC = 80, KK_CLIP_HELLO_CC = 126,
    /* 0.65: COPY / PASTE keys (notes, press / release) and the clipboard the
     * script reports for the screen: CC 106 kind (KK_COPY_*, +64 = paste
     * armed, +32 = copy armed), 107 / 108 its numbers. */
    KK_CLIP_COPY_NOTE = 100, KK_CLIP_PASTE_NOTE = 101, KK_CLIP_ARRANGE_NOTE = 102,
    /* 0.66: host CC 109 = first track shown, 110 = Scene Arrange on. */
    KK_HOST_TRACKS_CC = 109, KK_HOST_ARRANGE_CC = 110,
    KK_COPY_KIND_CC = 106, KK_COPY_A_CC = 107, KK_COPY_B_CC = 108,
    KK_CLIP_SYNC_CC = 127, KK_CLIP_PROTOCOL = 1,
    /* host -> device (0.54): Live's tempo x 10 as two 7-bit CCs, MSB first;
     * applied when the LSB arrives. While received, ARP follows Live. */
    KK_HOST_TEMPO_MSB_CC = 102, KK_HOST_TEMPO_LSB_CC = 103,
    /* host -> device (0.55): first scene shown on the 8 x 2 grid (0 based). */
    KK_HOST_SCENE_CC = 104,
    /* host -> device (0.57): pad of Live's highlighted slot (0..15), 127 = none. */
    KK_HOST_HIGHLIGHT_CC = 105
};
enum {
    KK_CLIP_EMPTY = 0, KK_CLIP_STOPPED = 1, KK_CLIP_PLAYING = 2, KK_CLIP_QUEUED = 3,
    KK_CLIP_RECORDING = 4, KK_CLIP_QUEUED_RECORD = 5, KK_CLIP_EMPTY_ARMED = 6,
    KK_CLIP_ARR_ON = 7, KK_CLIP_ARR_OFF = 8, KK_CLIP_ARR_NONE = 9,   /* Scene Arrange: track on / off / no clip */
    KK_CLIP_UNKNOWN = 255
};

/* Step Mode protocol (channel 14, 0.58), shared with remote_script/KamoteKeys.
 *  device -> host: Note 36+pad = step pad 1..16 (press / release)
 *                  Note 60.. = function keys (KK_STEP_*; press / release)
 *                  CC 127 = 127 entering Step Mode (send everything), 0 leaving
 *                  and play the clip, 64 leaving quietly (PATCH held; 0.64)
 *                  Channel 13 notes = key-zone key pressed: select that pitch, or
 *                  a locked chord (0.63: all note-ons, then all note-offs)
 *  host -> device: CC 80+pad = pad colour code (KK_STEPC_*)
 *                  CC 96 page, 97 page count, 98 resolution, 99 follow,
 *                  100 status, 101 drum view, 102 active pitch,
 *                  103 key-zone octave (64 = default), 104 key zone = scale
 *                  keys (1) or drum keys (0), 105 playing bar (1.., 0 =
 *                  stopped), 106 clip length in bars (0.62) */
enum {
    KK_STEP_PAD_NOTE = 36,
    KK_STEP_DRUM = 60, KK_STEP_PAGE_PREV, KK_STEP_PAGE_NEXT, KK_STEP_LENGTH, KK_STEP_DOUBLE,
    KK_STEP_RESOLUTION, KK_STEP_FOLLOW, KK_STEP_COPY, KK_STEP_CLEAR,
    KK_STEP_VEL_1, KK_STEP_VEL_2, KK_STEP_VEL_3, KK_STEP_VEL_4, KK_STEP_VEL_5, KK_STEP_VEL_6,
    KK_STEP_VEL_DOWN, KK_STEP_VEL_UP, KK_STEP_PASTE,
    KK_STEP_BARS, KK_STEP_OCT_DOWN, KK_STEP_OCT_UP, KK_STEP_SCALE_KEYS,
    KK_STEP_REPEAT_1, KK_STEP_REPEAT_2, KK_STEP_REPEAT_3, KK_STEP_NOTE_LEN,
    KK_STEP_CHORD_TOGGLE,   /* keyboard only: chord keys <-> single notes (never sent) */
    KK_STEP_PLAY,           /* 0.67: PARA in Step Mode: create the clip if needed, play / stop it */
    KK_STEP_CODE_CC = 80, KK_STEP_PAGE_CC = 96, KK_STEP_PAGES_CC = 97, KK_STEP_RES_CC = 98,
    KK_STEP_FOLLOW_CC = 99, KK_STEP_STATUS_CC = 100, KK_STEP_DRUMVIEW_CC = 101, KK_STEP_ACTIVE_CC = 102,
    KK_STEP_KEYOCT_CC = 103, KK_STEP_KEYS_CC = 104, KK_STEP_BAR_CC = 105, KK_STEP_BARS_CC = 106,
    KK_STEP_INFO_LAST = 106,
    KK_STEP_SYNC_CC = 127
};
/* Clipboard kinds (0.65): clip (A = track, B = scene), step (A = step, B = page),
 * sound (A = note, B = page), bar (A = bar). */
enum { KK_COPY_NONE, KK_COPY_CLIP, KK_COPY_STEP, KK_COPY_SOUND, KK_COPY_BAR };
/* Pad colour codes sent by the script (rendered by kk_led.c). */
enum {
    KK_STEPC_OUTSIDE = 0, KK_STEPC_EMPTY = 1, KK_STEPC_EMPTY_BEAT = 2,
    KK_STEPC_NOTE_1 = 3, KK_STEPC_NOTE_6 = 8,      /* 6 velocity brightness levels */
    KK_STEPC_TIE = 9, KK_STEPC_PLAY_EMPTY = 10, KK_STEPC_PLAY_NOTE = 11, KK_STEPC_HELD = 12,
    KK_STEPC_GHOST_1 = 13, KK_STEPC_GHOST_2 = 14, KK_STEPC_GHOST_3 = 15,   /* other sounds on an empty step */
    KK_STEPC_RATCHET = 16,                                                /* step repeated 2x / 3x */
    KK_STEPC_BAR_IN = 17, KK_STEPC_BAR_OUT = 18, KK_STEPC_BAR_SHOWN = 19, KK_STEPC_BAR_PLAY = 24,
    KK_STEPC_LEN_ON = 25,                                                 /* note length view: inside the length */
    KK_STEPC_DRUM_EMPTY = 20, KK_STEPC_DRUM_LOADED = 21, KK_STEPC_DRUM_ACTIVE = 22, KK_STEPC_DRUM_ACTIVE_EMPTY = 23
};

/* Ableton control change numbers (channel 15). */
enum {
    KK_CC_START = 20, KK_CC_STOP = 21, KK_CC_CAPTURE = 22,
    KK_CC_UNDO = 24, KK_CC_RECORD = 25, KK_CC_REDO = 27,
    /* 0.51: PARA = Play/Pause (continue / stop), HOME + PARA = metronome. */
    KK_CC_PLAY_PAUSE = 23, KK_CC_METRONOME = 26,
    /* Clip Mode black keys (0.55/0.56): scene down / up (last two), launch the
     * selected scene (fourth from the bottom). */
    KK_CC_SCENE_DOWN = 28, KK_CC_SCENE_UP = 29, KK_CC_SCENE_LAUNCH = 30,
    KK_CC_TRACKS_LEFT = 31, KK_CC_TRACKS_RIGHT = 32,   /* 0.66: move the 8-track view */
    /* Spare pads 5-8, 14-16 (0.43): mappable momentary buttons, CC 102-108
     * (undefined in the MIDI spec), 127 press / 0 release. */
    KK_CC_SPARE_BASE = 102
};

/* Logical KamoteKeys buttons. On the SMK-37 these are the physical button
 * indexes reported by app/board/scan.c (btnmap order: left block three rows
 * of two, right block two rows of six). Other devices map their own buttons
 * onto these (FM-1: app/kamotkeys/kk_fm1_input.c). */
enum {
    KK_BTN_KNOB_BANK = 0, KK_BTN_FADER_BANK = 1, KK_BTN_LEFT = 2, KK_BTN_RIGHT = 3,
    KK_BTN_OCT_DOWN = 4, KK_BTN_OCT_UP = 5,
    KK_BTN_ARP = 6, KK_BTN_NOTE_REPEAT = 7, KK_BTN_SCALE = 8, KK_BTN_CHORD = 9,
    KK_BTN_GLOBE = 10, KK_BTN_BT = 11,
    KK_BTN_PATCH = 12, KK_BTN_PARA = 13, KK_BTN_FX = 14, KK_BTN_SEQ = 15,
    KK_BTN_PLAY = 16, KK_BTN_REC = 17,
    KK_BUTTONS = 18
};

/* Logical sizes. KK_KEYS, the key-to-note anchor and the default keyboard
 * octave are device values (kk_device.h). Pads and faders may be virtual
 * (FM-1: pads are keys, fader values are set with knobs). */
enum { KK_PADS = 16, KK_FADERS = 4, KK_ENCODERS = 8 };
/* Pad Scale/Drum pads keep their own octave starting at 0 so Drum pad 1 is
 * MIDI 36 at power-on. */
enum { KK_OCTAVE_MIN = -4, KK_OCTAVE_MAX = 4, KK_PAD_OCTAVE_DEFAULT = 0 };

/* Pad RGB peak value for "100 % programmed brightness" before Fader 4 (the
 * LCD pad map uses the same colours on every device).
 * 160: at 64 the dim idle colours had too few steps and lost their hue.
 * Channel values are always rounded to multiples of 4 (see kk_led.c). */
enum { KK_RGB_PEAK = 160 };

/* Device sizes, labels and key maps (devices/<device>/app/kamotkeys/). Last,
 * so the device header can use the enums above. */
#include "kk_device.h"

#endif

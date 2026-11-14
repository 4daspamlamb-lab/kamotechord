/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys performance core: modes, physical sources (keys/pads), chord
 * pads, ARP, Note Repeat, faders, buttons and saved preferences. */
#ifndef KK_PERFORM_H
#define KK_PERFORM_H
#include <stdint.h>
#include "kk_config.h"
#include "kk_theory.h"

enum { KK_MODE_NORMAL, KK_MODE_KEY_SCALE, KK_MODE_PAD_SCALE, KK_MODE_DRUM, KK_MODE_CLIP, KK_MODE_STEP, KK_MODES };
enum { KK_PAGE_MAIN, KK_PAGE_BT, KK_PAGE_PATCH, KK_PAGE_ARP, KK_PAGE_REPEAT, KK_PAGE_DIAG };
enum { KK_ARP_NORMAL, KK_ARP_LATCH, KK_ARP_MODES };
enum { KK_DIR_UP, KK_DIR_DOWN, KK_DIR_UPDOWN, KK_DIR_RANDOM, KK_DIR_ORDER, KK_DIRS };
enum { KK_DIV_4, KK_DIV_8, KK_DIV_8T, KK_DIV_16, KK_DIV_16T, KK_DIV_32, KK_DIVS };
enum { KK_ANIMATIONS = 8 };

struct kk_arp_settings { uint8_t mode, direction, division, gate, swing, octaves; };

struct kk_state {
    uint8_t mode, page;
    int8_t octave;                           /* keyboard octave (starts -2)      */
    int8_t pad_octave;                       /* Pad Scale / Drum octave (starts 0) */
    uint8_t chord_lock;                      /* pad 13 LOCK active               */
    uint16_t latched_mods;                   /* chord modifiers held by LOCK     */
    uint8_t clip_pad;                        /* last Clip Mode pad pressed       */
    uint8_t clip_action;                     /* Clip Mode key held: 0 launch, 1 overdub, 2 delete, 3 select */
    uint8_t ripple;                          /* SMK-37 ripple lights (3 x SEQ REC), not saved */
    uint8_t knob_shift;                      /* FM-1 SAVE held: K1-K3 = velocity, scale, root */
    uint8_t ports_seen;                      /* ready MIDI ports at the last tick */
    uint8_t rgb_timing;                      /* pad LED timing preset (saved)  */
    uint8_t sustain_on;                      /* SUSTAIN button latch (0.45)      */
    uint8_t pad_sensitivity;                 /* pad velocity curve 1..5, 3 = linear (saved) */
    uint8_t velocity;                        /* fixed note velocity 1..127 (FM-1, saved) */
    struct kk_scale_ctx scale;
    uint16_t bpm;
    uint8_t arp_on, repeat_on, repeat_division, program;
    struct kk_arp_settings arp;
    uint8_t fader[KK_FADERS], fader_known;   /* raw 0..127, bit per fader */
    uint16_t brightness;                     /* Fader 4, 0..256            */
    uint8_t screen_dim;                      /* Fader 4 <= 10 %            */
    uint32_t buttons;                        /* physical button bitmap     */
    uint64_t keys;                           /* physical key bitmap (KK_KEYS) */
    uint16_t pads;                           /* physical pad bitmap        */
    uint16_t chord_mods;                     /* active chord-pad modifiers */
    uint8_t anim_active, anim_pattern;
    uint8_t clip_play;                       /* Clip Mode pads: 0 = select (default), 1 = play (0.65, saved) */
    uint8_t step_single;                     /* Step Mode: locked chord keys pick single notes (0.64 toggle) */
    uint32_t anim_phase;                     /* 2^32 per animation cycle   */
    uint16_t anim_speed;                     /* mHz                        */
    int16_t chord_root;                      /* root of the last generated chord, -1 */
    int16_t last_note;                       /* last direct note (display) */
    uint8_t last_pad;                        /* last pad pressed (display) */
    int16_t encoder[KK_ENCODERS];            /* pending detents for the UI */
    uint32_t pressed_at[KK_BUTTONS];
    uint32_t edit_revision;                  /* bumps on any visible change */
};
extern struct kk_state kk;

void kk_perform_init(void);
/* Control task: ARP, Note Repeat, long presses, animation clock. */
void kk_perform_tick(uint32_t now_ms);
/* Engine guard: keep track 1 DX7 / track 8 drums, engine ARPs off, tempo. */
void kk_perform_guard(void);

const char *kk_division_name(unsigned division);
const char *kk_direction_name(unsigned direction);
const char *kk_mode_name(unsigned mode);
/* Velocity after the Fader 1 override (natural = physical velocity). */
unsigned kk_velocity(unsigned natural);
unsigned kk_velocity_override(void);   /* 0 = natural, else 1..127 */
/* Pad velocity curve: 1 = harder (needs more force) .. 3 = linear .. 7 =
 * most sensitive (light hits give high velocity). Endpoints are kept. */
enum { KK_PAD_SENSITIVITY_MAX = 7 };
unsigned kk_pad_curve(unsigned velocity);
/* Note for a pad in the current pad mode (-1 if none) and its channel. */
int kk_pad_note(unsigned pad, unsigned *channel);
unsigned kk_pad_lit(unsigned pad);     /* held, or its note is sounding */
/* Pad Scale played on the keys with chord keys (devices with
 * KK_PAD_SCALE_ON_KEYS, i.e. the FM-1, while in Pad Scale Mode). */
unsigned kk_pad_scale_on_keys(void);

/* UI/menu actions (also used by host tests). */
void kk_set_mode(unsigned mode);
void kk_set_arp(unsigned on);
void kk_set_repeat(unsigned on);
void kk_set_octave(int octave);           /* octave of the active mode (keys or pads) */
int kk_active_octave(void);
void kk_edit_bpm(int delta);                      /* ignored while Live sets the tempo */
/* Effective tempo: Ableton's while the Remote Script sends it, else kk.bpm. */
unsigned kk_tempo_x10(void);
unsigned kk_tempo_bpm(void);
void kk_edit_arp(unsigned parameter, int delta);   /* 0 mode .. 5 octaves */
void kk_edit_repeat(int delta);
void kk_edit_program(int delta);
void kk_edit_scale(int delta);                     /* knob edits (devices without faders) */
void kk_edit_root(int delta);
void kk_edit_velocity(int delta);
void kk_settings_touch(void);                      /* schedule a deferred save */

/* Physical event entry (device input code calls kk_control_event). */
#include "kk_events.h"
#endif

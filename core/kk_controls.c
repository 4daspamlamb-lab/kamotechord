/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys performance core, shared by every device (core/kk_controls.c).
 * The build copies it to app/smk37_controls.c on the SMK-37 (that file name
 * gets the SDK's size-optimised flags) and app/kk_controls.c on the FM-1.
 * Device differences are the few KK_DEVICE_* / KK_FIXED_VELOCITY blocks.
 * Control task only: every physical event, ARP
 * step, Note Repeat hit and mode change runs here, never in an interrupt. */
#include "kamotkeys/kk_perform.h"
#include "kamotkeys/kk_midi.h"
#include "kamotkeys/kk_led.h"
#include "instrument.h"
#include <string.h>

struct kk_state kk;

/* ---------------------------------------------------------------- sources */
/* One source per key (0..36) and pad (37..52). A source remembers exactly
 * the notes it started, so its release always stops those notes even if the
 * octave, scale, root or mode has changed while it was held. */
enum { ROUTE_NONE, ROUTE_DIRECT, ROUTE_ARP, ROUTE_REPEAT, ROUTE_MOD, ROUTE_LOCK, ROUTE_CC, ROUTE_CLIPMOD };
/* Clip Mode action keys (0.50): source modifiers of the OVERDUB / DELETE keys. */
enum { CLIPMOD_OVERDUB = 0x2000U, CLIPMOD_DELETE = 0x1000U, CLIPMOD_SELECT = 0x0800U };
enum { SOURCES = KK_KEYS + KK_PADS, PAD_SOURCE = KK_KEYS };
struct source {
    uint8_t held, route, channel, velocity, count, sounding;
    uint8_t notes[KK_MAX_CHORD];
    uint16_t order, modifier;
    uint64_t off_us;
};
static struct source sources[SOURCES];
static void hold_release(void);   /* SUSTAIN: release the held notes */
static uint16_t press_order;
static uint64_t clock_us;      /* monotonic microseconds from fm1_millis */
static uint32_t last_ms;

/* --------------------------------------------------------------- timing */
static const uint8_t division_den[KK_DIVS] = {1, 2, 3, 4, 6, 8}; /* per quarter */
static const char *const division_names[KK_DIVS] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const direction_names[KK_DIRS] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER"};
static const char *const mode_names[KK_MODES] = {"NORMAL", "KEY SCALE", "PAD SCALE", "DRUM", "CLIP", "STEP"};
const char *kk_division_name(unsigned d) { return division_names[d % KK_DIVS]; }
const char *kk_direction_name(unsigned d) { return direction_names[d % KK_DIRS]; }
const char *kk_mode_name(unsigned m) { return mode_names[m < KK_MODES ? m : 0]; }
/* Tempo in tenths of BPM: Ableton's (via the Remote Script) has priority. */
unsigned kk_tempo_x10(void)
{
    unsigned live = kk_midi_host_tempo_x10();
    if (live) return live;
    return (kk.bpm < 30 ? 30U : kk.bpm) * 10U;
}
unsigned kk_tempo_bpm(void) { return (kk_tempo_x10() + 5U) / 10U; }
static uint64_t step_us(unsigned division)
{
    return 600000000ULL / ((uint64_t)kk_tempo_x10() * division_den[division % KK_DIVS]);
}

/* ------------------------------------------------------------- velocity */
/* Fader 1 position in tenths of a percent across the usable travel; the
 * end-stop dead zones absorb ADC offsets at both ends. */
static unsigned fader_permille(unsigned value)
{
    enum { LOW = 2, HIGH = 125 };
    if (value <= LOW) return 0;
    if (value >= HIGH) return 1000;
    return (value - LOW) * 1000U / (HIGH - LOW);
}
unsigned kk_velocity_override(void)
{
#ifdef KK_FIXED_VELOCITY
    /* Keys without velocity (FM-1): every note uses the fixed velocity set
     * with a knob (saved, default KK_VELOCITY_DEFAULT). */
    return kk.velocity < 1 ? 1 : kk.velocity > 127 ? 127 : kk.velocity;
#endif
    if (!(kk.fader_known & 1) || kk.anim_active) return 0;
    unsigned p = fader_permille(kk.fader[0]);
    if (p <= 100) return 0;                          /* 0-10 %: natural */
    unsigned v = (p * 127U + 500U) / 1000U;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}
unsigned kk_velocity(unsigned natural)
{
    unsigned v = kk_velocity_override();
    if (!v) v = natural;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}

/* ----------------------------------------------------------- note intent */
/* Pad Scale and Drum: musical pads that ARP/Repeat and the pad octave act on. */
static unsigned in_pad_mode(void) { return kk.mode == KK_MODE_PAD_SCALE || kk.mode == KK_MODE_DRUM; }
/* Devices with KK_PAD_SCALE_ON_KEYS (FM-1, 0.52): Pad Scale is played on the
 * keys above KK_SPLIT_KEY (every key, white and black, is the next scale
 * note) and the chord pads stay chord pads, so chords work in Pad Scale. */
unsigned kk_pad_scale_on_keys(void)
{
#ifdef KK_PAD_SCALE_ON_KEYS
    return kk.mode == KK_MODE_PAD_SCALE;
#else
    return 0;
#endif
}
/* Every mode in which the pads are not chord/spare pads. */
static unsigned pads_reassigned(void) { return (in_pad_mode() && !kk_pad_scale_on_keys()) || kk.mode == KK_MODE_CLIP || kk.mode == KK_MODE_STEP; }
int kk_pad_note(unsigned pad, unsigned *channel)
{
    *channel = KK_CH_MELODIC;
    if (kk.mode == KK_MODE_PAD_SCALE) return kk_pad_scale_note(pad, kk.pad_octave, &kk.scale);
    if (kk.mode == KK_MODE_DRUM) { *channel = KK_CH_DRUM; return kk_drum_note(pad, kk.pad_octave); }
    if (kk.mode == KK_MODE_CLIP) { *channel = KK_CH_CLIP; return pad < KK_PADS ? KK_CLIP_NOTE_BASE + (int)pad : -1; }
    if (kk.mode == KK_MODE_STEP) { *channel = KK_CH_STEP; return pad < KK_PADS ? KK_STEP_PAD_NOTE + (int)pad : -1; }
    return kk_spare_pad_note(pad);
}
int kk_active_octave(void) { return in_pad_mode() ? kk.pad_octave : kk.octave; }
/* Fill a source with the notes this press should produce. */
static void intent(unsigned id, struct source *s)
{
    s->count = 0; s->channel = KK_CH_MELODIC; s->modifier = 0;
    if (id < KK_KEYS && kk.mode == KK_MODE_STEP && kk_step_key_function(id) >= 0) {
        /* Step Mode function keys: logical notes on channel 14. */
        s->channel = KK_CH_STEP; s->notes[0] = (uint8_t)kk_step_key_function(id); s->count = 1;
        return;
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_STEP && kk_step_drum_key(id) >= 0) {
        /* Step Mode key zone (second octave): drum keys (0.60) play the
         * drum's note on the drum channel; with SEQ (0.62) they are scale
         * keys on channel 1. The octave is the script's, remembered per
         * track (64 = C1 drums / the keyboard octave). */
        unsigned raw = kk_midi_step_info(KK_STEP_KEYOCT_CC);
        int off = raw ? (int)raw - 64 : 0, i = kk_step_drum_key(id), note;
        if (kk_midi_step_info(KK_STEP_KEYS_CC)) {
            note = kk_key_scale_note(12U + (unsigned)i, kk.octave + off, &kk.scale);
        } else {
            note = 36 + 12 * off + i; s->channel = KK_CH_DRUM;
        }
        if (note < 0 || note > 127) return;
        /* 0.63: a locked chord turns melodic keys (scale keys, or any track
         * without a Drum Rack) into chord keys: the steps get the chord. */
        if (kk.chord_mods && !kk.step_single && (kk_midi_step_info(KK_STEP_KEYS_CC) || kk_midi_step_info(KK_STEP_STATUS_CC) == 1)) {
            s->channel = KK_CH_MELODIC;
            s->count = (uint8_t)kk_chord_build(note, kk.chord_mods, &kk.scale, s->notes);
        } else { s->notes[0] = (uint8_t)note; s->count = 1; }
        return;
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP && (id == KK_CLIP_COPY_KEY || id == KK_CLIP_PASTE_KEY || id == KK_CLIP_ARRANGE_KEY)) {
        /* 0.65: COPY / PASTE (0.66: Scene Arrange) go to the script as notes. */
        s->channel = KK_CH_CLIP; s->count = 1;
        s->notes[0] = (uint8_t)(id == KK_CLIP_COPY_KEY ? KK_CLIP_COPY_NOTE : id == KK_CLIP_PASTE_KEY ? KK_CLIP_PASTE_NOTE : KK_CLIP_ARRANGE_NOTE);
        return;
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP && id == KK_CLIP_TOGGLE_KEY) return;   /* no note */
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP && (id == KK_TRACKS_LEFT_KEY || id == KK_TRACKS_RIGHT_KEY)) {
        s->modifier = 0x4000U; s->channel = KK_CH_ABLETON;   /* 0.66: move the 8-track view */
        s->notes[0] = (uint8_t)(id == KK_TRACKS_LEFT_KEY ? KK_CC_TRACKS_LEFT : KK_CC_TRACKS_RIGHT);
        return;
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP &&
        (id == KK_CLIP_OVERDUB_KEY || id == KK_CLIP_DELETE_KEY || id == KK_CLIP_SELECT_KEY)) {
        /* Clip Mode: the first black key held = OVERDUB, the second = DELETE,
         * the third = SELECT for the clips pressed meanwhile; no note. */
        s->modifier = (uint16_t)(id == KK_CLIP_OVERDUB_KEY ? CLIPMOD_OVERDUB : id == KK_CLIP_DELETE_KEY ? CLIPMOD_DELETE : CLIPMOD_SELECT);
        return;
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP &&
        (id == KK_SCENE_LAUNCH_KEY || id == KK_SCENE_UP_KEY || id == KK_SCENE_DOWN_KEY)) {
        /* Clip Mode scene keys (ch 15 CC): launch, up, down. */
        s->modifier = 0x4000U; s->channel = KK_CH_ABLETON;
        s->notes[0] = (uint8_t)(id == KK_SCENE_LAUNCH_KEY ? KK_CC_SCENE_LAUNCH : id == KK_SCENE_UP_KEY ? KK_CC_SCENE_UP : KK_CC_SCENE_DOWN);
        return;
    }
    if (id < KK_KEYS) {
        int note;
#ifdef KK_PAD_SCALE_ON_KEYS
        if (kk_pad_scale_on_keys())
            note = id >= KK_SPLIT_KEY ? kk_pad_scale_note(id - KK_SPLIT_KEY, kk.pad_octave, &kk.scale) : -1;
        else
#endif
        note = kk.mode == KK_MODE_KEY_SCALE ? kk_key_scale_note(id, kk.octave, &kk.scale)
                                            : kk_key_note(id, kk.octave);
        unsigned mods = kk.chord_mods;   /* 0.62: a locked chord also plays in the pad modes */
        s->count = (uint8_t)kk_chord_build(note, mods, &kk.scale, s->notes);
        return;
    }
    unsigned pad = id - PAD_SOURCE, channel;
    if (!pads_reassigned() && kk_chord_pad_modifier(pad)) { s->modifier = (uint16_t)kk_chord_pad_modifier(pad); return; }
    if (!pads_reassigned() && kk_is_lock_pad(pad)) { s->modifier = 0x8000U; return; }
    if (!pads_reassigned() && kk_spare_pad_note(pad) >= 0) {
        /* Spare pads are mappable CC buttons on channel 15, not notes. */
        s->modifier = 0x4000U; s->channel = KK_CH_ABLETON;
        s->notes[0] = (uint8_t)(KK_CC_SPARE_BASE + (kk_spare_pad_note(pad) - 84));
        return;
    }
    int note = kk_pad_note(pad, &channel);
    s->channel = (uint8_t)channel;
    /* A clip pressed while DELETE / OVERDUB is held sends its action note. */
    if (channel == KK_CH_CLIP && note >= 0)
        note += kk.clip_action == 2 ? KK_CLIP_DELETE_BASE - KK_CLIP_NOTE_BASE
              : kk.clip_action == 3 ? KK_CLIP_SELECT_BASE - KK_CLIP_NOTE_BASE
              : kk.clip_action == 1 ? KK_CLIP_OVERDUB_BASE - KK_CLIP_NOTE_BASE
#ifdef KK_CLIP_PRESS_SELECT
              : !kk.clip_play ? KK_CLIP_SELECT_BASE - KK_CLIP_NOTE_BASE   /* 0.65: plain press selects by default */
#endif
              : 0;
    if (note >= 0) { s->notes[0] = (uint8_t)note; s->count = 1; }
}
static unsigned route_for(unsigned id, const struct source *s)
{
    if (s->modifier == 0x8000U) return ROUTE_LOCK;
    if (s->modifier == 0x4000U) return ROUTE_CC;
    if (s->modifier == CLIPMOD_OVERDUB || s->modifier == CLIPMOD_DELETE || s->modifier == CLIPMOD_SELECT) return ROUTE_CLIPMOD;
    if (s->modifier) return ROUTE_MOD;
    if (!s->count) return ROUTE_NONE;
    if (s->channel == KK_CH_CLIP || s->channel == KK_CH_STEP) return ROUTE_DIRECT;   /* no ARP/Repeat */
    if (kk.mode == KK_MODE_STEP && id < KK_KEYS && kk_step_drum_key(id) >= 0) return ROUTE_DIRECT;   /* drum keys */
    unsigned arp_domain = id < KK_KEYS ? !in_pad_mode() || kk_pad_scale_on_keys() : in_pad_mode() && !kk_pad_scale_on_keys();
    if (kk.arp_on && arp_domain) return ROUTE_ARP;
    if (kk.repeat_on) return ROUTE_REPEAT;
    return ROUTE_DIRECT;
}

#include "kamotkeys/kk_ctl_arp.inc"
/* ------------------------------------------------------ engage/disengage */
static void mods_refresh(void);
static void engage(unsigned id)
{
    struct source *s = &sources[id];
    s->route = (uint8_t)route_for(id, s);
    s->sounding = 0;
    if (s->route == ROUTE_CC) {
        kk_midi_cc(KK_CH_ABLETON, s->notes[0], 127);
        s->sounding = 1;
    } else if (s->route == ROUTE_DIRECT) {
        for (unsigned k = 0; k < s->count; ++k) kk_midi_note_on(s->channel, s->notes[k], kk_velocity(s->velocity));
        s->sounding = 1;
    } else if (s->route == ROUTE_REPEAT) {
        repeat_hit(s, step_us(kk.repeat_division));
    } else if (s->route == ROUTE_ARP && sustain_active()) {
        /* Sustain hold: the notes join the held pattern at once, so even a
         * tap shorter than one control tick is kept. */
        for (unsigned k = 0; k < s->count; ++k) {
            struct arp_note a = {s->channel, s->notes[k], s->velocity, s->order};
            sustained_add(&a);
        }
    } else if (s->route == ROUTE_ARP && kk.arp.mode == KK_ARP_LATCH) {
        /* A new chord after a full release replaces the latched one. */
        unsigned others = 0;
        for (unsigned i = 0; i < SOURCES; ++i) others |= i != id && sources[i].held && sources[i].route == ROUTE_ARP;
        if (!others) latched_count = 0;
    }
}
static void disengage(struct source *s)
{
    if (s->route == ROUTE_CC && s->sounding) kk_midi_cc(KK_CH_ABLETON, s->notes[0], 0);
    if ((s->route == ROUTE_DIRECT || s->route == ROUTE_REPEAT) && s->sounding)
        for (unsigned k = 0; k < s->count; ++k) kk_midi_note_off(s->channel, s->notes[k]);
    s->sounding = 0;
}
/* Re-evaluate held keys after a chord-modifier change: common tones keep
 * sounding, removed tones stop, added tones start. */
static void reconcile_keys(void)
{
    for (unsigned id = 0; id < KK_KEYS; ++id) {
        struct source *s = &sources[id];
        if (!s->held || s->route == ROUTE_NONE) continue;
        struct source next = *s;
        intent(id, &next);
        if (next.count == s->count && !memcmp(next.notes, s->notes, s->count)) continue;
        if (s->route == ROUTE_DIRECT && s->sounding) {
            for (unsigned k = 0; k < next.count; ++k) if (!memchr(s->notes, next.notes[k], s->count))
                kk_midi_note_on(next.channel, next.notes[k], kk_velocity(s->velocity));
            for (unsigned k = 0; k < s->count; ++k) if (!memchr(next.notes, s->notes[k], next.count))
                kk_midi_note_off(s->channel, s->notes[k]);
            memcpy(s->notes, next.notes, sizeof s->notes); s->count = next.count;
        } else {
            disengage(s);
            memcpy(s->notes, next.notes, sizeof s->notes); s->count = next.count;
            engage(id);
        }
        if (s->count > 1) kk.chord_root = s->notes[0];
    }
}
static void mods_refresh(void)
{
    /* LOCK: latched modifiers stay on; while locked, modifier pads toggle
     * the latched set instead of adding while held. */
    unsigned mods = kk.chord_lock ? kk.latched_mods : 0;
    for (unsigned p = 0; p < KK_PADS && !kk.chord_lock; ++p) {
        const struct source *s = &sources[PAD_SOURCE + p];
        if (s->held && s->route == ROUTE_MOD) mods |= s->modifier;
    }
    if (mods != kk.chord_mods) { kk.chord_mods = (uint16_t)mods; reconcile_keys(); ++kk.edit_revision; }
}
/* Drop held sources (they stay physically down but silent until released). */
static void consume(unsigned first, unsigned last)
{
    for (unsigned i = first; i < last; ++i) {
        struct source *s = &sources[i];
        if (!s->held) continue;
        disengage(s);
        s->route = ROUTE_NONE; s->count = 0; s->modifier = 0;
    }
}
static void reroute_all(void)
{
    arp_stop();
    for (unsigned i = 0; i < SOURCES; ++i) {
        struct source *s = &sources[i];
        if (!s->held || s->route == ROUTE_NONE || s->route == ROUTE_MOD || s->route == ROUTE_LOCK ||
            s->route == ROUTE_CC || s->route == ROUTE_CLIPMOD) continue;
        disengage(s); engage(i);
    }
}

/* ------------------------------------------------------- public actions */
void kk_settings_touch(void) { ++kk.edit_revision; groove_settings_panel_changed(); }
void kk_set_mode(unsigned mode)
{
    if (mode == kk.mode) mode = mode == KK_MODE_STEP ? kk.step_from : KK_MODE_NORMAL;   /* same button again: back */
    if (mode == KK_MODE_STEP) kk.step_from = kk.mode;   /* 0.67: leaving Step Mode returns here (e.g. Clip Mode) */
    hold_release();                                   /* sustained notes belong to the old mode */
    /* 0.62: a chord LOCK survives every mode change (it is released only
     * with the LOCK pad), and the keys keep playing the locked chord. */
    unsigned keep_lock = kk.chord_lock;
    unsigned keys_change = (mode == KK_MODE_KEY_SCALE) != (kk.mode == KK_MODE_KEY_SCALE);
#ifdef KK_PAD_SCALE_ON_KEYS
    keys_change |= (mode == KK_MODE_PAD_SCALE) != (kk.mode == KK_MODE_PAD_SCALE);   /* keys change meaning */
#endif
    keys_change |= (mode == KK_MODE_STEP) != (kk.mode == KK_MODE_STEP);   /* Step Mode function keys */
    unsigned step_was = kk.mode == KK_MODE_STEP;
    consume(PAD_SOURCE, SOURCES);                      /* pads change meaning */
    if (keys_change) consume(0, KK_KEYS);
    kk.mode = (uint8_t)mode;
    kk.chord_mods = keep_lock ? kk.latched_mods : 0;
    arp_stop();
    reroute_all();
    kk.chord_root = -1;
    if (mode == KK_MODE_CLIP) kk_midi_clip_sync_request();   /* fresh clip LEDs */
    if (mode == KK_MODE_STEP) kk_midi_step_mode(1);          /* the script binds the sequencer */
    else if (step_was) kk_midi_step_mode(kk.buttons & (1U << KK_BTN_PATCH) ? 2 : 0);   /* 0.64: leaving plays the clip */
    ++kk.edit_revision;
}
static void sustain_update(void);
void kk_set_arp(unsigned on)
{
    kk.arp_on = !!on; reroute_all(); sustain_update(); kk_settings_touch();
}
void kk_set_repeat(unsigned on) { kk.repeat_on = !!on; reroute_all(); ++kk.edit_revision; }
void kk_set_octave(int octave)
{
    if (octave < KK_OCTAVE_MIN) octave = KK_OCTAVE_MIN;
    if (octave > KK_OCTAVE_MAX) octave = KK_OCTAVE_MAX;
    if (in_pad_mode()) kk.pad_octave = (int8_t)octave; else kk.octave = (int8_t)octave;
    ++kk.edit_revision;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void kk_edit_bpm(int delta)
{
    if (kk_midi_host_tempo_x10()) return;              /* Ableton sets the tempo */
    int next = clampi((int)kk.bpm + delta, 30, 300);
    if (next != kk.bpm) { kk.bpm = (uint16_t)next; kk_settings_touch(); }
}
void kk_edit_arp(unsigned parameter, int delta)
{
    struct kk_arp_settings a = kk.arp;
    switch (parameter) {
    case 0: a.mode = (uint8_t)clampi(a.mode + delta, 0, KK_ARP_MODES - 1); break;
    case 1: a.direction = (uint8_t)clampi(a.direction + delta, 0, KK_DIRS - 1); break;
    case 2: a.division = (uint8_t)clampi(a.division + delta, 0, KK_DIVS - 1); break;
    case 3: a.gate = (uint8_t)clampi(a.gate + 5 * delta, 10, 100); break;
    case 4: a.swing = (uint8_t)clampi(a.swing + delta, 0, 50); break;
    case 5: a.octaves = (uint8_t)clampi(a.octaves + delta, 1, 4); break;
    default: return;
    }
    if (memcmp(&a, &kk.arp, sizeof a)) {
        if (a.mode != kk.arp.mode && a.mode != KK_ARP_LATCH) latched_count = 0;
        kk.arp = a; kk_settings_touch();
    }
}
void kk_edit_repeat(int delta)
{
    unsigned next = (unsigned)clampi(kk.repeat_division + delta, 0, KK_DIVS - 1);
    if (next != kk.repeat_division) { kk.repeat_division = (uint8_t)next; kk_settings_touch(); }
}
void kk_edit_program(int delta)
{
    unsigned next = (unsigned)clampi(kk.program + delta, 0, 127);
    if (next != kk.program) { kk.program = (uint8_t)next; kk_settings_touch(); kk_perform_guard(); }
}

/* ---------------------------------------------------- faders, quantising */
/* Stable selection of one of n zones over 0..127: switch only once the
 * fader is two counts past the boundary of the current zone. */
static unsigned quantise(unsigned current, unsigned value, unsigned n)
{
    unsigned zone = value * n / 128U;
    if (current >= n || zone == current) return zone;
    /* Values v with v * n / 128 == current lie in [lo, hi). */
    unsigned lo = (current * 128U + n - 1) / n, hi = ((current + 1) * 128U + n - 1) / n;
    return value + 2 <= lo || value >= hi + 1 ? zone : current;
}
static void fader(unsigned id, unsigned value)
{
    kk.fader[id] = (uint8_t)value;
    unsigned first = !(kk.fader_known & (1U << id));
    kk.fader_known |= (uint8_t)(1U << id);
    if (id == 1) {
        unsigned s = first ? value * KK_SCALES / 128U : quantise(kk.scale.scale, value, KK_SCALES);
        if (s != kk.scale.scale) { kk.scale.scale = (uint8_t)s; ++kk.edit_revision; }
    } else if (id == 2) {
        unsigned r = first ? value * 12U / 128U : quantise(kk.scale.root, value, 12);
        if (r != kk.scale.root) { kk.scale.root = (uint8_t)r; ++kk.edit_revision; }
    } else if (id == 3) {
        unsigned p = fader_permille(value);
        kk.brightness = (uint16_t)(p * 256U / 1000U);
        unsigned dim = p <= 100;
        if (dim != kk.screen_dim) { kk.screen_dim = (uint8_t)dim; ++kk.edit_revision; }
    }
    ++kk.edit_revision;
}
/* Knob edits for devices without faders (FM-1: SELECT, ALGORITHM, PRESETS). */
void kk_edit_scale(int delta)
{
    int v = (int)kk.scale.scale + delta;
    kk.scale.scale = (uint8_t)(v < 0 ? 0 : v >= KK_SCALES ? KK_SCALES - 1 : v);
    kk_settings_touch();
}
void kk_edit_root(int delta)
{
    kk.scale.root = (uint8_t)(((int)kk.scale.root + delta % 12 + 12) % 12);
    kk_settings_touch();
}
void kk_edit_velocity(int delta)
{
    int v = (int)kk.velocity + delta;
    kk.velocity = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    kk_settings_touch();
}
/* Animation speed in mHz: 0 % = half of maximum, 1-10 % very slow,
 * 50 % medium, 100 % very fast (exponential between 30 mHz and 2 Hz). */
static unsigned animation_speed(void)
{
    static const uint16_t curve[11] = {30, 45, 70, 105, 160, 245, 370, 560, 860, 1310, 2000};
    unsigned value = kk.fader_known & 1 ? kk.fader[0] : 64;
    if (value <= 1) return curve[10] / 2;
    unsigned p = fader_permille(value), i = p / 100, f = p % 100;
    if (i >= 10) return curve[10];
    return curve[i] + (curve[i + 1] - curve[i]) * f / 100U;
}

#include "kamotkeys/kk_ctl_buttons.inc"
#include "kamotkeys/kk_ctl_keys.inc"
/* --------------------------------------------------------------- query */
unsigned kk_pad_lit(unsigned pad)
{
    if (pad >= KK_PADS) return 0;
    const struct source *s = &sources[PAD_SOURCE + pad];
    if (s->held && s->route != ROUTE_NONE) return 1;
    if (!in_pad_mode() || kk_pad_scale_on_keys()) {
        /* Spare/clip pads light only for their own source (incl. Note Repeat
         * hits): a chord from the keys that reaches 84-90 must not light them. */
        if (s->sounding) return 1;
        if (pads_reassigned()) return 0;
        if (kk_chord_pad_modifier(pad)) return (kk.chord_mods & kk_chord_pad_modifier(pad)) != 0;
        if (kk_is_lock_pad(pad)) return kk.chord_lock;
        return 0;
    }
    /* Pad Scale / Drum: Move-style feedback. Any source sounding the pad's
     * note lights it: the pad itself, ARP steps, Note Repeat, other keys, and
     * notes coming back from the host (Ableton). */
    unsigned channel;
    int note = kk_pad_note(pad, &channel);
    return note >= 0 && (kk_midi_count(channel, (unsigned)note) || kk_midi_remote(channel, (unsigned)note));
}

#include "kamotkeys/kk_ctl_engine.inc"
void kk_perform_init(void)
{
    memset(&kk, 0, sizeof kk);
    memset(sources, 0, sizeof sources);
    kk.bpm = 120; kk.pad_sensitivity = 3; kk.velocity = KK_VELOCITY_DEFAULT;
#ifdef KK_DEVICE_FM1
    kk.brightness = 256; kk.fader_known = 8;   /* no Fader 4: full brightness */
#endif
    /* Octaves are never restored from flash: keys start at -2, pads at 0. */
    kk.octave = KK_KEY_OCTAVE_DEFAULT; kk.pad_octave = KK_PAD_OCTAVE_DEFAULT;
    kk.arp = (struct kk_arp_settings){KK_ARP_NORMAL, KK_DIR_UP, KK_DIV_8, 75, 0, 1};
    kk.repeat_division = KK_DIV_16;
    kk.anim_pattern = KK_ANIMATIONS - 1;  /* first SEQ REC press shows pattern 1 */
    kk.chord_root = -1; kk.last_note = -1;
    kk.scale = (struct kk_scale_ctx){0, 0};
    arp_running = latched_count = repeat_running = 0; arp_note = -1;
    hold_count = 0; sustained_count = 0;
    pedal_down = 0; sustain_sent = 0; wheel_sent[0] = 64; wheel_sent[1] = 0;
    last_ms = fm1_millis(); clock_us = 0;
    synced_program = 255; synced_bpm = 0; program_revision = synced_revision = 0;
    kk_midi_init();
}

void kk_perform_tick(uint32_t now)
{
    uint32_t elapsed = now - last_ms;
    last_ms = now;
    clock_us += (uint64_t)elapsed * 1000U;
    long_presses(now);
    /* A port coming up (USB plugged, BLE connected): ask the Remote Script for
     * the clip state. A port going away: forget stale host state. */
    unsigned ready = fm1_midi_ready_ports() & 6U;
    static uint32_t sync_at; static uint8_t sync_due;
    if (ready != kk.ports_seen) {
        /* 0.66: ask 100 ms later, once the router has finished resetting the
         * new port (a request sent during the reset can be dropped). */
        if (ready & ~kk.ports_seen) { sync_due = 1; sync_at = now; }
        if (!ready) kk_midi_remote_clear();
        kk.ports_seen = (uint8_t)ready;
    }
    if (sync_due && now - sync_at >= 100U) { sync_due = 0; kk_midi_clip_sync_request(); }
    /* Step Mode: (re)announce it whenever the Remote Script (re)connects. */
    static unsigned script_seen;
    unsigned script = kk_midi_host_protocol() != 0;
    if (script && !script_seen && kk.mode == KK_MODE_STEP) kk_midi_step_mode(1);
    script_seen = script;
    /* 0.66: while in Step Mode, say so every 2 s; the script leaves Step Mode
     * by itself when this stops (keyboard off, port changed, message lost). */
    static uint32_t step_beat;
    if (kk.mode == KK_MODE_STEP && script && now - step_beat >= 2000U) { step_beat = now; kk_midi_step_mode(1); }
    audition_tick(now);
    arp_tick();
    repeat_tick();
    if (kk.anim_active) {
        kk.anim_speed = (uint16_t)animation_speed();
        /* 2^32 per cycle: cycles = mHz * ms / 10^6, so the step is
         * mHz * ms * 2^32 / 10^6. (0.30 divided by 1000 too many: shows
         * crawled at 1/1000 of the intended speed.) */
        kk.anim_phase += (uint32_t)((uint64_t)kk.anim_speed * elapsed * 4294967296ULL / 1000000ULL);
    }
}

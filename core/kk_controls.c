/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys performance core, shared by every device (core/kk_controls.c).
 * The build copies it to app/smk37_controls.c on the SMK-37 (that file name
 * gets the SDK's size-optimised flags) and app/kk_controls.c on the FM-1.
 * Device differences are the few KK_DEVICE_* / KK_FIXED_VELOCITY blocks.
 * Control task only: every physical event, ARP
 * step, Note Repeat hit and mode change runs here, never in an interrupt. */
#include "kamotkeys/kk_perform.h"
#include "kamotkeys/kk_midi.h"
#include "instrument.h"
#include <string.h>

struct kk_state kk;

/* ---------------------------------------------------------------- sources */
/* One source per key (0..36) and pad (37..52). A source remembers exactly
 * the notes it started, so its release always stops those notes even if the
 * octave, scale, root or mode has changed while it was held. */
enum { ROUTE_NONE, ROUTE_DIRECT, ROUTE_ARP, ROUTE_REPEAT, ROUTE_MOD, ROUTE_LOCK, ROUTE_CC, ROUTE_CLIPMOD };
/* Clip Mode action keys (0.50): source modifiers of the OVERDUB / DELETE keys. */
enum { CLIPMOD_OVERDUB = 0x2000U, CLIPMOD_DELETE = 0x1000U };
enum { SOURCES = KK_KEYS + KK_PADS, PAD_SOURCE = KK_KEYS };
struct source {
    uint8_t held, route, channel, velocity, count, sounding;
    uint8_t notes[KK_MAX_CHORD];
    uint16_t order, modifier;
    uint64_t off_us;
};
static struct source sources[SOURCES];
static uint16_t press_order;
static uint64_t clock_us;      /* monotonic microseconds from fm1_millis */
static uint32_t last_ms;

/* --------------------------------------------------------------- timing */
static const uint8_t division_den[KK_DIVS] = {1, 2, 3, 4, 6, 8}; /* per quarter */
static const char *const division_names[KK_DIVS] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const direction_names[KK_DIRS] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER"};
static const char *const mode_names[KK_MODES] = {"NORMAL", "KEY SCALE", "PAD SCALE", "DRUM", "CLIP"};
const char *kk_division_name(unsigned d) { return division_names[d % KK_DIVS]; }
const char *kk_direction_name(unsigned d) { return direction_names[d % KK_DIRS]; }
const char *kk_mode_name(unsigned m) { return mode_names[m < KK_MODES ? m : 0]; }
static uint64_t step_us(unsigned division)
{
    unsigned bpm = kk.bpm < 30 ? 30 : kk.bpm;
    return 60000000ULL / ((uint64_t)bpm * division_den[division % KK_DIVS]);
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
static unsigned pads_reassigned(void) { return (in_pad_mode() && !kk_pad_scale_on_keys()) || kk.mode == KK_MODE_CLIP; }
int kk_pad_note(unsigned pad, unsigned *channel)
{
    *channel = KK_CH_MELODIC;
    if (kk.mode == KK_MODE_PAD_SCALE) return kk_pad_scale_note(pad, kk.pad_octave, &kk.scale);
    if (kk.mode == KK_MODE_DRUM) { *channel = KK_CH_DRUM; return kk_drum_note(pad, kk.pad_octave); }
    if (kk.mode == KK_MODE_CLIP) { *channel = KK_CH_CLIP; return pad < KK_PADS ? KK_CLIP_NOTE_BASE + (int)pad : -1; }
    return kk_spare_pad_note(pad);
}
int kk_active_octave(void) { return in_pad_mode() ? kk.pad_octave : kk.octave; }
/* Fill a source with the notes this press should produce. */
static void intent(unsigned id, struct source *s)
{
    s->count = 0; s->channel = KK_CH_MELODIC; s->modifier = 0;
    if (id < KK_KEYS && kk.mode == KK_MODE_CLIP && (id == KK_CLIP_OVERDUB_KEY || id == KK_CLIP_DELETE_KEY)) {
        /* Clip Mode: the first black key held = OVERDUB, the second = DELETE
         * for the clips pressed meanwhile; they play no note. */
        s->modifier = (uint16_t)(id == KK_CLIP_OVERDUB_KEY ? CLIPMOD_OVERDUB : CLIPMOD_DELETE);
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
        unsigned mods = pads_reassigned() ? 0 : kk.chord_mods;
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
              : kk.clip_action == 1 ? KK_CLIP_OVERDUB_BASE - KK_CLIP_NOTE_BASE : 0;
    if (note >= 0) { s->notes[0] = (uint8_t)note; s->count = 1; }
}
static unsigned route_for(unsigned id, const struct source *s)
{
    if (s->modifier == 0x8000U) return ROUTE_LOCK;
    if (s->modifier == 0x4000U) return ROUTE_CC;
    if (s->modifier == CLIPMOD_OVERDUB || s->modifier == CLIPMOD_DELETE) return ROUTE_CLIPMOD;
    if (s->modifier) return ROUTE_MOD;
    if (!s->count) return ROUTE_NONE;
    if (s->channel == KK_CH_CLIP) return ROUTE_DIRECT;     /* clip launches: no ARP/Repeat */
    unsigned arp_domain = id < KK_KEYS ? !in_pad_mode() || kk_pad_scale_on_keys() : in_pad_mode() && !kk_pad_scale_on_keys();
    if (kk.arp_on && arp_domain) return ROUTE_ARP;
    if (kk.repeat_on) return ROUTE_REPEAT;
    return ROUTE_DIRECT;
}

/* ------------------------------------------------------------------ ARP */
enum { POOL = 32 };
struct arp_note { uint8_t channel, note, velocity; uint16_t order; };
static struct arp_note latched[POOL];
static unsigned latched_count, arp_running, arp_step;
static uint64_t arp_next_us, arp_off_us;
static int16_t arp_note = -1; static uint8_t arp_channel;
static uint32_t arp_random = 0x2545f491U;
/* Sustain hold (0.53): while Sustain (button latch or pedal) is on with ARP,
 * every note played joins this pool and keeps arpeggiating after release,
 * like a sustain pedal on a synth's arpeggiator. Cleared when sustain ends. */
static struct arp_note sustained[POOL];
static unsigned sustained_count;
static unsigned sustain_active(void);
static void sustained_add(const struct arp_note *note)
{
    for (unsigned j = 0; j < sustained_count; ++j)
        if (sustained[j].note == note->note && sustained[j].channel == note->channel) return;
    if (sustained_count < POOL) sustained[sustained_count++] = *note;
}

static unsigned arp_collect(struct arp_note *pool)
{
    unsigned n = 0;
    for (unsigned i = 0; i < SOURCES; ++i) {
        const struct source *s = &sources[i];
        if (!s->held || s->route != ROUTE_ARP) continue;
        for (unsigned k = 0; k < s->count && n < POOL; ++k)
            pool[n++] = (struct arp_note){s->channel, s->notes[k], s->velocity, s->order};
    }
    return n;
}
static void arp_stop_note(void)
{
    if (arp_note >= 0) kk_midi_note_off(arp_channel, (unsigned)arp_note);
    arp_note = -1;
}
static void arp_stop(void) { arp_stop_note(); arp_running = 0; latched_count = 0; sustained_count = 0; }
/* Expand by octave range, then order by pitch or by press order. */
static unsigned arp_sequence(struct arp_note *pool, unsigned n)
{
    unsigned octaves = kk.arp.octaves < 1 ? 1 : kk.arp.octaves, base = n;
    for (unsigned o = 1; o < octaves; ++o)
        for (unsigned i = 0; i < base && n < POOL; ++i)
            if (pool[i].note + 12 * o <= 127) {
                pool[n] = pool[i]; pool[n].note = (uint8_t)(pool[i].note + 12 * o);
                pool[n].order = (uint16_t)(pool[i].order + o * 4096U); ++n;
            }
    for (unsigned i = 1; i < n; ++i) {      /* insertion sort, n <= 32 */
        struct arp_note x = pool[i];
        unsigned j = i;
        unsigned key = kk.arp.direction == KK_DIR_ORDER ? x.order : (unsigned)x.note * 2U + x.channel;
        while (j) {
            unsigned prev = kk.arp.direction == KK_DIR_ORDER ? pool[j - 1].order
                                                             : (unsigned)pool[j - 1].note * 2U + pool[j - 1].channel;
            if (prev <= key) break;
            pool[j] = pool[j - 1]; --j;
        }
        pool[j] = x;
    }
    /* Remove duplicates of the same channel/note (two keys of one chord). */
    unsigned m = 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned dup = 0;
        for (unsigned j = 0; j < m; ++j) if (pool[j].note == pool[i].note && pool[j].channel == pool[i].channel) dup = 1;
        if (!dup) pool[m++] = pool[i];
    }
    return m;
}
static unsigned arp_index(unsigned step, unsigned n)
{
    switch (kk.arp.direction) {
    case KK_DIR_DOWN: return n - 1 - step % n;
    case KK_DIR_UPDOWN: {
        if (n < 2) return 0;
        unsigned period = 2 * n - 2, i = step % period;
        return i < n ? i : period - i;
    }
    case KK_DIR_RANDOM:
        arp_random ^= arp_random << 13; arp_random ^= arp_random >> 17; arp_random ^= arp_random << 5;
        return arp_random % n;
    default: return step % n;
    }
}
static void arp_tick(void)
{
    struct arp_note pool[POOL];
    unsigned n = arp_collect(pool);
    if (sustain_active() && kk.arp_on) {
        for (unsigned i = 0; i < n; ++i) sustained_add(&pool[i]);   /* add newly played notes */
        memcpy(pool, sustained, sustained_count * sizeof *pool); n = sustained_count;
    } else if (sustained_count) sustained_count = 0;      /* sustain released */
    if (sustain_active() && kk.arp_on) {
        /* held by sustain: skip the latch handling below */
    } else if (n) {
        if (kk.arp.mode == KK_ARP_LATCH) { memcpy(latched, pool, n * sizeof *pool); latched_count = n; }
    } else if (kk.arp.mode == KK_ARP_LATCH && latched_count && kk.arp_on) {
        memcpy(pool, latched, latched_count * sizeof *pool); n = latched_count;
    }
    if (arp_note >= 0 && clock_us >= arp_off_us) arp_stop_note();
    if (!n) { if (arp_running) { arp_stop_note(); arp_running = 0; } return; }
    if (!arp_running) { arp_running = 1; arp_step = 0; arp_next_us = clock_us; }
    if (clock_us < arp_next_us) return;
    n = arp_sequence(pool, n);
    uint64_t step = step_us(kk.arp.division);
    /* Swing lengthens even steps and shortens odd ones; a pair stays 2 steps. */
    uint64_t swing = step * (kk.arp.swing > 50 ? 50 : kk.arp.swing) / 100U;
    uint64_t length = arp_step & 1 ? step - swing : step + swing;
    const struct arp_note *next = &pool[arp_index(arp_step, n)];
    arp_stop_note();
    kk_midi_note_on(next->channel, next->note, kk_velocity(next->velocity));
    arp_note = next->note; arp_channel = next->channel;
    unsigned gate = kk.arp.gate < 10 ? 10 : kk.arp.gate > 100 ? 100 : kk.arp.gate;
    uint64_t hold = length * gate / 100U;
    if (gate >= 100 && hold > 2000) hold -= 1000;     /* legato, never overlap */
    if (hold < 4000) hold = 4000;
    arp_off_us = clock_us + hold;
    /* Keep the grid even if a poll was late; never schedule in the past. */
    arp_next_us += length;
    if (arp_next_us <= clock_us) arp_next_us = clock_us + 1;
    ++arp_step;
}

/* ---------------------------------------------------------- Note Repeat */
static unsigned repeat_running;
static uint64_t repeat_next_us;
static void repeat_hit(struct source *s, uint64_t length)
{
    if (s->sounding) for (unsigned k = 0; k < s->count; ++k) kk_midi_note_off(s->channel, s->notes[k]);
    for (unsigned k = 0; k < s->count; ++k) kk_midi_note_on(s->channel, s->notes[k], kk_velocity(s->velocity));
    s->sounding = 1; s->off_us = clock_us + length / 2U;
}
static void repeat_tick(void)
{
    unsigned any = 0;
    uint64_t length = step_us(kk.repeat_division);
    for (unsigned i = 0; i < SOURCES; ++i) {
        struct source *s = &sources[i];
        if (s->sounding && s->route == ROUTE_REPEAT && clock_us >= s->off_us) {
            for (unsigned k = 0; k < s->count; ++k) kk_midi_note_off(s->channel, s->notes[k]);
            s->sounding = 0;
        }
        any |= s->held && s->route == ROUTE_REPEAT;
    }
    if (!any) { repeat_running = 0; return; }
    if (!repeat_running) { repeat_running = 1; repeat_next_us = clock_us + length; }
    if (clock_us < repeat_next_us) return;
    for (unsigned i = 0; i < SOURCES; ++i)
        if (sources[i].held && sources[i].route == ROUTE_REPEAT) repeat_hit(&sources[i], length);
    repeat_next_us += length;
    if (repeat_next_us <= clock_us) repeat_next_us = clock_us + length;
}

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
    if (mode == kk.mode) mode = KK_MODE_NORMAL;     /* same button again: normal */
    kk.chord_lock = 0; kk.latched_mods = 0;
    unsigned keys_change = (mode == KK_MODE_KEY_SCALE) != (kk.mode == KK_MODE_KEY_SCALE);
#ifdef KK_PAD_SCALE_ON_KEYS
    keys_change |= (mode == KK_MODE_PAD_SCALE) != (kk.mode == KK_MODE_PAD_SCALE);   /* keys change meaning */
#endif
    consume(PAD_SOURCE, SOURCES);                      /* pads change meaning */
    if (keys_change) consume(0, KK_KEYS);
    kk.mode = (uint8_t)mode;
    kk.chord_mods = 0;
    arp_stop();
    reroute_all();
    kk.chord_root = -1;
    if (mode == KK_MODE_CLIP) kk_midi_clip_sync_request();   /* fresh clip LEDs */
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

/* -------------------------------------------------------------- buttons */
static void ableton(unsigned controller, unsigned value) { kk_midi_cc(KK_CH_ABLETON, controller, value); }
static void sustain_update(void);
static void open_page(unsigned page) { kk.page = (uint8_t)page; ++kk.edit_revision; }
static uint8_t left_combo, para_cc = KK_CC_PLAY_PAUSE;
/* The Bluetooth menu borrows LEFT/RIGHT as its original HOME/SAVE keys. */
extern void kk_ui_bt_key(unsigned save);
static void button(unsigned id, unsigned down, uint32_t now)
{
    uint32_t bit = 1U << id;
    if (!!(kk.buttons & bit) == !!down) return;
    if (down) { kk.buttons |= bit; kk.pressed_at[id] = now ? now : 1; }
    else kk.buttons &= ~bit;
    ++kk.edit_revision;
    unsigned held_ms = now - kk.pressed_at[id];
    unsigned menu = kk.page == KK_PAGE_BT;
    /* A press taken by a menu also owns its release, so leaving the menu
     * on press can never emit a stray Ableton release on the way up. */
    static uint32_t menu_owned;
    if (!down && (menu_owned & bit)) { menu_owned &= ~bit; return; }
    switch (id) {
    /* 0.51: PARA = Ableton Play/Pause; with HOME (LEFT) held = metronome. */
    case KK_BTN_PARA:
        if (down) { para_cc = (kk.buttons & (1U << KK_BTN_LEFT)) ? KK_CC_METRONOME : KK_CC_PLAY_PAUSE; left_combo |= para_cc == KK_CC_METRONOME; }
        ableton(para_cc, down ? 127 : 0); return;
    case KK_BTN_CHORD: if (down) kk_set_mode(KK_MODE_CLIP); return;
    case KK_BTN_PLAY: ableton(KK_CC_START, down ? 127 : 0); return;
    case KK_BTN_GLOBE: ableton(KK_CC_STOP, down ? 127 : 0); return;
    case KK_BTN_FADER_BANK: ableton(KK_CC_CAPTURE, down ? 127 : 0); return;
    /* HOME (LEFT): Undo, sent on release unless PARA was pressed while it was
     * held (HOME + PARA = metronome). */
    case KK_BTN_LEFT:
        if (menu || kk.page == KK_PAGE_DIAG) { if (down) { menu_owned |= bit; kk_ui_bt_key(0); } return; }
        if (down) left_combo = 0;
        else if (!left_combo) { ableton(KK_CC_UNDO, 127); ableton(KK_CC_UNDO, 0); }
        return;
    case KK_BTN_RIGHT:
        if (menu) { if (down) { menu_owned |= bit; kk_ui_bt_key(1); } return; }
        ableton(KK_CC_REDO, down ? 127 : 0); return;
    case KK_BTN_KNOB_BANK:   /* Session Record: momentary; the Remote Script acts on press */
        ableton(KK_CC_RECORD, down ? 127 : 0); return;
    case KK_BTN_OCT_DOWN: if (down) kk_set_octave(kk_active_octave() - 1); return;
    case KK_BTN_OCT_UP: if (down) kk_set_octave(kk_active_octave() + 1); return;
    /* 0.45: SCALE = Pad Scale Mode, SEQ = Key Scale Mode (swapped on request). */
    case KK_BTN_SCALE: if (down) kk_set_mode(KK_MODE_PAD_SCALE); return;
    case KK_BTN_SEQ: if (down) kk_set_mode(KK_MODE_KEY_SCALE); return;
    case KK_BTN_FX: if (down) kk_set_mode(KK_MODE_DRUM); return;
    case KK_BTN_ARP:
        if (!down && kk.page != KK_PAGE_ARP && held_ms < 600) kk_set_arp(!kk.arp_on);
        else if (!down && kk.page == KK_PAGE_ARP && held_ms < 600) open_page(KK_PAGE_MAIN);
        return;
    case KK_BTN_NOTE_REPEAT:   /* 0.45: SUSTAIN latch (tap on, tap off) */
        if (down) { kk.sustain_on = !kk.sustain_on; sustain_update(); }
        return;
    case KK_BTN_BT:
        if (down) open_page(kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG ? KK_PAGE_MAIN : KK_PAGE_BT);
        return;
    case KK_BTN_PATCH:
        if (down) open_page(kk.page == KK_PAGE_PATCH ? KK_PAGE_MAIN : KK_PAGE_PATCH);
        return;
    case KK_BTN_REC:
        if (down) {
            kk.anim_pattern = (uint8_t)((kk.anim_pattern + 1) % KK_ANIMATIONS);
            consume(PAD_SOURCE, SOURCES);              /* pads stop making sound */
            kk.anim_active = 1; kk.anim_phase = 0;
            mods_refresh();
        } else kk.anim_active = 0;
        return;
    }
}
/* Long presses are detected while still held. */
static void long_presses(uint32_t now)
{
    static const uint8_t ids[2] = {KK_BTN_ARP, KK_BTN_BT};
    static const uint8_t pages[2] = {KK_PAGE_ARP, KK_PAGE_DIAG};
    static const uint16_t limits[2] = {600, 1500};
    for (unsigned i = 0; i < 2; ++i) {
        unsigned id = ids[i];
        if ((kk.buttons & (1U << id)) && now - kk.pressed_at[id] >= limits[i] && kk.page != pages[i])
            open_page(pages[i]);
    }
}

/* ------------------------------------------------------- physical events */
static uint8_t pedal_down, wheel_sent[2] = {64, 0}, sustain_sent;
/* Sustain (CC 64, channel 1) is on while the SUSTAIN button latch OR a
 * sustain pedal is on; one shared state, so nothing is sent twice. */
static unsigned sustain_active(void) { return kk.sustain_on || pedal_down; }
static void sustain_update(void)
{
    /* With ARP on, Sustain holds the arpeggio instead of sending CC 64 (which
     * would smear every ARP step into the next). */
    unsigned want = sustain_active() && !kk.arp_on;
    if (want == sustain_sent) return;
    sustain_sent = (uint8_t)want;
    kk_midi_cc(KK_CH_MELODIC, 64, want ? 127 : 0);
}
unsigned kk_pad_curve(unsigned v)
{
    static const int8_t k[5] = {-50, -25, 0, 50, 100};
    unsigned s = kk.pad_sensitivity >= 1 && kk.pad_sensitivity <= 5 ? kk.pad_sensitivity : 3;
    if (v > 127) v = 127;
    int out = (int)v + k[s - 1] * (int)v * (127 - (int)v) / (127 * 100);
    return out < 1 ? 1 : out > 127 ? 127 : (unsigned)out;
}
/* Clip Mode action from the held OVERDUB / DELETE keys (DELETE wins). */
static void clip_action_refresh(void)
{
    unsigned action = 0;
    for (unsigned id = 0; id < KK_KEYS; ++id) {
        const struct source *k = &sources[id];
        if (!k->held || k->route != ROUTE_CLIPMOD) continue;
        if (k->modifier == CLIPMOD_DELETE) action = 2;
        else if (!action) action = 1;
    }
    if (action != kk.clip_action) { kk.clip_action = (uint8_t)action; ++kk.edit_revision; }
}
static void source_press(unsigned id, unsigned velocity)
{
    if (id >= PAD_SOURCE) velocity = kk_pad_curve(velocity);
    struct source *s = &sources[id];
    if (s->held) { disengage(s); }
    s->held = 1; s->velocity = (uint8_t)velocity; s->order = ++press_order;
    intent(id, s);
    if (id >= PAD_SOURCE && kk.anim_active) { s->count = 0; s->modifier = 0; s->route = ROUTE_NONE; return; }
    engage(id);
    if (s->route == ROUTE_CLIPMOD) { clip_action_refresh(); return; }
    if (s->route == ROUTE_LOCK) {
        kk.chord_lock = !kk.chord_lock;
        /* Locking keeps whatever is held now; unlocking releases the latch. */
        kk.latched_mods = kk.chord_lock ? kk.chord_mods : 0;
        mods_refresh(); ++kk.edit_revision; return;
    }
    if (s->route == ROUTE_MOD) {
        if (kk.chord_lock) {
            /* LOCK (0.42): within a row a tap SWITCHES (MIN replaces MAJ; 9
             * replaces 7); pads combine only when pressed while another pad
             * of the same row is held; tapping the active pad clears its row. */
            enum { QUALITY = KK_MOD_MAJ | KK_MOD_MIN | KK_MOD_DIM | KK_MOD_SUS4,
                   EXTENSION = KK_MOD_7 | KK_MOD_MAJ7 | KK_MOD_6 | KK_MOD_9 };
            unsigned group = (s->modifier & QUALITY) ? QUALITY : EXTENSION, held_same = 0;
            for (unsigned p = 0; p < KK_PADS; ++p) {
                const struct source *o = &sources[PAD_SOURCE + p];
                if (PAD_SOURCE + p != id && o->held && o->route == ROUTE_MOD && (o->modifier & group)) held_same = 1;
            }
            if (held_same) kk.latched_mods |= s->modifier;                        /* chord of both */
            else if ((kk.latched_mods & group) == s->modifier) kk.latched_mods &= ~group; /* off */
            else kk.latched_mods = (uint16_t)((kk.latched_mods & ~group) | s->modifier);  /* switch */
        }
        mods_refresh(); return;
    }
    if (s->channel == KK_CH_CLIP) kk.clip_pad = (uint8_t)(id - PAD_SOURCE);
    if (s->count) {
        if (id < KK_KEYS) { kk.chord_root = s->count > 1 ? s->notes[0] : -1; kk.last_note = s->notes[0]; }
        else { kk.last_pad = (uint8_t)(id - PAD_SOURCE); kk.last_note = s->notes[0]; }
    }
}
static void source_release(unsigned id)
{
    struct source *s = &sources[id];
    if (!s->held) return;
    unsigned was_mod = s->route == ROUTE_MOD || s->route == ROUTE_LOCK, was_clip = s->route == ROUTE_CLIPMOD;
    disengage(s);
    s->held = 0; s->route = ROUTE_NONE; s->count = 0; s->modifier = 0;
    if (was_mod) mods_refresh();
    if (was_clip) clip_action_refresh();
}
void kk_control_event(unsigned kind, unsigned id, int value)
{
    uint32_t now = fm1_millis();
    if (kind == KK_EV_KEY && id < KK_KEYS) {
        if (value) { kk.keys |= 1ULL << id; source_press(id, (unsigned)value); }
        else { kk.keys &= ~(1ULL << id); source_release(id); }
        ++kk.edit_revision;
    } else if (kind == KK_EV_PAD && id < KK_PADS) {
        if (value) { kk.pads |= (uint16_t)(1U << id); source_press(PAD_SOURCE + id, (unsigned)value); }
        else { kk.pads &= (uint16_t)~(1U << id); source_release(PAD_SOURCE + id); }
        ++kk.edit_revision;
    } else if (kind == KK_EV_ENCODER && id < KK_ENCODERS) {
        int sum = kk.encoder[id] + value;
        kk.encoder[id] = (int16_t)(sum > 1000 ? 1000 : sum < -1000 ? -1000 : sum);
    } else if (kind == KK_EV_FADER && id < KK_FADERS) {
        fader(id, (unsigned)value);
    } else if (kind == KK_EV_FADER && id < 6) {
        unsigned wheel = id - 4;
        if (wheel_sent[wheel] == (unsigned)value) return;
        wheel_sent[wheel] = (uint8_t)value;
        uint8_t m[3] = {(uint8_t)((wheel ? 0xb0 : 0xe0) | KK_CH_MELODIC), (uint8_t)(wheel ? 1 : 0), (uint8_t)value};
        kk_midi_message(m, 3);
    } else if (kind == KK_EV_PEDAL) {
        unsigned down = value >= 64;
        if (down == pedal_down) return;
        pedal_down = (uint8_t)down;
        sustain_update();
    } else if (kind == KK_EV_BUTTON && id < KK_BUTTONS) {
        button(id, (unsigned)!!value, now);
    }
}
void kk_controls_panic(void)
{
    /* The scanner queue overflowed: releases may have been lost. */
    for (unsigned i = 0; i < SOURCES; ++i) { disengage(&sources[i]); sources[i].held = 0; sources[i].route = ROUTE_NONE; }
    arp_stop();
    kk_midi_all_off();
    kk.keys = 0; kk.pads = 0; kk.buttons = 0; kk.chord_mods = 0; kk.anim_active = 0; kk.clip_action = 0; kk.knob_shift = 0;
    pedal_down = 0; kk.sustain_on = 0; sustain_update();
    ++kk.edit_revision;
}

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
        if (kk.mode != KK_MODE_CLIP && kk_chord_pad_modifier(pad)) return (kk.chord_mods & kk_chord_pad_modifier(pad)) != 0;
        if (kk.mode != KK_MODE_CLIP && kk_is_lock_pad(pad)) return kk.chord_lock;
        return 0;
    }
    /* Pad Scale / Drum: Move-style feedback. Any source sounding the pad's
     * note lights it: the pad itself, ARP steps, Note Repeat, other keys, and
     * notes coming back from the host (Ableton). */
    unsigned channel;
    int note = kk_pad_note(pad, &channel);
    return note >= 0 && (kk_midi_count(channel, (unsigned)note) || kk_midi_remote(channel, (unsigned)note));
}

/* ----------------------------------------------------------- engine guard */
/* Engine values last pushed by the guard; reset with the rest of the state. */
static uint8_t synced_program = 255;
static uint16_t synced_bpm;
static uint32_t program_revision, synced_revision;

/* Settings layout (16 bytes, little endian), stored by app/project.c in the
 * journaled tuning sector. Octave is intentionally not stored. Bytes 10, 14
 * and 15 are device settings: SMK-37 (version 1) Repeat division, LED timing,
 * pad sensitivity; FM-1 (version 0x46) scale, root, velocity. */
#ifdef KK_DEVICE_FM1
enum { KK_SETTINGS_VERSION = 0x46, KK_SETTINGS_BYTES = 16 };
#else
enum { KK_SETTINGS_VERSION = 1, KK_SETTINGS_BYTES = 16 };
#endif
void kamotkeys_settings_read(uint8_t d[KK_SETTINGS_BYTES])
{
    memset(d, 0, KK_SETTINGS_BYTES);
    d[0] = KK_SETTINGS_VERSION; d[1] = (uint8_t)kk.bpm; d[2] = (uint8_t)(kk.bpm >> 8);
    d[3] = kk.arp_on; d[4] = kk.arp.mode; d[5] = kk.arp.direction; d[6] = kk.arp.division;
    d[7] = kk.arp.gate; d[8] = kk.arp.swing; d[9] = kk.arp.octaves;
    d[11] = kk.program; d[12] = (uint8_t)kk_midi_ports_mode(); d[13] = (uint8_t)kk_midi_local();
#ifdef KK_DEVICE_FM1
    d[10] = kk.scale.scale; d[14] = kk.scale.root; d[15] = kk.velocity;
#else
    d[10] = kk.repeat_division; d[14] = kk.rgb_timing; d[15] = kk.pad_sensitivity;
#endif
}
int kamotkeys_settings_apply(const uint8_t d[KK_SETTINGS_BYTES])
{
    unsigned bpm = d[1] | (unsigned)d[2] << 8;
    if (d[0] != KK_SETTINGS_VERSION || bpm < 30 || bpm > 300 || d[3] > 1 || d[4] >= KK_ARP_MODES ||
        d[5] >= KK_DIRS || d[6] >= KK_DIVS || d[7] < 10 || d[7] > 100 || d[8] > 50 ||
        d[9] < 1 || d[9] > 4 || d[11] > 127 || d[12] >= KK_PORTS_MODES || d[13] > 1)
        return -1;
#ifdef KK_DEVICE_FM1
    if (d[10] >= KK_SCALES || d[14] > 11 || d[15] < 1 || d[15] > 127) return -1;
#else
    if (d[10] >= KK_DIVS || d[14] >= SMK37_RGB_TIMINGS || d[15] > 5) return -1;
#endif
    kk.bpm = (uint16_t)bpm;
    kk.arp = (struct kk_arp_settings){d[4], d[5], d[6], d[7], d[8], d[9]};
    kk.program = d[11];
    kk_midi_set_ports(d[12]); kk_midi_set_local(d[13]);
#ifdef KK_DEVICE_FM1
    kk.scale.scale = d[10]; kk.scale.root = d[14]; kk.velocity = d[15];
#else
    kk.repeat_division = d[10];
    kk.rgb_timing = d[14]; smk37_rgb_timing(kk.rgb_timing);
    kk.pad_sensitivity = d[15] ? d[15] : 3;   /* records before 0.44: linear */
#endif
    if (kk.arp_on != d[3]) { kk.arp_on = d[3]; reroute_all(); }
    kk_perform_guard();
    ++kk.edit_revision;
    return 0;
}
void kk_perform_guard(void)
{
    /* KamoteKeys owns arpeggiation; the engine's per-track ARP must stay off
     * or it would double every note. */
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) if (groove_arp_enabled(t)) groove_arp_set(t, 0);
    if (groove_track_role(0) == GROOVE_TRACK_OFF) (void)groove_track_role_set(0, GROOVE_TRACK_NORMAL);
    if (groove_track_role(7) == GROOVE_TRACK_OFF) (void)groove_track_role_set(7, GROOVE_TRACK_NORMAL);
    if (groove_source_type(0) != GROOVE_DX7) (void)groove_source_set(0, GROOVE_DX7);
    /* Channel 10 must reach a drum engine, never a melodic synth. */
    if (!groove_pad_count(7)) (void)groove_source_set(7, GROOVE_808);
    if (groove_midi_input_channel(0) != KK_CH_MELODIC) (void)groove_midi_input_set(0, KK_CH_MELODIC);
    if (groove_midi_input_channel(7) != KK_CH_DRUM) (void)groove_midi_input_set(7, KK_CH_DRUM);
    for (unsigned t = 1; t < 7; ++t) {
        unsigned ch = groove_midi_input_channel(t);
        if (ch == KK_CH_MELODIC || ch == KK_CH_DRUM) (void)groove_midi_input_set(t, t);
    }
    struct groove_view view;
    groove_get_view(0, 0, &view);
    if (synced_program != kk.program || program_revision != groove_project_revision()) {
        if (view.program != kk.program) groove_control_edit(0, (int)kk.program - (int)view.program, 0, 0);
        synced_program = kk.program; program_revision = groove_project_revision();
    }
    /* The engine tempo follows the KamoteKeys BPM (clock out, synced delay).
     * Edit once per change: never accumulate deltas against a stale view. */
    if (synced_bpm != kk.bpm || synced_revision != groove_project_revision()) {
        if (view.tempo && view.tempo != kk.bpm) groove_tempo_edit((int)kk.bpm - (int)view.tempo);
        synced_bpm = kk.bpm; synced_revision = groove_project_revision();
    }
}

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
    if (ready != kk.ports_seen) {
        if (ready & ~kk.ports_seen) kk_midi_clip_sync_request();
        if (!ready) kk_midi_remote_clear();
        kk.ports_seen = (uint8_t)ready;
    }
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

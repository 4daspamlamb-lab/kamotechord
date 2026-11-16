/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * ARP and Note Repeat.
 * Part of core/kk_controls.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* ------------------------------------------------------------------ ARP */
enum { POOL = 32 };
struct arp_note { uint8_t channel, note, velocity; uint16_t order; };
static unsigned arp_running, arp_step;
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
static struct { uint8_t channel, note; } rep_on[POOL];   /* 0.93 CHORD REPEAT: the chord now sounding */
static unsigned rep_n;
static void arp_stop_note(void)
{
    if (arp_note >= 0) kk_midi_note_off(arp_channel, (unsigned)arp_note);
    arp_note = -1;
    for (unsigned i = 0; i < rep_n; ++i) kk_midi_note_off(rep_on[i].channel, rep_on[i].note);
    rep_n = 0;
}
static void arp_stop(void) { arp_stop_note(); arp_running = 0; sustained_count = 0; }
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
    /* 0.80: no separate LATCH mode any more (it replayed old notes while
     * Sustain was off and went silent while it was on); SUSTAIN holds the ARP. */
    if ((arp_note >= 0 || rep_n) && clock_us >= arp_off_us) arp_stop_note();
    if (!n) { if (arp_running) { arp_stop_note(); arp_running = 0; } return; }
    if (!arp_running) { arp_running = 1; arp_step = 0; arp_next_us = clock_us; }
    if (clock_us < arp_next_us) return;
    if (kk.arp.mode != KK_ARP_CHORD_REPEAT) n = arp_sequence(pool, n);
    uint64_t step = step_us(kk.arp.division);
    /* Swing lengthens even steps and shortens odd ones; a pair stays 2 steps. */
    uint64_t swing = step * (kk.arp.swing > 50 ? 50 : kk.arp.swing) / 100U;
    uint64_t length = arp_step & 1 ? step - swing : step + swing;
    arp_stop_note();
    if (kk.arp.mode == KK_ARP_CHORD_REPEAT) {
        /* 0.93: the whole chord re-struck on every step (no octave expansion). */
        for (unsigned i = 0; i < n && rep_n < POOL; ++i) {
            if (pool[i].note >= 128) continue;
            unsigned dup = 0;
            for (unsigned j = 0; j < rep_n; ++j) dup |= rep_on[j].note == pool[i].note && rep_on[j].channel == pool[i].channel;
            if (dup) continue;
            kk_midi_note_on(pool[i].channel, pool[i].note, kk_velocity(pool[i].velocity));
            if (!rep_n) note_ripple(SOURCES, pool[i].note, pool[i].velocity);   /* one ring per chord */
            rep_on[rep_n].channel = pool[i].channel; rep_on[rep_n].note = pool[i].note; ++rep_n;
        }
    } else {
        const struct arp_note *next = &pool[arp_index(arp_step, n)];
        kk_midi_note_on(next->channel, next->note, kk_velocity(next->velocity));
        note_ripple(SOURCES, next->note, next->velocity);
        arp_note = next->note; arp_channel = next->channel;
    }
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

/* --------------------------------------------------------------- Strum */
/* 0.93 ARP types STRUM UP / DOWN / UP-DOWN and HUMANIZE: a chord's notes
 * start one after another (kk_strum_ms apart, or at random within that for
 * HUMANIZE) and then sound while the key is held, like a strummed guitar. */
enum { STRUM_MAX = 48 };
static struct { uint8_t src, channel, note, velocity, fired; uint64_t at; } strum_q[STRUM_MAX];
static unsigned strum_n, strum_flip;
static void strum_start(unsigned src, const struct source *s)
{
    unsigned n = s->count, order[KK_MAX_CHORD];
    for (unsigned k = 0; k < n; ++k) order[k] = k;
    for (unsigned i = 1; i < n; ++i)                  /* by pitch, low -> high */
        for (unsigned j = i; j > 0 && s->notes[order[j - 1]] > s->notes[order[j]]; --j) { unsigned t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
    unsigned down = kk.arp.mode == KK_ARP_STRUM_DOWN || (kk.arp.mode == KK_ARP_STRUM_ALT && (strum_flip++ & 1U));
    uint64_t gap = (uint64_t)kk_strum_ms() * 1000U;
    for (unsigned k = 0; k < n && strum_n < STRUM_MAX; ++k) {
        unsigned idx = order[down ? n - 1 - k : k], velocity = s->velocity;
        uint64_t delay = (uint64_t)k * gap;
        if (kk.arp.mode == KK_ARP_HUMANIZE) {
            arp_random ^= arp_random << 13; arp_random ^= arp_random >> 17; arp_random ^= arp_random << 5;
            delay = k ? (arp_random % (gap + 1U)) : 0;
            int v = (int)velocity + (int)(arp_random >> 24) % 21 - 10;
            velocity = (unsigned)(v < 1 ? 1 : v > 127 ? 127 : v);
        }
        strum_q[strum_n].src = (uint8_t)src; strum_q[strum_n].channel = s->channel; strum_q[strum_n].note = s->notes[idx];
        strum_q[strum_n].velocity = (uint8_t)velocity; strum_q[strum_n].fired = 0; strum_q[strum_n].at = clock_us + delay;
        ++strum_n;
    }
}
static void strum_tick(void)
{
    for (unsigned i = 0; i < strum_n; ++i)
        if (!strum_q[i].fired && clock_us >= strum_q[i].at) {
            kk_midi_note_on(strum_q[i].channel, strum_q[i].note, kk_velocity(strum_q[i].velocity));
            note_ripple(SOURCES, strum_q[i].note, strum_q[i].velocity);
            strum_q[i].fired = 1;
        }
}
/* keep: Sustain holds the notes (unplayed ones start now); otherwise they stop. */
static void strum_end(unsigned src, unsigned keep)
{
    unsigned m = 0;
    for (unsigned i = 0; i < strum_n; ++i) {
        if (strum_q[i].src != src) { strum_q[m++] = strum_q[i]; continue; }
        if (keep && !strum_q[i].fired) kk_midi_note_on(strum_q[i].channel, strum_q[i].note, kk_velocity(strum_q[i].velocity));
        else if (!keep && strum_q[i].fired) kk_midi_note_off(strum_q[i].channel, strum_q[i].note);
    }
    strum_n = m;
}

/* ---------------------------------------------------------- Note Repeat */
static unsigned repeat_running;
static uint64_t repeat_next_us;
static void repeat_hit(struct source *s, uint64_t length)
{
    if (s->sounding) for (unsigned k = 0; k < s->count; ++k) kk_midi_note_off(s->channel, s->notes[k]);
    for (unsigned k = 0; k < s->count; ++k) kk_midi_note_on(s->channel, s->notes[k], kk_velocity(s->velocity));
    if (s->sounding || repeat_running) note_ripple((unsigned)(s - sources), s->count ? s->notes[0] : 0, s->velocity);   /* repeats, not the press */
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

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * ARP and Note Repeat.
 * Part of core/kk_controls.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
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

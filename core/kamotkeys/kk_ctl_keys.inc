/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Keys and pads: velocity curve, Sustain, press/release, kk_control_event.
 * Part of core/kk_controls.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* ------------------------------------------------------- physical events */
static uint8_t pedal_down, wheel_sent[2] = {64, 0}, sustain_sent;
/* Sustain (CC 64, channel 1) is on while the SUSTAIN button latch OR a
 * sustain pedal is on; one shared state, so nothing is sent twice. */
static unsigned sustain_active(void) { return kk.sustain_on || pedal_down; }
static void sustain_update(void)
{
    /* CC 64 comes only from a sustain pedal (the SUSTAIN button holds notes
     * itself), and not while ARP is on: there the pedal holds the arpeggio
     * (CC 64 would smear every ARP step into the next). */
    unsigned want = pedal_down && !kk.arp_on;
    if (want == sustain_sent) return;
    sustain_sent = (uint8_t)want;
    kk_midi_cc(KK_CH_MELODIC, 64, want ? 127 : 0);
}
/* Pad sensitivity (0.57): velocity curves out = 127 * (v / 127) ^ gamma,
 * gamma 1.6 1.3 1.0 0.75 0.55 0.42 0.32 for levels 1..7 (3 = linear, as
 * before; 7 = a light tap already gives a strong note). Endpoints kept. */
static const uint8_t pad_curves[7][128] = {
    {0,1,1,1,1,1,1,1,2,2,2,3,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,11,11,12,13,13,14,15,15,16,17,18,18,19,20,21,22,22,23,24,25,26,27,28,29,30,30,31,32,33,34,35,36,37,38,39,40,41,42,43,45,46,47,48,49,50,51,52,54,55,56,57,58,59,61,62,63,64,66,67,68,69,71,72,73,75,76,77,78,80,81,83,84,85,87,88,89,91,92,94,95,97,98,99,101,102,104,105,107,108,110,111,113,114,116,118,119,121,122,124,125,127},
    {0,1,1,1,1,2,2,3,3,4,5,5,6,7,7,8,9,9,10,11,11,12,13,14,15,15,16,17,18,19,19,20,21,22,23,24,25,26,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,77,78,79,80,81,82,84,85,86,87,88,89,91,92,93,94,96,97,98,99,100,102,103,104,105,107,108,109,110,112,113,114,115,117,118,119,121,122,123,124,126,127},
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127},
    {0,3,6,8,9,11,13,14,16,17,19,20,22,23,24,26,27,28,29,31,32,33,34,35,36,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,71,72,73,74,75,76,77,78,79,79,80,81,82,83,84,85,86,86,87,88,89,90,91,91,92,93,94,95,96,96,97,98,99,100,101,101,102,103,104,105,105,106,107,108,109,109,110,111,112,112,113,114,115,116,116,117,118,119,119,120,121,122,122,123,124,125,125,126,127},
    {0,9,13,16,19,21,24,26,28,30,31,33,35,36,38,39,41,42,43,45,46,47,48,50,51,52,53,54,55,56,57,58,60,61,62,63,63,64,65,66,67,68,69,70,71,72,73,74,74,75,76,77,78,79,79,80,81,82,83,83,84,85,86,86,87,88,89,89,90,91,92,92,93,94,94,95,96,96,97,98,98,99,100,101,101,102,102,103,104,104,105,106,106,107,108,108,109,110,110,111,111,112,113,113,114,114,115,116,116,117,117,118,119,119,120,120,121,121,122,123,123,124,124,125,125,126,126,127},
    {0,17,22,26,30,33,35,38,40,42,44,45,47,49,50,52,53,55,56,57,58,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,77,78,79,80,81,81,82,83,84,84,85,86,87,87,88,89,89,90,91,91,92,93,93,94,95,95,96,96,97,98,98,99,99,100,101,101,102,102,103,103,104,105,105,106,106,107,107,108,108,109,109,110,110,111,111,112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,120,120,120,121,121,122,122,123,123,124,124,124,125,125,126,126,127,127},
    {0,27,34,38,42,45,48,50,52,54,56,58,60,61,63,64,65,67,68,69,70,71,72,74,75,75,76,77,78,79,80,81,82,83,83,84,85,86,86,87,88,88,89,90,90,91,92,92,93,94,94,95,95,96,97,97,98,98,99,99,100,100,101,101,102,102,103,103,104,104,105,105,106,106,107,107,108,108,109,109,110,110,110,111,111,112,112,113,113,113,114,114,115,115,115,116,116,117,117,117,118,118,118,119,119,119,120,120,121,121,121,122,122,122,123,123,123,124,124,124,125,125,125,126,126,126,127,127}
};
unsigned kk_pad_curve(unsigned v)
{
    unsigned s = kk.pad_sensitivity >= 1 && kk.pad_sensitivity <= KK_PAD_SENSITIVITY_MAX ? kk.pad_sensitivity : 3;
    if (v > 127) v = 127;
    return v ? pad_curves[s - 1][v] : 0;
}
/* Clip Mode action from the held OVERDUB / DELETE / SELECT keys
 * (DELETE wins, then SELECT). */
static void clip_action_refresh(void)
{
    unsigned action = 0;
    for (unsigned id = 0; id < KK_KEYS; ++id) {
        const struct source *k = &sources[id];
        if (!k->held || k->route != ROUTE_CLIPMOD) continue;
        if (k->modifier == CLIPMOD_DELETE) action = 2;
        else if (k->modifier == CLIPMOD_SELECT && action != 2) action = 3;
        else if (!action) action = 1;
    }
    if (action != kk.clip_action) { kk.clip_action = (uint8_t)action; ++kk.edit_revision; }
}
/* ------------------------------------------------------------ Sustain */
/* SUSTAIN button (0.54): released notes keep sounding until the next key or
 * pad is pressed with nothing else held; notes pressed together (or added
 * while others are held) are sustained together. KamoteKeys holds the notes
 * itself (no CC 64), so the synth and the DAW see long notes. A sustain pedal
 * still sends CC 64. With ARP on the same rule latches the arpeggio. */
enum { HOLD_MAX = 32 };
static struct { uint8_t channel, note; } hold[HOLD_MAX];
static unsigned hold_count;
static void hold_release(void)
{
    for (unsigned i = 0; i < hold_count; ++i) kk_midi_note_off(hold[i].channel, hold[i].note);
    if (hold_count) ++kk.edit_revision;
    hold_count = 0;
}
static void hold_keep(const struct source *s)
{
    for (unsigned k = 0; k < s->count; ++k) {
        if (hold_count == HOLD_MAX) { kk_midi_note_off(s->channel, s->notes[k]); continue; }
        hold[hold_count].channel = s->channel; hold[hold_count].note = s->notes[k]; ++hold_count;
    }
}
static unsigned musical(unsigned route) { return route == ROUTE_DIRECT || route == ROUTE_ARP || route == ROUTE_REPEAT; }
/* A musical press with no other musical source held starts a new phrase. */
static unsigned new_phrase(unsigned id)
{
    for (unsigned i = 0; i < SOURCES; ++i)
        if (i != id && sources[i].held && musical(sources[i].route)) return 0;
    return 1;
}
static void source_press(unsigned id, unsigned velocity)
{
    if (id >= PAD_SOURCE) velocity = kk_pad_curve(velocity);
    struct source *s = &sources[id];
    if (s->held) { disengage(s); }
    s->held = 1; s->velocity = (uint8_t)velocity; s->order = ++press_order;
    intent(id, s);
    if (id >= PAD_SOURCE && kk.anim_active) { s->count = 0; s->modifier = 0; s->route = ROUTE_NONE; return; }
    if (kk.sustain_on && musical(route_for(id, s)) && new_phrase(id)) {
        hold_release();                                  /* the next key ends the held notes */
        sustained_count = 0;                             /* and a held arpeggio */
    }
    if (id < KK_KEYS && kk.mode == KK_MODE_STEP && kk_step_drum_key(id) >= 0)
        kk_midi_cc(KK_CH_STEP, KK_STEP_SELECT_CC, (unsigned)kk_step_drum_key(id));   /* select it for the steps */
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
    if (kk.sustain_on && s->route == ROUTE_DIRECT && s->sounding && s->channel != KK_CH_CLIP && s->channel != KK_CH_STEP) {
        hold_keep(s); s->sounding = 0;                   /* keep sounding until the next key */
    }
    disengage(s);
    s->held = 0; s->route = ROUTE_NONE; s->count = 0; s->modifier = 0;
    if (was_mod) mods_refresh();
    if (was_clip) clip_action_refresh();
}
void kk_control_event(unsigned kind, unsigned id, int value)
{
    uint32_t now = fm1_millis();
    if (kind == KK_EV_KEY && id < KK_KEYS) {
#ifdef KK_HAS_RGB_PADS
        if (value && kk.ripple) kk_led_ripple(0, id, (unsigned)value);
#endif
        if (value) { kk.keys |= 1ULL << id; source_press(id, (unsigned)value); }
        else { kk.keys &= ~(1ULL << id); source_release(id); }
        ++kk.edit_revision;
    } else if (kind == KK_EV_PAD && id < KK_PADS) {
#ifdef KK_HAS_RGB_PADS
        if (value && kk.ripple) kk_led_ripple(1, id, (unsigned)value);
#endif
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
    pedal_down = 0; kk.sustain_on = 0; sustain_update(); hold_count = 0;   /* all off was sent */
    ++kk.edit_revision;
}

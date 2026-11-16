/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Panel buttons, combos and long presses.
 * Part of core/kk_controls.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* -------------------------------------------------------------- buttons */
static void ableton(unsigned controller, unsigned value) { kk_midi_cc(KK_CH_ABLETON, controller, value); }
static void sustain_update(void);
static void open_page(unsigned page) { kk.page = (uint8_t)page; ++kk.edit_revision; }
static uint8_t left_combo, patch_combo, para_cc = KK_CC_PLAY_PAUSE, chord_long;
#ifndef KK_DEVICE_FM1
static uint8_t chord_used, chord_back, chord_step_from;   /* 0.80: momentary Clip Mode */
#endif
static uint8_t repeat_used;                        /* 0.93: NOTE REPEAT held and used (no Sustain toggle) */
static uint8_t track_hold_id = 255, track_hold_cc;   /* 0.86 / 0.91: a track key held: repeat */
static uint32_t track_hold_next;
static uint8_t show_latched, bank_combo;          /* 0.94: light show latched (GLOBE held 3 s); KNOB + FADER together */
static uint32_t show_cycle_ms, show_cycle_next, bank_combo_since;
static uint8_t show_combo, show_ripple_done;           /* KNOB + FADER BANK light show */
static uint32_t show_since, show_taps[3];           /* 0.77: last three light-show starts */
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
    /* PATCH (SMK37 OPEN's HOME) or LEFT held + PARA = metronome (0.54). */
    case KK_BTN_PARA:
        if (down) {
            unsigned shift = kk.buttons & ((1U << KK_BTN_LEFT) | (1U << KK_BTN_PATCH));
            para_cc = shift ? KK_CC_METRONOME : KK_CC_PLAY_PAUSE;
            if (shift) { left_combo = 1; patch_combo = 1; }
            /* 0.67: in Step Mode PARA plays / stops the clip being made (the script creates it if needed). */
            if (!shift && kk.mode == KK_MODE_STEP) { para_cc = 0; kk_midi_tap(KK_CH_STEP, KK_STEP_PLAY); return; }
        }
        if (para_cc) ableton(para_cc, down ? 127 : 0);
        return;
    /* CHORD: tap = Clip Mode; hold 0.6 s = Step Mode (0.58, long_presses). */
    case KK_BTN_CHORD:
        if (kk.buttons & (1U << KK_BTN_PATCH)) patch_combo = 1;    /* PATCH held: leave Step Mode quietly */
#ifdef KK_DEVICE_FM1
        if (down) chord_long = 0;
        else if (!chord_long && held_ms < 600U) kk_set_mode(KK_MODE_CLIP);
#else
        /* 0.80: Clip Mode at once on press. A tap stays in Clip Mode; held
         * (or used with a pad) it is momentary: release goes back to the mode
         * before. CHORD in Clip Mode: back to Normal, as before. */
        if (down) {
            chord_long = kk.mode != KK_MODE_CLIP;
            chord_used = 0;
            if (chord_long) {
                chord_back = kk.mode; chord_step_from = kk.step_from;
                kk_set_mode(KK_MODE_CLIP);
            }
        } else if (chord_long) {
            if (held_ms >= 400U || chord_used) {
                kk_set_mode(chord_back);
                kk.step_from = chord_step_from;          /* Step Mode still returns where it came from */
            }
        } else if (held_ms < 600U) kk_set_mode(KK_MODE_CLIP);
#endif
        return;
#ifdef KK_HAS_RGB_PADS
    /* 0.92 SMK-37: SEQ PLAY does nothing (finish & next = top G#); GLOBE is the
     * light-show (RGB) button: held = light show, each new press = next
     * pattern, three quick presses = ripple lights on / off. Stop = PARA. */
    case KK_BTN_PLAY:   /* 0.93: AUTO = step automation on / off (Step Mode: held step + knobs) */
        if (down) {
            kk.step_auto = !kk.step_auto; ++kk.edit_revision;
            kk_midi_cc(KK_CH_STEP, KK_STEP_AUTO_STATE_CC, kk.step_auto ? 127 : 0);   /* the script needs it for DELETE */
        }
        return;
    case KK_BTN_GLOBE:
        if (down) {
            show_combo = 1; show_since = now; show_ripple_done = 0;
            show_taps[0] = show_taps[1]; show_taps[1] = show_taps[2]; show_taps[2] = now ? now : 1;
            if (show_taps[0] && now - show_taps[0] <= 1200U) {
                kk.ripple = !kk.ripple; show_ripple_done = 1;
                if (kk.ripple) kk_led_ripple(2, 0, 127);       /* giant ripple from the middle */
                show_taps[0] = show_taps[1] = show_taps[2] = 0;
                kk.anim_pattern = (uint8_t)((kk.anim_pattern + KK_ANIMATIONS - 2) % KK_ANIMATIONS);   /* undo the quick taps' pattern steps */
                kk.anim_active = 0; ++kk.edit_revision;
                return;
            }
            kk.anim_pattern = (uint8_t)((kk.anim_pattern + 1) % KK_ANIMATIONS);
            consume(PAD_SOURCE, SOURCES);              /* pads stop making sound */
            kk.anim_active = 1; kk.anim_phase = 0;
            mods_refresh();
        } else { if (!show_latched) kk.anim_active = 0; show_combo = 0; }   /* 0.94: latched: keeps running */
        return;
    case KK_BTN_FADER_BANK:
    case KK_BTN_KNOB_BANK: {
        /* 0.94: with the light show latched, KNOB + FADER BANK together: a tap
         * ends it; held, the show moves to the next pattern every 10 s per
         * second held (0.5 s -> 5 s, 6 s -> 1 min). Nothing goes to Live. */
        unsigned other = id == KK_BTN_KNOB_BANK ? KK_BTN_FADER_BANK : KK_BTN_KNOB_BANK;
        if (down) {
            if (show_latched && (kk.buttons & (1U << other))) { bank_combo = 1; bank_combo_since = now; }
            return;                                    /* sent on release, as before */
        }
        if (bank_combo) {
            if (kk.buttons & (1U << other)) {          /* the first of the two let go: decide now */
                uint32_t held = now - bank_combo_since;
                if (held < 300U) { show_latched = 0; show_cycle_ms = 0; kk.anim_active = 0; kk.show_cycle_s = 0; }
                else { show_cycle_ms = held * 10U; show_cycle_next = now + show_cycle_ms; kk.show_cycle_s = (uint16_t)((show_cycle_ms + 500U) / 1000U); }
                ++kk.edit_revision;
                bank_combo = 2;
            } else bank_combo = 0;                     /* the second one: done */
            return;
        }
        unsigned cc = id == KK_BTN_KNOB_BANK ? KK_CC_RECORD : KK_CC_CAPTURE;
        if (cc == KK_CC_RECORD) ableton(KK_CC_REC_LENGTH, kk_rec_bars());   /* 0.91: fixed length (0 = off) first */
        ableton(cc, 127); ableton(cc, 0);
        return;
    }
#else
    case KK_BTN_PLAY: ableton(KK_CC_FINISH, down ? 127 : 0); return;   /* 0.77: finish the clip, next track */
    case KK_BTN_GLOBE: ableton(KK_CC_STOP, down ? 127 : 0); return;
    /* 0.59: KNOB BANK + FADER BANK held together = the light show (each new
     * combo press: next pattern; held 3 s: ripple lights on/off). Alone they
     * send Capture MIDI / Session Record on release, so a combo never
     * records. */
    case KK_BTN_FADER_BANK:
    case KK_BTN_KNOB_BANK: {
        unsigned other = id == KK_BTN_KNOB_BANK ? KK_BTN_FADER_BANK : KK_BTN_KNOB_BANK;
        if (down) {
            if (kk.buttons & (1U << other)) {          /* second of the two: light show */
                show_combo = 1; show_since = now; show_ripple_done = 0;
                show_taps[0] = show_taps[1]; show_taps[1] = show_taps[2]; show_taps[2] = now ? now : 1;
                if (show_taps[0] && now - show_taps[0] <= 1200U) {
                    /* 0.77: three quick light-show starts = ripple lights on / off. */
                    kk.ripple = !kk.ripple; show_ripple_done = 1;
#ifndef KK_DEVICE_FM1
                    if (kk.ripple) kk_led_ripple(2, 0, 127);   /* 0.80: giant ripple from the middle */
#endif
                    show_taps[0] = show_taps[1] = show_taps[2] = 0;
                    kk.anim_pattern = (uint8_t)((kk.anim_pattern + KK_ANIMATIONS - 2) % KK_ANIMATIONS);   /* undo the quick taps' pattern steps */
                    kk.anim_active = 0; ++kk.edit_revision;
                    return;
                }
                kk.anim_pattern = (uint8_t)((kk.anim_pattern + 1) % KK_ANIMATIONS);
                consume(PAD_SOURCE, SOURCES);          /* pads stop making sound */
                kk.anim_active = 1; kk.anim_phase = 0;
                mods_refresh();
            }
            return;
        }
        if (show_combo) {                              /* part of the combo: no Ableton message */
            kk.anim_active = 0;
            if (!(kk.buttons & (1U << other))) show_combo = 0;
            return;
        }
        unsigned cc = id == KK_BTN_KNOB_BANK ? KK_CC_RECORD : KK_CC_CAPTURE;
        if (cc == KK_CC_RECORD) ableton(KK_CC_REC_LENGTH, kk_rec_bars());   /* 0.91: fixed length (0 = off) first */
        ableton(cc, 127); ableton(cc, 0);
        return;
    }
#endif
    /* HOME (LEFT): Undo, sent on release unless PARA was pressed while it was
     * held (HOME + PARA = metronome). */
    case KK_BTN_LEFT:
        if (menu || kk.page == KK_PAGE_DIAG) { if (down) { menu_owned |= bit; kk_ui_bt_key(0); } return; }
        if (down) left_combo = 0;
        /* 0.69: in Step Mode ◀ / ▶ turn the page (◀ + PARA stays the metronome). */
        if (down && kk.mode == KK_MODE_STEP) { left_combo = 1; kk_midi_tap(KK_CH_STEP, KK_STEP_PAGE_PREV); return; }
        else if (!down && !left_combo) { ableton(KK_CC_UNDO, 127); ableton(KK_CC_UNDO, 0); }
        return;
    case KK_BTN_RIGHT:
        if (menu) { if (down) { menu_owned |= bit; kk_ui_bt_key(1); } return; }
        if (kk.mode == KK_MODE_STEP) { if (down) kk_midi_tap(KK_CH_STEP, KK_STEP_PAGE_NEXT); return; }
        ableton(KK_CC_REDO, down ? 127 : 0); return;
    case KK_BTN_OCT_DOWN: case KK_BTN_OCT_UP:
        if (!down) return;
        /* Step Mode: the key zone's octave, kept by the script per track (0.62). */
        if (kk.mode == KK_MODE_STEP) { kk_midi_tap(KK_CH_STEP, id == KK_BTN_OCT_UP ? KK_STEP_OCT_UP : KK_STEP_OCT_DOWN); return; }
        kk_set_octave(kk_active_octave() + (id == KK_BTN_OCT_UP ? 1 : -1)); return;
    /* 0.45: SCALE = Pad Scale Mode, SEQ = Key Scale Mode (swapped on request). */
    case KK_BTN_SCALE: if (down) kk_set_mode(KK_MODE_PAD_SCALE); return;
    case KK_BTN_SEQ:
        /* Step Mode + SEQ (0.62): the key zone becomes scale keys (per track). */
        if (down && kk.mode == KK_MODE_STEP) { kk_midi_tap(KK_CH_STEP, KK_STEP_SCALE_KEYS); return; }
        if (down) kk_set_mode(KK_MODE_KEY_SCALE);
        return;
    case KK_BTN_FX: if (down) kk_set_mode(KK_MODE_DRUM); return;
    case KK_BTN_ARP:
        /* 0.82: tap = ARP on / off; hold = ARP page. Let go without turning a
         * knob and the page stays open (tap ARP to close it); turn a knob
         * while holding and the page closes on release. */
        if (down) kk.arp_page_used = 0;
        else if (kk.page != KK_PAGE_ARP && held_ms < 600) kk_set_arp(!kk.arp_on);
        else if (kk.page == KK_PAGE_ARP && (held_ms < 600 || kk.arp_page_used)) open_page(KK_PAGE_MAIN);
        return;
    case KK_BTN_NOTE_REPEAT:
        /* 0.93: tap = SUSTAIN on / off (as before, now on release); hold =
         * real Note Repeat: pads / keys pressed meanwhile repeat at the rate
         * (E1 while holding), pad pressure sets the loudness. */
        if (down) { kk.repeat_on = 1; repeat_used = 0; reroute_all(); ++kk.edit_revision; return; }
        kk.repeat_on = 0; reroute_all();
        if (!repeat_used && held_ms < 400U) {
            kk.sustain_on = !kk.sustain_on;
            if (!kk.sustain_on) { hold_release(); sustained_count = 0; }
        }
        ++kk.edit_revision;
        return;
    case KK_BTN_BT:
        if (down) open_page(kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG ? KK_PAGE_MAIN : KK_PAGE_BT);
        return;
    case KK_BTN_PATCH:   /* on release, unless it was used with PARA */
        if (down) patch_combo = 0;
        else if (!patch_combo) open_page(kk.page == KK_PAGE_PATCH ? KK_PAGE_MAIN : KK_PAGE_PATCH);
        return;
    case KK_BTN_REC:   /* 0.59: SEQ REC = Step Mode on / off (the light show is KNOB + FADER BANK) */
        if (kk.buttons & (1U << KK_BTN_PATCH)) patch_combo = 1;    /* PATCH held: leave Step Mode quietly */
        if (down) kk_set_mode(KK_MODE_STEP);
        return;
    }
}
/* Long presses are detected while still held. */
static void long_presses(uint32_t now)
{
#ifdef KK_HAS_RGB_PADS
    /* 0.94: GLOBE held 3 s = an inward ring, then the light show stays on. */
    if ((kk.buttons & (1U << KK_BTN_GLOBE)) && !show_latched && now - kk.pressed_at[KK_BTN_GLOBE] >= 3000U) {
        show_latched = 1; show_cycle_ms = 0; kk.show_cycle_s = 0; kk.anim_active = 1; ++kk.edit_revision;
        kk_led_ripple(3, 0, 127);
    }
    if (show_latched && show_cycle_ms && (int32_t)(now - show_cycle_next) >= 0) {
        kk.anim_pattern = (uint8_t)((kk.anim_pattern + 1) % KK_ANIMATIONS); kk.anim_phase = 0;
        show_cycle_next = now + show_cycle_ms; ++kk.edit_revision;
    }
    kk.show_latched = show_latched;
#endif
    /* 0.86 / 0.91: holding a track key (SHIFT + F / G) moves one more track every 0.5 s. */
    if (track_hold_id < KK_KEYS) {
        if (!sources[track_hold_id].held) track_hold_id = 255;
        else if ((int32_t)(now - track_hold_next) >= 0) {
            ableton(track_hold_cc, 0); ableton(track_hold_cc, 127);
            track_hold_next = now + 500U;
        }
    }
    static const uint8_t ids[2] = {KK_BTN_ARP, KK_BTN_BT};
    static const uint8_t pages[2] = {KK_PAGE_ARP, KK_PAGE_DIAG};
    static const uint16_t limits[2] = {600, 1500};
    for (unsigned i = 0; i < 2; ++i) {
        unsigned id = ids[i];
        if ((kk.buttons & (1U << id)) && now - kk.pressed_at[id] >= limits[i] && kk.page != pages[i])
            open_page(pages[i]);
    }
#ifdef KK_DEVICE_FM1
    if ((kk.buttons & (1U << KK_BTN_CHORD)) && !chord_long && now - kk.pressed_at[KK_BTN_CHORD] >= 600U) {
        chord_long = 1; kk_set_mode(KK_MODE_STEP);      /* again from Step Mode: back to Normal */
    }
#endif
}

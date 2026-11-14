/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Main screen: piano, pad map, header, big text, status line.
 * Part of core/screen.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* --------------------------------------------------------------- piano */
static const int8_t white_of_pc[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
static int note_center(int note)    /* world pixels */
{
    int octave = note / 12, pc = note % 12;
    if (white_of_pc[pc] >= 0) return (octave * 7 + white_of_pc[pc]) * WK + WK / 2;
    return (octave * 7 + white_of_pc[pc - 1] + 1) * WK;
}
static void piano_target(uint32_t now)
{
    int target;
    if (sounding_n) {
        int lo = note_center(sounding[0]), hi = note_center(sounding[sounding_n - 1]);
        target = hi - lo <= 240 - 2 * WK ? (lo + hi) / 2 - 120 : lo - WK;
    } else return;   /* nothing sounding: stay where the last notes were */
    if (target * 16 == view_to16) return;
    view_from16 = view_x16; view_to16 = target * 16; view_started = now;
}
static void piano_animate(uint32_t now)
{
    uint32_t t = now - view_started;
    if (t >= 110U) { view_x16 = view_to16; return; }
    /* Ease-out cubic over ~110 ms; redirection restarts from here. */
    int32_t k = (int32_t)(t * 1024U / 110U), inv = 1024 - k;
    int32_t e = 1024 - (int32_t)((int64_t)inv * inv / 1024 * inv / 1024);
    view_x16 = view_from16 + (int32_t)((int64_t)(view_to16 - view_from16) * e / 1024);
}
static uint16_t note_colour(int note)
{
    if (sounding_n > 1 && chord_root_shown >= 0) return note == chord_root_shown ? CYAN : ORANGE;
    return GREEN;
}
static unsigned is_sounding(int note)
{
    for (unsigned i = 0; i < sounding_n; ++i) if (sounding[i] == note) return 1;
    return 0;
}
static void draw_piano(void)
{
    const struct widget *w = &widgets[W_PIANO];
    fill(0, w->y, 240, w->h, BG);
    fill(0, PIANO_TOP - 2, 240, 1, PANEL2);
    int offset = piano_offset;
    unsigned scale_view = kk.mode == KK_MODE_KEY_SCALE || kk_pad_scale_on_keys();
    for (int y = clip_y0; y < clip_y1; ++y) {
        if (y < PIANO_TOP || y >= PIANO_TOP + WHITE_H) continue;
        int ky = y - PIANO_TOP;
        for (int x = clip_x0; x < clip_x1; ++x) {
            int wx = x + offset;
            if (wx < 0) { dot(x, y, BG); continue; }
            int wi = wx / WK, within = wx - wi * WK;
            int octave = wi / 7, idx = wi % 7;
            static const uint8_t white_pc[7] = {0, 2, 4, 5, 7, 9, 11};
            int note = octave * 12 + white_pc[idx];
            uint16_t c = WHITE_KEY;
            /* Black keys sit centred on the boundary after C, D, F, G, A. */
            int black = -1;
            if (ky < BLACK_H) {
                if (within >= WK - BK / 2 && idx != 2 && idx != 6) black = note + 1;
                else if (within < BK - BK / 2 && idx != 0 && idx != 3) black = note - 1;
            }
            if (black >= 0) {
                c = black <= 127 && is_sounding(black) ? note_colour(black) : BLACK_KEY;
                if (c == BLACK_KEY && scale_view && black <= 127 && kk_in_scale(&kk.scale, black) && ky > BLACK_H - 7 && ky < BLACK_H - 3
                    && within != WK - BK / 2 && within != BK - BK / 2 - 1) c = FAINT;
            } else {
                if (note > 127) c = FAINT;
                else if (is_sounding(note)) c = note_colour(note);
                else if (scale_view && kk_in_scale(&kk.scale, note) && ky > WHITE_H - 9 && ky < WHITE_H - 5 &&
                         within > 4 && within < WK - 4) c = (unsigned)note % 12 == kk.scale.root ? CYAN : TEAL;
                if (within == 0) c = KEY_LINE;
                if (ky == WHITE_H - 1) c = KEY_LINE;
            }
            /* C labels along the bottom edge keep orientation while scrolling. */
            dot(x, y, c);
        }
    }
    /* Octave labels under each C ("C4"). */
    int first = offset / (7 * WK);
    for (int octave = first; octave <= first + 3; ++octave) {
        int x = octave * 7 * WK - offset;
        char label[5]; kk_note_label(label, octave * 12);
        if (x > -20 && x < 240 && octave * 12 <= 127) text(x + 3, PIANO_TOP + WHITE_H - 14, label, 1, DIM);
    }
}
/* 2 x 8 pad map for Pad Scale and Drum modes. */
static void draw_pads(void)
{
    const struct widget *w = &widgets[W_PIANO];
    fill(0, w->y, 240, w->h, BG);
    for (unsigned pad = 0; pad < KK_PADS; ++pad) {
        int x = 1 + (int)(pad % 8) * 30, y = w->y + 3 + (int)(pad / 8) * 32;
        unsigned channel;
        int note = kk_pad_note(pad, &channel);
        uint16_t base = c565(kk_led_pad_base(pad));
        unsigned lit = kk_pad_lit(pad);
        pill(x, y, 28, 30, note < 0 ? PANEL : blend565(PANEL, base, lit ? 230 : 70));
        char label[6];
        if (kk.mode == KK_MODE_STEP) {
            /* Step number on the shown page (D1..D16 while picking a drum). */
            unsigned code = kk_midi_step_code(pad), drum = kk_midi_step_info(KK_STEP_DRUMVIEW_CC);
            unsigned n = drum ? pad + 1 : kk_midi_step_info(KK_STEP_PAGE_CC) * 16U + pad + 1;
            char *q = label; if (drum) *q++ = 'D';
            decimal(q, (int)(n > 999 ? 999 : n));
            unsigned strong = lit || (code >= KK_STEPC_NOTE_1 && code <= KK_STEPC_PLAY_NOTE) || code == KK_STEPC_DRUM_ACTIVE;
            pill(x, y, 28, 30, blend565(PANEL, base, strong ? 220 : code == KK_STEPC_OUTSIDE ? 20 : 70));
            text_center(x + 14, y + 12, label, 1, strong ? BG : TEXT);
            continue;
        }
        if (kk.mode == KK_MODE_CLIP) {
            /* Track.scene, scene following the Remote Script's grid position. */
            unsigned scene = kk_midi_host_scene() + pad / 8 + 1;
            char *q = label;
            if (scene < 10) *q++ = 'T';
            *q++ = (char)('1' + pad % 8); *q++ = '.';
            decimal(q, (int)(scene > 99 ? 99 : scene));
            unsigned level, st = kk_midi_clip_state(pad);
            uint16_t c = c565(kk_led_pad_base(pad));
            (void)level;
            unsigned strong = st == KK_CLIP_PLAYING || st == KK_CLIP_RECORDING || st == KK_CLIP_QUEUED ||
                              st == KK_CLIP_QUEUED_RECORD || lit;
            pill(x, y, 28, 30, blend565(PANEL, c, strong ? 220 : st == KK_CLIP_STOPPED ? 110 : 25));
            text_center(x + 14, y + 12, label, 1, strong ? BG : TEXT);
            continue;
        }
        if (note < 0) strcpy(label, "--");
        else if (kk.mode == KK_MODE_DRUM) strcpy(label, kk_drum_label(note));
        else kk_note_label(label, note);
        text_center(x + 14, y + 12, label, 1, lit ? BG : TEXT);
    }
}

/* ----------------------------------------------------------- widgets */
static void draw_header(void)
{
    fill(0, 0, 240, 20, BG);
    fill(0, 19, 240, 1, PANEL);
    text(6, 6, KAMOTKEYS_TITLE, 1, TEAL);
    unsigned ready = fm1_midi_ready_ports();
    pill(180, 4, 25, 12, ready & (1U << FM1_MIDI_USB) ? C565(0, 70, 50) : PANEL);
    text(184, 6, "USB", 1, ready & (1U << FM1_MIDI_USB) ? GREEN : DIM);
    pill(209, 4, 25, 12, ready & (1U << FM1_MIDI_BLE) ? C565(0, 50, 80) : PANEL);
    text(213, 6, "BLE", 1, ready & (1U << FM1_MIDI_BLE) ? CYAN : DIM);
}
static uint16_t mode_colour(void)
{
    static const uint16_t colours[4] = {TEAL, CYAN, GREEN, ORANGE};
    return colours[kk.mode & 3];
}
static void draw_info(void)
{
    const struct widget *w = &widgets[W_INFO];
    fill(0, w->y, 240, w->h, BG);
    const char *name = kk_mode_name(kk.mode);
    int width = text_width(name, 1) + 12;
    pill(6, w->y + 5, width, 17, blend565(BG, mode_colour(), 70));
    text(12, w->y + 10, name, 1, mode_colour());
    char buf[12], *p = append(buf, "OCT ");
    int octave = kk_active_octave();
    if (octave > 0) *p++ = '+';
    decimal(p, octave);
    int after = text(width + 16, w->y + 10, buf, 1, octave ? AMBER : DIM);
    if (kk_midi_host_protocol()) { pill(after + 4, w->y + 7, 30, 13, C565(0, 60, 30)); text(after + 8, w->y + 10, "LIVE", 1, GREEN); }
    decimal(buf, kk_tempo_bpm());                      /* Live's tempo has priority */
    text_right(206, w->y + 6, buf, 2, kk_midi_host_tempo_x10() ? GREEN : TEXT);
    text(210, w->y + 13, "BPM", 1, DIM);
}
/* 0.65: the script's clipboard, e.g. "COPIED T3 S2", "PASTE: PRESS A PAD (BAR 2)". */
static void copy_line(int y)
{
    unsigned a, b, kind = kk_midi_copy(&a, &b), what = kind & 31U;
    char buf[40], *p = buf;
    if (kind & 64U) p = append(p, "PASTE: PRESS A TARGET  ");
    else if (kind & 32U) { text_center(120, y, "COPY: PRESS WHAT TO COPY", 1, AMBER); return; }
    else if (!what) return;
    else p = append(p, "COPIED ");
    switch (what) {
    case KK_COPY_CLIP: p = append(p, "T"); p = decimal(p, (int)a); p = append(p, " S"); p = decimal(p, (int)b); break;
    case KK_COPY_STEP: p = append(p, "STEP "); p = decimal(p, (int)a); p = append(p, " P"); p = decimal(p, (int)b); break;
    case KK_COPY_SOUND: kk_note_label(p, (int)a); while (*p) ++p; p = append(p, " P"); p = decimal(p, (int)b); break;
    case KK_COPY_BAR: p = append(p, "BAR "); p = decimal(p, (int)a); break;
    default: p = append(p, "-"); break;
    }
    text_center(120, y, buf, 1, kind & 64U ? GREEN : CYAN);
}
static void draw_big(void)
{
    const struct widget *w = &widgets[W_BIG];
    fill(0, w->y, 240, w->h, BG);
    char buf[64];
    if (kk.mode == KK_MODE_STEP) {
        /* Step Mode: page, resolution, follow, and what the script reports. */
        static const char *const res[5] = {"1/16", "1/8", "1/32", "1/16T", "1/8T"};
        unsigned status = kk_midi_step_info(KK_STEP_STATUS_CC), drum = kk_midi_step_info(KK_STEP_DRUMVIEW_CC);
        /* 0.62: the title shows the bar the clip is playing ("BAR 2/4"), or
         * its length when stopped ("4 BARS"). */
        unsigned bar = kk_midi_step_info(KK_STEP_BAR_CC), bars = kk_midi_step_info(KK_STEP_BARS_CC);
        char tbuf[16], *t = tbuf; tbuf[0] = 0;
        if (bars && bar) { t = append(t, "BAR "); t = decimal(t, (int)bar); t = append(t, "/"); t = decimal(t, (int)bars); }
        else if (bars) { t = decimal(t, (int)bars); t = append(t, bars == 1 ? " BAR" : " BARS"); }
        const char *title = !kk_midi_host_protocol() ? "STEP" : drum ? "DRUM PADS" : status == 2 ? "NO MIDI TRACK"
                          : status == 0 && tbuf[0] ? tbuf : "STEP";
        text_center(120, w->y + 14, title, text_width(title, 3) <= 228 ? 3 : 2, drum ? TEAL : status == 2 ? RED : ORANGE);
        char *p = buf;
        if (!kk_midi_host_protocol()) p = append(p, "LOAD THE KAMOTEKEYS REMOTE SCRIPT");
        else if (status == 1) p = append(p, "NO DRUM RACK - NOTE C1");
        else if (status == 2) p = append(p, "SELECT A MIDI TRACK");
        else if (status == 3) p = append(p, "PRESS A STEP TO START A CLIP");
        else {
            p = append(p, "PAGE "); p = decimal(p, (int)kk_midi_step_info(KK_STEP_PAGE_CC) + 1); p = append(p, "/");
            p = decimal(p, (int)kk_midi_step_info(KK_STEP_PAGES_CC)); p = append(p, "  ");
            p = append(p, res[kk_midi_step_info(KK_STEP_RES_CC) % 5]);
            if (kk_midi_step_info(KK_STEP_FOLLOW_CC)) p = append(p, "  FOLLOW");
            p = append(p, "  "); kk_note_label(p, (int)kk_midi_step_info(KK_STEP_ACTIVE_CC)); while (*p) ++p;
            if (kk_midi_step_info(KK_STEP_KEYS_CC)) p = append(p, " SCALE");
            if (kk.chord_mods) p = append(p, kk.step_single ? " SINGLE" : " CHORD");
        }
        text_center(120, w->y + 42, buf, 1, DIM);
        copy_line(w->y + 55);
        return;
    }
    if (kk.mode == KK_MODE_CLIP && kk.clip_action) {
        /* 0.50: OVERDUB (first black key) / DELETE (second) held. */
        text_center(120, w->y + 14, kk.clip_action == 2 ? "DELETE" : kk.clip_action == 3 ? "SELECT" : "OVERDUB", 3,
                    kk.clip_action == 2 ? RED : kk.clip_action == 3 ? CYAN : AMBER);
        text_center(120, w->y + 50, "PRESS A CLIP", 1, TEXT);
        return;
    }
    if (kk.mode == KK_MODE_CLIP && kk_midi_arrange()) {
        /* 0.66 Scene Arrange: pads = which tracks play in these two scenes. */
        text_center(120, w->y + 14, "ARRANGE", 3, GREEN);
        char *p = append(buf, "SCENES "); p = decimal(p, (int)kk_midi_host_scene() + 1); p = append(p, " / ");
        p = decimal(p, (int)kk_midi_host_scene() + 2); p = append(p, "  TRACKS "); p = decimal(p, (int)kk_midi_host_tracks() + 1);
        p = append(p, "-"); decimal(p, (int)kk_midi_host_tracks() + 8);
        text_center(120, w->y + 42, buf, 1, DIM);
        text_center(120, w->y + 55, "TAP: TRACK ON / OFF   TOP F#: LAUNCH", 1, TEXT);
        return;
    }
    if (kk.mode == KK_MODE_CLIP) {
        static const char *const states[] = {"EMPTY", "STOPPED", "PLAYING", "QUEUED", "RECORDING", "QUEUED REC", "EMPTY (ARMED)"};
        unsigned st = kk_midi_clip_state(kk.clip_pad);
        const char *name = !kk_midi_host_protocol() ? "CLIPS" : st < 7 ? states[st] : "--";
        static const uint16_t colours[7] = {DIM, C565(60, 120, 255), GREEN, AMBER, RED, RED, DIM};
        text_center(120, w->y + 14, name, text_width(name, 3) <= 228 ? 3 : 2,
                    kk_midi_host_protocol() && st < 7 ? colours[st] : TEXT);
        char *p = append(buf, "TRACK "); p = decimal(p, (int)(kk_midi_host_tracks() + kk.clip_pad % 8 + 1));
        p = append(p, "  SCENE "); p = decimal(p, (int)(kk_midi_host_scene() + kk.clip_pad / 8 + 1));
#ifdef KK_CLIP_PRESS_SELECT
        p = append(p, kk.clip_play ? "  PADS: PLAY" : "  PADS: SELECT");   /* PLAY while the PLAY key is held / tapped */
#endif
        text_center(120, w->y + 42, kk_midi_host_protocol() ? buf : "LOAD THE KAMOTEKEYS REMOTE SCRIPT", 1, DIM);
        copy_line(w->y + 55);
        return;
    }
    const char *name = big_name[0] ? big_name : "--";
    unsigned scale = 3;
    while (scale > 2 && text_width(name, scale) > 228) --scale;
    uint16_t colour = !sounding_n && big_kind != 3 && big_kind != 4 ? DIM
        : big_kind == 2 ? CYAN : big_kind == 4 ? ORANGE : GREEN;
    text_center(120, w->y + 14 + (3 - (int)scale) * 4, name, scale, colour);
    const char *caption = big_kind == 2 ? "CHORD" : big_kind == 3 ? "PAD NOTE" : big_kind == 4 ? "DRUM"
        : big_kind == 1 ? "NOTE" : "PLAY A KEY";
    if (big_kind == 3 || big_kind == 4) {
        char *p = append(buf, caption); p = append(p, "  PAD "); decimal(p, kk.last_pad + 1);
        caption = buf;
    }
    text_center(120, w->y + 50, caption, 1, DIM);
}
static void draw_scale(void)
{
    const struct widget *w = &widgets[W_SCALE];
    fill(0, w->y, 240, w->h, BG);
    fill(6, w->y + 1, 228, 22, PANEL);
    int x = text(12, w->y + 5, kk_note_name(kk.scale.root), 2, CYAN);
    const char *name = kk_scale_name(kk.scale.scale);
    unsigned scale = text_width(name, 2) <= 222 - (x + 8) ? 2 : 1;
    text(x + 8, w->y + (scale == 2 ? 5 : 9), name, scale, TEXT);
}
static void draw_status(void)
{
    const struct widget *w = &widgets[W_STATUS];
    fill(0, w->y, 240, w->h, BG);
    fill(0, w->y, 240, 1, PANEL);
    char buf[48], *p;   /* "LOCK " + every chord type fits */
    if (kk.arp_on) {
        pill(6, w->y + 5, 26, 12, C565(0, 70, 80)); text(10, w->y + 8, "ARP", 1, CYAN);
        p = append(buf, kk_division_name(kk.arp.division)); p = append(p, " ");
        p = append(p, kk_direction_name(kk.arp.direction)); p = append(p, " "); p = decimal(p, kk.arp.gate); append(p, "%");
        text(38, w->y + 8, buf, 1, TEXT);
        if (kk.arp.mode == KK_ARP_LATCH) text(150, w->y + 8, "LATCH", 1, AMBER);
    } else text(10, w->y + 8, "ARP OFF", 1, FAINT);
    if (kk.sustain_on) text_right(234, w->y + 8, "SUSTAIN", 1, GREEN);
    if (kk.anim_active) {
        p = append(buf, "LIGHT SHOW "); p = decimal(p, kk.anim_pattern + 1); p = append(p, "/8  SPEED ");
        decimal(p, kk.anim_speed / 10);
        text(10, w->y + 22, buf, 1, CYAN);
        return;
    }
    if (popup_until && (int32_t)(now_ms - popup_until) < 0) {
        text(10, w->y + 22, popup, 1, AMBER);
        return;
    }
    unsigned v = kk_velocity_override();
    p = append(buf, "VEL ");
    if (v) decimal(p, (int)v); else append(p, "NAT");
#ifdef KK_FIXED_VELOCITY
    text(10, w->y + 22, buf, 1, DIM);         /* fixed velocity is the normal case */
#else
    text(10, w->y + 22, buf, 1, v ? AMBER : DIM);
#endif
    if ((kk.chord_mods || kk.chord_lock) && kk.mode <= KK_MODE_KEY_SCALE) {
        static const char *const mods[9] = {"MAJ", "MIN", "DIM", "SUS4", "7", "MAJ7", "6", "9", "SCALE"};
        p = buf; *p = 0;
        if (kk.chord_lock) p = append(p, kk.chord_mods ? "LOCK " : "LOCK");
        for (unsigned i = 0, first = 1; i < 9; ++i)
            if (kk.chord_mods & (1U << i)) { if (!first) p = append(p, "+"); p = append(p, mods[i]); first = 0; }
        text_right(234, w->y + 22, buf, 1, ORANGE);
    } else if (!kk_midi_local()) text_right(234, w->y + 22, "SYNTH OFF", 1, DIM);
}

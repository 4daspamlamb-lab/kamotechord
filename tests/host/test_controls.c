/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: Theory, buttons, Key Scale, velocity and faders. */
#include "helpers.h"

/* ------------------------------------------------------------- tests */
void test_theory(void)
{
    struct kk_scale_ctx a_major = {0, 9}, a_minpent = {10, 9};
    static const int8_t major_up[8] = {69, 71, 73, 74, 76, 78, 80, 81};
    static const uint8_t whites_up[8] = {12, 14, 16, 17, 19, 21, 23, 24};
    for (unsigned i = 0; i < 8; ++i)
        CHECK(kk_key_scale_note(whites_up[i], 0, &a_major) == major_up[i], "A major white %u -> %d", i, kk_key_scale_note(whites_up[i], 0, &a_major));
    static const int8_t pent_up[7] = {69, 72, 74, 76, 79, 81, 84};
    for (unsigned i = 0; i < 7; ++i)
        CHECK(kk_key_scale_note(whites_up[i], 0, &a_minpent) == pent_up[i], "A min pent up %u", i);
    /* Below the anchor the keys descend through the scale. */
    static const uint8_t whites_down[7] = {11, 9, 7, 5, 4, 2, 0};
    static const int8_t pent_down[7] = {67, 64, 62, 60, 57, 55, 52};
    for (unsigned i = 0; i < 7; ++i)
        CHECK(kk_key_scale_note(whites_down[i], 0, &a_minpent) == pent_down[i], "A min pent down %u -> %d", i, kk_key_scale_note(whites_down[i], 0, &a_minpent));
    CHECK(kk_key_scale_note(13, 0, &a_major) == -1, "black key disabled");
    CHECK(kk_key_scale_note(12, 1, &a_minpent) == 81 && kk_key_scale_note(12, -1, &a_minpent) == 57, "pentatonic octave shift");
    /* Every scale: monotonic white-key mapping, anchor = tonic. */
    for (unsigned s = 0; s < KK_SCALES; ++s) for (unsigned r = 0; r < 12; ++r) {
        struct kk_scale_ctx c = {(uint8_t)s, (uint8_t)r};
        CHECK(kk_key_scale_note(12, 0, &c) == 60 + (int)r, "anchor scale %u root %u", s, r);
        int prev = -1;
        for (unsigned k = 0; k < 37; ++k) {
            int n = kk_key_scale_note(k, 0, &c);
            if (n < 0) continue;
            CHECK(n > prev && kk_in_scale(&c, n), "scale %u root %u key %u", s, r, k);
            prev = n;
        }
    }
    uint8_t notes[KK_MAX_CHORD]; char name[16];
    unsigned n = kk_chord_build(69, KK_MOD_MIN | KK_MOD_7, &a_minpent, notes);
    kk_chord_name(notes, n, 69, name);
    CHECK(n == 4 && notes[1] == 72 && notes[2] == 76 && notes[3] == 79 && !strcmp(name, "Am7"), "Am7 -> %s", name);
    struct kk_scale_ctx c_major = {0, 0};
    static const struct { int root; unsigned mods; const char *name; } chords[] = {
        {60, KK_MOD_MAJ, "C"}, {60, KK_MOD_MIN, "Cm"}, {60, KK_MOD_DIM, "Cdim"}, {60, KK_MOD_SUS4, "Csus4"},
        {60, KK_MOD_MAJ | KK_MOD_7, "C7"}, {60, KK_MOD_MAJ | KK_MOD_MAJ7, "Cmaj7"}, {60, KK_MOD_MIN | KK_MOD_MAJ7, "CmMaj7"},
        {60, KK_MOD_DIM | KK_MOD_7, "Cdim7"}, {60, KK_MOD_MAJ | KK_MOD_6, "C6"}, {60, KK_MOD_MAJ | KK_MOD_9, "Cadd9"},
        {60, KK_MOD_MAJ | KK_MOD_7 | KK_MOD_9, "C9"}, {60, KK_MOD_MIN | KK_MOD_7 | KK_MOD_9, "Cm9"},
        {62, KK_MOD_SCALE, "Dm"}, {71, KK_MOD_SCALE | KK_MOD_7, "Bm7b5"}, {67, KK_MOD_7, "G7"}, {64, KK_MOD_SCALE | KK_MOD_7, "Em7"},
        {65, KK_MOD_MAJ7, "Fmaj7"}};
    for (unsigned i = 0; i < sizeof chords / sizeof *chords; ++i) {
        n = kk_chord_build(chords[i].root, chords[i].mods, &c_major, notes);
        kk_chord_name(notes, n, chords[i].root, name);
        CHECK(!strcmp(name, chords[i].name), "chord %s -> %s", chords[i].name, name);
    }
    /* Root detection from an inverted voicing (E G C -> C). */
    uint8_t inv[3] = {64, 67, 72}; kk_chord_name(inv, 3, -1, name);
    CHECK(!strcmp(name, "C"), "inversion -> %s", name);
    uint8_t one = 69; kk_chord_name(&one, 1, -1, name);
    CHECK(!strcmp(name, "A4"), "single -> %s", name);
    static const uint8_t drums[16] = {36, 38, 40, 42, 44, 46, 48, 50, 37, 39, 41, 43, 45, 47, 49, 51};
    for (unsigned p = 0; p < 16; ++p) CHECK(kk_drum_note(p, 0) == drums[p], "drum pad %u", p + 1);
    CHECK(kk_drum_note(0, 1) == 48 && kk_drum_note(0, -1) == 24 && kk_drum_note(15, 7) == -1, "drum octave");
    CHECK(kk_drum_family(36) == KK_DRUM_KICK && kk_drum_family(38) == KK_DRUM_SNARE && kk_drum_family(42) == KK_DRUM_HIHAT &&
          kk_drum_family(41) == KK_DRUM_TOM && kk_drum_family(37) == KK_DRUM_RIM && kk_drum_family(39) == KK_DRUM_CLAP &&
          kk_drum_family(49) == KK_DRUM_CYMBAL, "drum families");
    static const int8_t spare[16] = {-1, -1, -1, -1, 84, 85, 86, 87, -1, -1, -1, -1, -1, 88, 89, 90};
    for (unsigned p = 0; p < 16; ++p) CHECK(kk_spare_pad_note(p) == spare[p], "spare pad %u", p + 1);
}

void test_buttons(void)
{
    boot();
    static const struct { unsigned id, cc; } momentary[] = {
        {KK_BTN_PLAY, KK_CC_FINISH}, {KK_BTN_GLOBE, 21}, {KK_BTN_PARA, 23}, {KK_BTN_RIGHT, 27}};
    /* HOME (LEFT) = Undo on release (0.51: it doubles as a shift for PARA). */
    host_clear(); btn(KK_BTN_LEFT, 1);
    CHECK(out_count(USB, CC15, 24) == 0 && kk_led_button(KK_BTN_LEFT) == 2, "HOME press: lit, Undo waits for release");
    btn(KK_BTN_LEFT, 0);
    CHECK(out_count(USB, CC15, 24) == 2 && last_out(USB, CC15, 0) == 24 && last_out(USB, CC15, 1) == (24 | 127 << 8), "HOME release -> Undo 127/0");
    for (unsigned i = 0; i < 4; ++i) {
        host_clear(); btn(momentary[i].id, 1);
        CHECK(out_count(USB, CC15, momentary[i].cc) == 1 && last_out(USB, CC15, 0) == (int)(momentary[i].cc | 127 << 8), "press cc %u", momentary[i].cc);
        CHECK(kk_led_button(momentary[i].id) == 2, "momentary bright %u", momentary[i].id);
        btn(momentary[i].id, 0);
        CHECK(out_count(USB, CC15, momentary[i].cc) == 2 && last_out(USB, CC15, 0) == (int)momentary[i].cc, "release cc %u", momentary[i].cc);
        CHECK(kk_led_button(momentary[i].id) == 1, "momentary dim %u", momentary[i].id);
        CHECK(out_count(BLE, CC15, -1) == 0, "AUTO: no duplicate on BLE while USB is ready");
    }
    /* 0.59: KNOB BANK = Session Record, FADER BANK = Capture, sent on release
     * (so they can be held together for the light show). */
    host_clear(); btn(KK_BTN_KNOB_BANK, 1);
    CHECK(out_count(USB, CC15, 25) == 0 && kk_led_button(KK_BTN_KNOB_BANK) == 2, "session record press: lit, waits for release");
    btn(KK_BTN_KNOB_BANK, 0);
    CHECK(out_count(USB, CC15, 25) == 2 && last_out(USB, CC15, 0) == 25 && kk_led_button(KK_BTN_KNOB_BANK) == 1, "session record release -> 127/0");
    host_clear(); btn(KK_BTN_FADER_BANK, 1);
    CHECK(out_count(USB, CC15, 22) == 0 && kk_led_button(KK_BTN_FADER_BANK) == 2, "capture press: lit, waits for release");
    btn(KK_BTN_FADER_BANK, 0);
    CHECK(out_count(USB, CC15, 22) == 2 && last_out(USB, CC15, 0) == 22, "capture release -> 127/0");
    host_clear(); show_on(); show_off();
    CHECK(out_count(USB, CC15, 22) == 0 && out_count(USB, CC15, 25) == 0, "KNOB + FADER BANK together send neither");
    /* 0.51: PARA = Play/Pause (CC 23); HOME (LEFT) + PARA = metronome (CC 26),
     * and HOME then sends no Undo. */
    host_clear(); unsigned page = kk.page, mode = kk.mode;
    tap(KK_BTN_PARA);
    CHECK(out_count(USB, CC15, 23) == 2 && last_out(USB, CC15, 1) == (23 | 127 << 8) && kk.page == page && kk.mode == mode, "PARA -> Play/Pause");
    CHECK(kk_led_button(KK_BTN_PARA) == 1 && kk_led_button(KK_BTN_CHORD) == 1, "PARA dim at rest, CHORD (Clip Mode) dim");
    host_clear(); btn(KK_BTN_LEFT, 1); tap(KK_BTN_PARA); btn(KK_BTN_LEFT, 0);
    CHECK(out_count(USB, CC15, 26) == 2 && out_count(USB, CC15, 24) == 0 && out_count(USB, CC15, 23) == 0, "HOME + PARA -> metronome, no Undo");
    host_clear(); tap(KK_BTN_LEFT);
    CHECK(out_count(USB, CC15, 24) == 2, "HOME alone still Undo");
    /* 0.54: PATCH (SMK37 OPEN's HOME) + PARA = metronome; PATCH then opens no page. */
    host_clear(); btn(KK_BTN_PATCH, 1); tap(KK_BTN_PARA); btn(KK_BTN_PATCH, 0);
    CHECK(out_count(USB, CC15, 26) == 2 && out_count(USB, CC15, 23) == 0 && kk.page == KK_PAGE_MAIN, "PATCH + PARA -> metronome, no page");
    /* 0.45: SEQ = Key Scale, SCALE = Pad Scale, independent. */
    tap(KK_BTN_SEQ);
    CHECK(kk.mode == KK_MODE_KEY_SCALE && kk_led_button(KK_BTN_SEQ) == 2 && kk_led_button(KK_BTN_SCALE) == 1, "SEQ -> Key Scale only");
    tap(KK_BTN_SCALE);
    CHECK(kk.mode == KK_MODE_PAD_SCALE && kk_led_button(KK_BTN_SCALE) == 2 && kk_led_button(KK_BTN_SEQ) == 1, "SCALE -> Pad Scale only");
    CHECK(kk_led_button(KK_BTN_SCALE) == 2 && kk_led_button(KK_BTN_FX) == 1, "SCALE LED");
    tap(KK_BTN_FX);
    CHECK(kk.mode == KK_MODE_DRUM && kk_led_button(KK_BTN_FX) == 2 && kk_led_button(KK_BTN_SCALE) == 1, "FX -> Drum");
    tap(KK_BTN_FX);
    CHECK(kk.mode == KK_MODE_NORMAL, "same mode button returns to normal");
    CHECK(kk_led_button(KK_BTN_SCALE) == kk_led_button(KK_BTN_SEQ) && kk_led_button(KK_BTN_SCALE) == kk_led_button(KK_BTN_NOTE_REPEAT),
          "idle SEQ LED matches SCALE and SUSTAIN");
    /* BT opens the Bluetooth menu; REC never does. */
    tap(KK_BTN_REC); CHECK(kk.page == KK_PAGE_MAIN && kk.mode == KK_MODE_STEP, "SEQ REC -> Step Mode, not BT");
    tap(KK_BTN_REC); CHECK(kk.mode == KK_MODE_NORMAL, "SEQ REC again -> back to Normal");
    tap(KK_BTN_BT); CHECK(kk.page == KK_PAGE_BT && kk_led_button(KK_BTN_BT) == 2, "BT opens menu");
    host_clear(); tap(KK_BTN_LEFT);
    CHECK(kk.page == KK_PAGE_MAIN && out_count(USB, CC15, 24) == 0, "LEFT acts as HOME in the BT menu, no Undo sent");
    tap(KK_BTN_BT); tap(KK_BTN_BT); CHECK(kk.page == KK_PAGE_MAIN, "BT toggles the menu");
    btn(KK_BTN_BT, 1); host_ms += 1600; frames(2, 1); btn(KK_BTN_BT, 0);
    CHECK(kk.page == KK_PAGE_DIAG, "long BT -> diagnostics"); tap(KK_BTN_BT);
    /* Octave. */
    kk_set_octave(0);
    tap(KK_BTN_OCT_UP); tap(KK_BTN_OCT_UP); CHECK(kk.octave == 2, "octave up");
    tap(KK_BTN_OCT_DOWN); tap(KK_BTN_OCT_DOWN); tap(KK_BTN_OCT_DOWN); CHECK(kk.octave == -1, "octave down");
    for (unsigned i = 0; i < 9; ++i) tap(KK_BTN_OCT_DOWN);
    CHECK(kk.octave == KK_OCTAVE_MIN, "octave clamp");
    /* ARP short tap toggles, long press opens the ARP page. */
    tap(KK_BTN_ARP); CHECK(kk.arp_on && kk_led_button(KK_BTN_ARP) == 2, "ARP on"); tap(KK_BTN_ARP); CHECK(!kk.arp_on, "ARP off");
    btn(KK_BTN_ARP, 1); host_ms += 700; frames(2, 1); btn(KK_BTN_ARP, 0);
    CHECK(kk.page == KK_PAGE_ARP && !kk.arp_on, "ARP long press opens page without toggling");
    tap(KK_BTN_ARP); CHECK(kk.page == KK_PAGE_MAIN, "ARP tap leaves page");
    tap(KK_BTN_PATCH); CHECK(kk.page == KK_PAGE_PATCH, "PATCH page"); tap(KK_BTN_PATCH);
}

void test_key_scale(void)
{
    boot();
    fader(1, 10 * 128 / 23 + 2);   /* Minor Pentatonic */
    fader(2, 9 * 128 / 12 + 4);    /* A */
    CHECK(kk.scale.scale == 10 && kk.scale.root == 9, "faders select A minor pentatonic (%u %u)", kk.scale.scale, kk.scale.root);
    tap(KK_BTN_SEQ);
    host_clear(); key(12, 90);
    CHECK(out_count(USB, ON1, 69) == 1 && last_out(USB, ON1, 0) == (69 | 90 << 8), "second C -> A4 natural velocity");
    key(12, 0); CHECK(out_count(USB, OFF1, 69) == 1, "note off");
    host_clear(); key(13, 100); CHECK(host_out_n == 0 && host_local_n == 0, "black key silent"); key(13, 0);
    host_clear(); key(4, 80); CHECK(out_count(USB, ON1, 57) == 1, "E below anchor -> A3"); key(4, 0);
    /* Chord pads: MIN (pad 2) + 7 (pad 9) -> Am7. */
    pad(2, 100); pad(9, 100);
    host_clear(); key(12, 100);
    uint8_t notes[16]; unsigned n; notes_on(USB, ON1, notes, &n);
    CHECK(n == 4 && notes[0] == 69 && notes[1] == 72 && notes[2] == 76 && notes[3] == 79, "Am7 voicing (%u notes)", n);
    frames(40, 2);
    shot("02_key_scale_Am7");
    CHECK(kk.chord_root == 69, "chord root");
    /* Releasing the 7 pad while holding re-voices to Am: only G stops. */
    host_clear(); pad(9, 0);
    CHECK(out_count(USB, OFF1, 79) == 1 && host_out_n == 1, "modifier release removes only the 7th");
    host_clear(); key(12, 0);
    CHECK(out_count(USB, OFF1, -1) == 3, "chord released");
    pad(2, 0);
    /* Octave change while a key is held: its notes still stop. */
    host_clear(); key(14, 100); tap(KK_BTN_OCT_UP); key(14, 0);
    CHECK(out_count(USB, ON1, 72) == 1 && out_count(USB, OFF1, 72) == 1, "octave change keeps held note release");
    key(14, 100); CHECK(last_out(USB, ON1, 0) == (84 | 100 << 8), "new octave applies"); key(14, 0);
    tap(KK_BTN_OCT_DOWN);
    /* Single note display. */
    key(16, 100); frames(60, 2); shot("03_key_scale_single"); key(16, 0);
    tap(KK_BTN_SEQ);
}

void test_velocity_faders(void)
{
    boot();
    host_clear(); fader(0, 13); key(0, 50);
    CHECK(last_out(USB, ON1, 0) == (48 | 50 << 8), "Fader 1 at 9%%: natural velocity"); key(0, 0);
    fader(0, 127); key(0, 50);
    CHECK(last_out(USB, ON1, 0) == (48 | 127 << 8), "Fader 1 at 100%%: 127"); key(0, 0);
    fader(0, 64); key(0, 50);
    unsigned v = (unsigned)last_out(USB, ON1, 0) >> 8; CHECK(v > 55 && v < 75, "Fader 1 mid: %u", v); key(0, 0);
    /* Note Off unaffected, held velocity unaffected. */
    kk_set_octave(0); host_clear(); key(1, 30); fader(0, 127); key(1, 0);
    CHECK(out_count(USB, OFF1, 49) == 1 && host_out[0].b[2] == 64 * 0 + v, "velocity fixed at note on");
    fader(0, 0);
    /* Fader 2 hysteresis: hovering at a boundary does not chatter. */
    fader(1, 60); unsigned s = kk.scale.scale;
    fader(1, 61); fader(1, 60); fader(1, 61);
    CHECK(kk.scale.scale == s || kk.scale.scale == s + 1, "scale stable");
    unsigned changes = 0, prev = kk.scale.scale;
    for (unsigned i = 0; i < 20; ++i) { fader(1, 61 + (i & 1)); if (kk.scale.scale != prev) ++changes, prev = kk.scale.scale; }
    CHECK(changes <= 1, "no chatter (%u)", changes);
    fader(1, 0); CHECK(kk.scale.scale == 0, "fader 2 bottom = Major");
    fader(1, 127); CHECK(kk.scale.scale == 22, "fader 2 top = Minor 6-Diminished");
    fader(2, 0); CHECK(kk.scale.root == 0, "root C"); fader(2, 127); CHECK(kk.scale.root == 11, "root B");
    /* 0.77: pad brightness from the Patch page (16 steps); Fader 4 = master volume. */
    host_clear(); fader(3, 90);
    CHECK(kk.master == 90 && last_out(USB, CC15, 0) == (KK_CC_MASTER | 90 << 8), "Fader 4 = master volume (CC 35)");
    fader(3, 127);
    kk_set_brightness(0); settle();
    unsigned lit = 0; for (unsigned p = 0; p < 16; ++p) lit |= host_rgb[p];
    CHECK(lit == 0, "Fader 4 at 0: pads off");
    pad(5, 100); frames(2, 2);
    CHECK(host_rgb[4] == 0, "Fader 4 at 0: pressed pad still dark"); pad(5, 0);
    kk_set_brightness(15); pad(5, 100); frames(2, 2); uint32_t full = host_rgb[4];
    CHECK(((full >> 16) & 255) == KK_RGB_PEAK, "instant attack (%06x)", full);
    kk_set_brightness(8); settle(); uint32_t half = host_rgb[4];
    CHECK(((full >> 16) & 255) == KK_RGB_PEAK && ((half >> 16) & 255) * 2 >= KK_RGB_PEAK - 12 && ((half >> 16) & 255) * 2 <= KK_RGB_PEAK + 24, "brightness scales (%06x %06x)", full, half);
    kk_set_brightness(15);
    pad(5, 0); frames(10, 3);
    unsigned released = (host_rgb[4] >> 16) & 255;
    CHECK(released >= 16 && released <= 24, "release returns to idle at once (%u)", released);
    settle();
    unsigned idle = (host_rgb[4] >> 16) & 255;
    CHECK(idle >= 16 && idle <= 24, "spare idle 12.5%% = half of chord 25%% (%u)", idle);
    kk_set_brightness(1); CHECK(kk.screen_dim, "LCD dims at <= 10%%"); kk_set_brightness(15); CHECK(!kk.screen_dim, "LCD normal");
    { uint8_t d[16]; kk_set_brightness(6); kamotkeys_settings_read(d); kk_set_brightness(15);
      CHECK(kamotkeys_settings_apply(d) == 0 && kk.brightness_level == 6, "brightness saved"); kk_set_brightness(15); }
    /* Every channel sent to the pads is a multiple of 4, in every mode and at
     * every Fader 4 position (values with bit 0/1 set flash on hardware). */
    {
        static const uint8_t modes[] = {KK_BTN_SEQ, KK_BTN_SCALE, KK_BTN_FX, KK_BTN_CHORD};
        unsigned odd = 0;
        for (unsigned m = 0; m <= 4; ++m) {
            for (unsigned v = 0; v <= 127; v += 9) {
                kk_set_brightness(v / 9U); pad(1, 90); pad(5, 90); frames(3, 3); pad(1, 0); pad(5, 0); frames(3, 3);
                for (unsigned p = 0; p < 16; ++p) odd |= host_rgb[p] & 0x030303;
            }
            if (m < 4) tap(modes[m]);
        }
        tap(KK_BTN_CHORD); kk_set_brightness(15);
        CHECK(!odd && kk.mode == KK_MODE_NORMAL, "all pad values are multiples of 4 (%06x)", odd);
    }
    /* Idle chord pads keep their hue (the blue MIN pad is clearly blue). */
    settle();
    CHECK((host_rgb[1] & 255) > 2 * ((host_rgb[1] >> 16) & 255) && (host_rgb[1] & 255) >= 15, "MIN pad idle is blue (%06x)", host_rgb[1]);
    /* Fader 1 is animation speed while SEQ REC is held. */
    fader(0, 127); show_on(); frames(3, 5);
    CHECK(kk_velocity_override() == 0 && kk.anim_speed == 2000, "light show: fader 1 = speed (%u)", kk.anim_speed);
    fader(0, 0); frames(2, 5); CHECK(kk.anim_speed == 1000, "0%% = half max");
    fader(0, 14); frames(2, 5); CHECK(kk.anim_speed < 80, "1-10%% very slow (%u)", kk.anim_speed);
    host_clear(); pad(5, 100); pad(5, 0); CHECK(host_out_n == 0, "pads silent during light show");
    show_off(); fader(0, 127);
    CHECK(kk_velocity_override() == 127, "velocity restored after the light show");
    fader(0, 0);
}

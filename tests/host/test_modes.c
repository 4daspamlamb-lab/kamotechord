/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: Pad modes, spare pads and refcounts, ARP/keys/persistence, fuzz, display screenshots. */
#include "helpers.h"

void test_pad_modes(void)
{
    boot();
    fader(1, 0); fader(2, 2 * 128 / 12 + 4);   /* D major */
    tap(KK_BTN_SCALE);
    host_clear();
    for (unsigned p = 1; p <= 16; ++p) { pad(p, 100); pad(p, 0); }
    CHECK(out_count(USB, ON1, -1) == 16 && out_count(USB, OFF1, -1) == 16, "16 scale pads");
    CHECK(host_out[0].b[1] == 50 && host_out[2 * 7].b[1] == 62, "pad 1 = D3, pad 8 = D4");
    /* Root pads cyan, others green. */
    settle();
    CHECK((host_rgb[0] & 0xff) * 10 >= ((host_rgb[0] >> 8) & 0xff) * 9 && ((host_rgb[1] >> 8) & 0xff) > 2 * (host_rgb[1] & 0xff),
          "root cyan / other green (%06x %06x)", host_rgb[0], host_rgb[1]);
    CHECK(((host_rgb[0] >> 8) & 0xff) > 3 * ((host_rgb[1] >> 8) & 0xff) / 2, "idle roots clearly brighter than other notes");
    pad(3, 110); frames(60, 2); shot("04_pad_scale"); pad(3, 0);
    /* Pad ARP in Pad Scale Mode: channel 1. */
    tap(KK_BTN_ARP); host_clear();
    pad(1, 100); pad(3, 100); frames(250, 2);   /* 0.5 s at 120 BPM, 1/8 = 250 ms */
    unsigned ons = out_count(USB, ON1, -1);
    CHECK(ons == 2 || ons == 3, "pad arp ~2 steps in 500 ms (%u)", ons);
    CHECK(out_count(USB, ON1, 50) >= 1 && out_count(USB, ON1, 54) >= 1, "arp cycles D and F#");
    pad(1, 0); pad(3, 0); frames(200, 2);
    CHECK(kk_midi_count(0, 50) == 0 && kk_midi_count(0, 54) == 0, "arp stops");
    tap(KK_BTN_ARP);
    /* Drum Mode. */
    tap(KK_BTN_FX); host_clear();
    pad(1, 100);
    CHECK(out_count(USB, ON10, 36) == 1 && out_count(USB, ON1, -1) == 0, "pad 1 -> 36 ch10");
    pad(1, 0); CHECK(out_count(USB, OFF10, 36) == 1, "drum off");
    tap(KK_BTN_OCT_UP); host_clear(); pad(1, 100); pad(1, 0); CHECK(out_count(USB, ON10, 48) == 1, "octave +1 -> 48");
    tap(KK_BTN_OCT_DOWN); tap(KK_BTN_OCT_DOWN); host_clear(); pad(1, 100); pad(1, 0); CHECK(out_count(USB, ON10, 24) == 1, "octave -1 -> 24");
    tap(KK_BTN_OCT_UP);
    settle();
    CHECK((host_rgb[0] >> 16) > 0 && (host_rgb[0] & 0xffff) == 0, "kick red (%06x)", host_rgb[0]);
    CHECK((host_rgb[3] >> 16) > 0 && ((host_rgb[3] >> 8) & 255) > 0 && (host_rgb[3] & 255) == 0, "hi-hat yellow (%06x)", host_rgb[3]);
    pad(2, 120); frames(60, 2); shot("05_drum"); pad(2, 0);
    /* Drum ARP on channel 10. */
    tap(KK_BTN_ARP); host_clear(); pad(1, 100); pad(2, 100); frames(260, 2);
    CHECK(out_count(USB, ON10, 36) >= 1 && out_count(USB, ON10, 38) >= 1 && out_count(USB, ON1, -1) == 0, "drum arp ch10");
    pad(1, 0); pad(2, 0); tap(KK_BTN_ARP); frames(100, 2);
    /* Mode change releases held pad notes. */
    host_clear(); pad(4, 100); tap(KK_BTN_SCALE);
    CHECK(out_count(USB, OFF10, 42) == 1, "mode change releases drum note");
    pad(4, 0); CHECK(out_count(USB, OFF1, -1) == 0, "consumed pad stays silent");
    tap(KK_BTN_SCALE);
}

void test_spare_and_refcount(void)
{
    boot();
    /* 0.43: spare pads are mappable CC buttons on channel 15 (CC 102-108). */
    static const uint8_t spare_pads[7] = {5, 6, 7, 8, 14, 15, 16};
    for (unsigned i = 0; i < 7; ++i) {
        host_clear(); pad(spare_pads[i], 100);
        CHECK(last_out(USB, CC15, 0) == ((102 + (int)i) | 127 << 8) && out_count(USB, ON1, -1) == 0 && host_local_n == 0,
              "spare pad %u -> CC %u = 127 (no note, no synth)", spare_pads[i], 102 + i);
        frames(2, 2); CHECK(kk_pad_lit(spare_pads[i] - 1), "lit while held");
        pad(spare_pads[i], 0);
        CHECK(last_out(USB, CC15, 0) == (102 + (int)i) && out_count(USB, CC15, -1) == 2, "release -> CC 0");
    }
    host_clear(); tap(KK_BTN_ARP); pad(5, 100); tap(KK_BTN_ARP); pad(5, 0);
    CHECK(out_count(USB, CC15, 102) == 2, "ARP toggles never retrigger a held CC pad (%u)", out_count(USB, CC15, 102));
    host_clear(); pad(6, 100); tap(KK_BTN_SCALE);
    CHECK(last_out(USB, CC15, 0) == 103, "mode change releases a held CC pad");
    pad(6, 0); tap(KK_BTN_SCALE); CHECK(out_count(USB, CC15, 103) == 2, "and its release stays silent");
    /* Two sources sharing one note: C major on the second C and the G key. */
    host_clear(); pad(1, 100); key(12, 100); key(19, 100);
    CHECK(out_count(USB, ON1, 67) == 1, "shared note sent once");
    key(12, 0);
    CHECK(out_count(USB, OFF1, 67) == 0, "no spurious off while the other key still holds it");
    key(19, 0); pad(1, 0);
    CHECK(out_count(USB, OFF1, 67) == 1, "off when last owner releases");
    host_clear(); pad(1, 100); pad(1, 0);
    CHECK(host_out_n == 0, "chord pad alone sends nothing");
    /* AUTO port policy. */
    host_ready_ports = 4; groove_midi_poll(); host_clear(); key(0, 100); key(0, 0);
    CHECK(out_count(BLE, ON1, 48) == 1 && out_count(USB, ON1, -1) == 0, "AUTO uses BLE without USB");
    host_ready_ports = 6; groove_midi_poll();
    kk_midi_set_ports(KK_PORTS_ALL); host_clear(); key(0, 100); key(0, 0);
    CHECK(out_count(BLE, ON1, 48) == 1 && out_count(USB, ON1, 48) == 1, "ALL sends both");
    kk_midi_set_ports(KK_PORTS_AUTO);
    /* 0.54: SUSTAIN holds released notes until the next key (no CC 64);
     * keys pressed together are held together. */
    host_clear(); tap(KK_BTN_NOTE_REPEAT);
    CHECK(kk.sustain_on && out_count(USB, CC1, 64) == 0 && kk_led_button(KK_BTN_NOTE_REPEAT) == 2, "SUSTAIN on: LED bright, no CC 64");
    host_clear(); key(0, 100); key(4, 100); key(0, 0); key(4, 0); frames(100, 2);
    CHECK(out_count(USB, ON1, 48) == 1 && out_count(USB, ON1, 52) == 1 && out_count(USB, OFF1, -1) == 0, "keys pressed together keep sounding after release");
    key(7, 100);
    CHECK(out_count(USB, OFF1, 48) == 1 && out_count(USB, OFF1, 52) == 1 && out_count(USB, ON1, 55) == 1, "the next key ends them and plays");
    key(9, 100);   /* added while 7 is held: joins */
    key(7, 0); key(9, 0); frames(50, 2);
    CHECK(out_count(USB, OFF1, 55) == 0 && out_count(USB, OFF1, 57) == 0, "a key added while another is held is sustained with it");
    host_clear(); key(0, 100);
    CHECK(out_count(USB, OFF1, 55) == 1 && out_count(USB, OFF1, 57) == 1 && out_count(USB, ON1, 48) == 1, "next phrase replaces both");
    key(0, 0);
    host_clear(); key(0, 100); key(0, 0);
    CHECK(out_count(USB, OFF1, 48) == 1 && out_count(USB, ON1, 48) == 1 && kk_midi_count(0, 48) == 1, "same note again retriggers cleanly");
    host_clear(); pad(2, 100); pad(2, 0);
    CHECK(out_count(USB, OFF1, 48) == 0, "a chord-modifier pad does not end the held notes");
    host_clear(); tap(KK_BTN_NOTE_REPEAT);
    CHECK(!kk.sustain_on && out_count(USB, OFF1, 48) == 1 && kk_midi_count(0, 48) == 0 && kk_led_button(KK_BTN_NOTE_REPEAT) == 1, "SUSTAIN off releases what it held");
    host_clear(); key(0, 100); key(0, 0);
    CHECK(out_count(USB, ON1, 48) == 1 && out_count(USB, OFF1, 48) == 1, "SUSTAIN off: normal release");
    /* A pedal still sends CC 64 (not the button). */
    host_clear(); pedal(127); pedal(0);
    CHECK(out_count(USB, CC1, 64) == 2 && last_out(USB, CC1, 0) == 64, "pedal -> CC 64 127 / 0");
    btn(KK_BTN_NOTE_REPEAT, 1); host_ms += 1000; frames(2, 1); btn(KK_BTN_NOTE_REPEAT, 0);
    CHECK(kk.page == KK_PAGE_MAIN, "long SUSTAIN opens no page");
    tap(KK_BTN_NOTE_REPEAT);
    CHECK(!kk.sustain_on, "sustain off again");
    /* Mode change releases held notes. */
    tap(KK_BTN_NOTE_REPEAT); key(0, 100); key(0, 0); host_clear(); tap(KK_BTN_FX);
    CHECK(out_count(USB, OFF1, 48) == 1 && kk_midi_count(0, 48) == 0, "mode change releases sustained notes");
    tap(KK_BTN_FX); tap(KK_BTN_NOTE_REPEAT);
    /* With ARP on, SUSTAIN latches the arpeggio the same way. */
    tap(KK_BTN_ARP); host_clear(); tap(KK_BTN_NOTE_REPEAT);
    key(0, 100); key(4, 100); frames(60, 2); key(0, 0); key(4, 0);
    host_clear(); frames(500, 2);                      /* 1 s at 1/8 = ~4 steps */
    unsigned held_steps = out_count(USB, ON1, 48) + out_count(USB, ON1, 52);
    CHECK(held_steps >= 3 && out_count(USB, ON1, 48) && out_count(USB, ON1, 52), "released notes keep arpeggiating (%u)", held_steps);
    key(7, 100); key(7, 0); host_clear(); frames(500, 2);
    CHECK(out_count(USB, ON1, 55) >= 2 && out_count(USB, ON1, 48) == 0 && out_count(USB, ON1, 52) == 0, "the next key replaces the held arpeggio");
    key(7, 100); key(9, 100); frames(10, 2); key(7, 0); key(9, 0); host_clear(); frames(500, 2);
    CHECK(out_count(USB, ON1, 55) >= 1 && out_count(USB, ON1, 57) >= 1, "keys pressed together arpeggiate together");
    tap(KK_BTN_NOTE_REPEAT); frames(200, 2); host_clear(); frames(500, 2);
    CHECK(out_count(USB, ON1, -1) == 0, "sustain off: the held arpeggio stops");
    CHECK(kk_midi_count(0, 48) + kk_midi_count(0, 52) + kk_midi_count(0, 55) + kk_midi_count(0, 57) == 0, "no stuck ARP notes");
    tap(KK_BTN_ARP);
}

void test_arp_keys_and_persistence(void)
{
    boot();
    kk_edit_arp(1, 0); tap(KK_BTN_ARP);
    host_clear(); pad(2, 100); key(12, 100);     /* Cm chord, arpeggiated */
    frames(400, 2);
    uint8_t notes[64]; unsigned n; notes_on(USB, ON1, notes, &n);
    CHECK(n >= 3 && notes[0] == 60 && notes[1] == 63 && notes[2] == 67, "chord arp up C Eb G (%u)", n);
    key(12, 0); pad(2, 0); frames(200, 2);
    for (unsigned k = 0; k < 128; ++k) CHECK(!kk_midi_count(0, k), "arp released %u", k);
    /* Settings round trip; octave is not saved. */
    kk_edit_bpm(-20); kk_edit_arp(2, 2); kk_edit_arp(3, -3); kk_edit_arp(4, 10); kk_edit_arp(1, 2); kk_edit_program(5);
    tap(KK_BTN_OCT_UP);
    uint8_t saved[16]; kamotkeys_settings_read(saved);
    CHECK(host_settings_changes > 0, "changes schedule a deferred save");
    groove_screen_init();
    CHECK(kk.bpm == 120 && kk.octave == -2 && kk.pad_octave == 0 && !kk.arp_on, "fresh defaults: keys -2, pads 0");
    CHECK(kamotkeys_settings_apply(saved) == 0, "settings valid");
    CHECK(kk.bpm == 100 && kk.arp_on && kk.arp.division == KK_DIV_8 + 2 && kk.arp.gate == 60 && kk.arp.swing == 10 &&
          kk.arp.direction == KK_DIR_UPDOWN && kk.program == 5 && kk.octave == -2, "restored BPM/ARP, octave back to -2");
    CHECK(host_engine_program() == 5, "patch applied to the engine");
    frames(150, 2);
    CHECK(host_engine_tempo() == 100, "engine tempo follows BPM (%u)", host_engine_tempo());
    /* 0.35: pad LED timing is chosen on the Diagnostics page (E2) and saved. */
    btn(KK_BTN_BT, 1); host_ms += 1600; frames(2, 1); btn(KK_BTN_BT, 0);
    CHECK(kk.page == KK_PAGE_DIAG, "diagnostics open");
    smk37_control_event(SMK37_ENCODER, 1, 3); frames(3, 2);
    CHECK(kk.rgb_timing == 3 && host_rgb_timing == 3, "E2 selects RGB timing 3 (%u)", kk.rgb_timing);
    tap(KK_BTN_BT);
    kamotkeys_settings_read(saved); groove_screen_init(); host_rgb_timing = 0;
    CHECK(kamotkeys_settings_apply(saved) == 0 && kk.rgb_timing == 3 && host_rgb_timing == 3, "RGB timing restored after restart");
    saved[14] = 0; CHECK(kamotkeys_settings_apply(saved) == 0, "older records (byte 14 = 0) still load");
    saved[14] = 99; CHECK(kamotkeys_settings_apply(saved) < 0, "invalid timing rejected");
    saved[14] = 0;
    /* 0.44: pad sensitivity (Patch page E4), saved; older records = 3. */
    CHECK(kk.pad_sensitivity == 3 && kk_pad_curve(64) == 64 && kk_pad_curve(1) == 1 && kk_pad_curve(127) == 127, "default linear");
    tap(KK_BTN_PATCH); smk37_control_event(SMK37_ENCODER, 3, 2); frames(3, 2); tap(KK_BTN_PATCH);
    CHECK(kk.pad_sensitivity == 5, "E4 raises sensitivity (%u)", kk.pad_sensitivity);
    CHECK(kk_pad_curve(40) >= 65 && kk_pad_curve(127) == 127 && kk_pad_curve(1) >= 1, "level 5: light hit 40 -> %u", kk_pad_curve(40));
    {
        unsigned mono = 1;
        for (unsigned lvl = 1; lvl <= KK_PAD_SENSITIVITY_MAX; ++lvl) {
            kk.pad_sensitivity = (uint8_t)lvl;
            for (unsigned v = 2; v <= 127; ++v) mono &= kk_pad_curve(v) >= kk_pad_curve(v - 1);
        }
        kk.pad_sensitivity = 7;
        CHECK(kk_pad_curve(20) >= 65 && kk_pad_curve(10) >= 50 && kk_pad_curve(127) == 127, "0.57 level 7: tap 20 -> %u", kk_pad_curve(20));
        kk.pad_sensitivity = 5;
        CHECK(mono, "every curve is monotonic");
    }
    if (kk.arp_on) tap(KK_BTN_ARP);
    frames(100, 3);
    host_clear(); tap(KK_BTN_FX); pad(1, 40); pad(1, 0); tap(KK_BTN_FX);
    CHECK(last_out(USB, ON10, 0) >> 8 == (int)kk_pad_curve(40), "pad velocity follows the curve (%d)", last_out(USB, ON10, 0) >> 8);
    host_clear(); key(0, 40); key(0, 0);
    CHECK(last_out(USB, ON1, 0) >> 8 == 40, "keys are not affected");
    kamotkeys_settings_read(saved); groove_screen_init();
    CHECK(kamotkeys_settings_apply(saved) == 0 && kk.pad_sensitivity == 5, "pad sensitivity restored after restart");
    saved[15] = 0; CHECK(kamotkeys_settings_apply(saved) == 0 && kk.pad_sensitivity == 3, "pre-0.44 record -> linear");
    saved[15] = 9; CHECK(kamotkeys_settings_apply(saved) < 0, "invalid sensitivity rejected");
    saved[15] = 3;
    saved[1] = 5; CHECK(kamotkeys_settings_apply(saved) < 0, "invalid record rejected");
}

/* Random play with mode/ARP/repeat/octave/scale changes must end silent. */
void test_fuzz(void)
{
    boot();
    srand(7);
    static const uint8_t buttons[] = {KK_BTN_ARP, KK_BTN_NOTE_REPEAT, KK_BTN_SEQ, KK_BTN_SCALE, KK_BTN_FX, KK_BTN_OCT_UP, KK_BTN_OCT_DOWN, KK_BTN_REC};
    for (unsigned i = 0; i < 20000; ++i) {
        unsigned r = (unsigned)rand() % 100;
        if (r < 40) { unsigned k = (unsigned)rand() % 37; key(k, (kk.keys >> k) & 1 ? 0 : 1 + (unsigned)rand() % 127); }
        else if (r < 75) { unsigned p = (unsigned)rand() % 16; pad(p + 1, (kk.pads >> p) & 1 ? 0 : 1 + (unsigned)rand() % 127); }
        else if (r < 85) { unsigned b = buttons[rand() % 8]; btn(b, !((kk.buttons >> b) & 1)); }
        else if (r < 90) fader((unsigned)rand() % 4, (unsigned)rand() % 128);
        else frames(1 + (unsigned)rand() % 20, 3);
    }
    for (unsigned k = 0; k < 37; ++k) key(k, 0);
    for (unsigned p = 1; p <= 16; ++p) pad(p, 0);
    for (unsigned b = 0; b < KK_BUTTONS; ++b) if ((kk.buttons >> b) & 1) btn(b, 0);
    if (kk.arp_on) tap(KK_BTN_ARP);
    if (kk.sustain_on) tap(KK_BTN_NOTE_REPEAT);
    frames(300, 3);
    unsigned stuck = 0;
    for (unsigned k = 0; k < 128; ++k) stuck += kk_midi_count(0, k) + kk_midi_count(9, k);
    CHECK(stuck == 0, "fuzz: %u stuck owner counts", stuck);
    /* External balance per port/channel/note across the whole run. */
    static int bal[3][16][128]; memset(bal, 0, sizeof bal);
    unsigned bad = 0;
    for (unsigned i = 0; i < host_out_n; ++i) {
        struct host_msg *m = &host_out[i];
        unsigned type = m->b[0] & 0xf0, ch = m->b[0] & 15;
        if (type == 0x90 && m->b[2]) { if (bal[m->port][ch][m->b[1]]++) ++bad; }
        else if (type == 0x80 || type == 0x90) { if (--bal[m->port][ch][m->b[1]] < 0) ++bad; }
    }
    for (unsigned p = 0; p < 3; ++p) for (unsigned c = 0; c < 16; ++c) for (unsigned k = 0; k < 128; ++k) if (bal[p][c][k]) ++bad;
    CHECK(host_out_n < HOST_LOG, "log not overflowed (%u)", host_out_n);
    CHECK(bad == 0, "fuzz: %u unbalanced/duplicate external notes", bad);
}

void test_display(void)
{
    host_ms = 1000; groove_midi_init(); groove_screen_init(); groove_midi_poll();
    fader(3, 127);
    frames(150, 2); shot("00_boot_300ms");
    frames(150, 2); shot("00_boot_600ms");
    frames(130, 2); shot("00_boot_860ms");
    frames(200, 2);
    fader(0, 0); fader(1, 0); fader(2, 0);
    frames(100, 2); shot("01_main_idle");
    /* Single note: exactly one highlighted key. */
    key(16, 100); frames(100, 2);
    unsigned green = 0;
    for (unsigned x = 0; x < 240; ++x) { uint16_t c = host_fb[(141 + 55) * 240 + x]; if (c == 0x474e) ++green; }
    shot("06_main_single_note");
    key(16, 0);
    /* G7 chord: four sounding notes, all on screen, root cyan. */
    pad(9, 100); key(19, 100); frames(100, 2);
    shot("07_main_chord_G7");
    key(19, 0);
    key(5, 100); frames(15, 2); frames(6, 0); shot("08_piano_transition_mid"); frames(60, 2); shot("09_piano_transition_end");
    key(5, 0); pad(9, 0);
    tap(KK_BTN_ARP); kk_edit_program(3); frames(80, 2); tap(KK_BTN_ARP);
    tap(KK_BTN_PATCH); frames(120, 2); shot("10_patch_page"); tap(KK_BTN_PATCH);
    btn(KK_BTN_ARP, 1); host_ms += 700; frames(2, 1); btn(KK_BTN_ARP, 0); frames(120, 2); shot("11_arp_page"); tap(KK_BTN_ARP);
    tap(KK_BTN_BT); frames(120, 2); shot("12_bluetooth_menu"); tap(KK_BTN_BT);
    btn(KK_BTN_BT, 1); host_ms += 1600; frames(2, 1); btn(KK_BTN_BT, 0); frames(120, 2); shot("13_diagnostics"); tap(KK_BTN_BT);
    fader(3, 8); frames(120, 2); shot("14_main_dimmed"); fader(3, 127); frames(120, 2);
    show_on(); fader(0, 70);
    for (unsigned p = 0; p < 8; ++p) {
        frames(60, 5); char name[32]; snprintf(name, sizeof name, "15_lightshow_%u", kk.anim_pattern + 1); shot(name);
        btn(KK_BTN_FADER_BANK, 0); btn(KK_BTN_FADER_BANK, 1);
    }
    show_off();
}

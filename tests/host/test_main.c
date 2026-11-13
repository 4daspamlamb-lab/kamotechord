/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: drive the real performance/display/router code with
 * synthetic physical events and check the MIDI it produces. Screenshots of
 * the LCD are written as PPM files (converted to PNG by run.sh). */
#include "stubs.h"
#include "kk_perform.h"
#include "kk_midi.h"
#include "kk_led.h"
#include "kk_theory.h"
#include "instrument.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures, checks;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

void kamotkeys_settings_read(uint8_t d[16]);
void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n);
int kamotkeys_settings_apply(const uint8_t d[16]);

/* ------------------------------------------------------------- driving */
static void frames(unsigned n, unsigned step_ms)
{
    for (unsigned i = 0; i < n; ++i) { host_ms += step_ms; groove_midi_poll(); groove_screen_control(); }
}
static void key(unsigned id, unsigned vel) { smk37_control_event(SMK37_KEY, id, (int)vel); groove_midi_poll(); }
static void pad(unsigned n, unsigned vel) { smk37_control_event(SMK37_PAD, n - 1, (int)vel); groove_midi_poll(); }
static void btn(unsigned id, unsigned down) { host_ms += 1; smk37_control_event(SMK37_BUTTON, id, (int)down); groove_midi_poll(); }
static void tap(unsigned id) { btn(id, 1); btn(id, 0); }
static void fader(unsigned id, unsigned v) { smk37_control_event(SMK37_FADER, id, (int)v); }
static void pedal(unsigned v) { smk37_control_event(SMK37_PEDAL, 0, (int)v); groove_midi_poll(); }
static void boot(void)
{
    host_ms = 1000; groove_midi_init(); groove_screen_init(); groove_midi_poll();
    fader(0, 0); fader(1, 0); fader(2, 0); fader(3, 127);
    frames(800, 2);            /* finish the Aurora boot and its LED fade */
    kk_set_octave(0);          /* most checks below use the 0.30 key octave */
    host_clear();
}
/* LED output fades out (~140 ms time constant): let it settle. */
static void settle(void) { frames(400, 3); }
/* External messages on one port matching status (high nibble + channel). */
static unsigned out_count(unsigned port, unsigned status, int data1)
{
    unsigned n = 0;
    for (unsigned i = 0; i < host_out_n; ++i)
        if (host_out[i].port == port && host_out[i].b[0] == status && (data1 < 0 || host_out[i].b[1] == data1)) ++n;
    return n;
}
static int last_out(unsigned port, unsigned status, unsigned skip)
{
    for (unsigned i = host_out_n; i--;)
        if (host_out[i].port == port && host_out[i].b[0] == status) { if (!skip--) return host_out[i].b[1] | host_out[i].b[2] << 8; }
    return -1;
}
static void notes_on(unsigned port, unsigned status, uint8_t *notes, unsigned *n)
{
    *n = 0;
    for (unsigned i = 0; i < host_out_n; ++i)
        if (host_out[i].port == port && host_out[i].b[0] == status) notes[(*n)++] = host_out[i].b[1];
}
enum { USB = 1, BLE = 2, ON1 = 0x90, OFF1 = 0x80, ON10 = 0x99, OFF10 = 0x89, CC1 = 0xB0, CC15 = 0xBE };
/* Messages posted to the internal synth matching status and data1. */
static unsigned local_count(unsigned status, int data1)
{
    unsigned n = 0;
    for (unsigned i = 0; i < host_local_n; ++i)
        if (host_local[i].b[0] == status && (data1 < 0 || host_local[i].b[1] == data1)) ++n;
    return n;
}

/* -------------------------------------------------------- screenshots */
static void shot(const char *name)
{
    char path[64]; snprintf(path, sizeof path, "%s.ppm", name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (unsigned i = 0; i < 240 * 240; ++i) {
        uint16_t c = host_fb[i];
        unsigned char rgb[3] = {(unsigned char)((c >> 11) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    /* Pad LED strip alongside: 8x2 swatches (final, after Fader 4). */
    snprintf(path, sizeof path, "%s_pads.ppm", name);
    f = fopen(path, "wb");
    fprintf(f, "P6\n%u %u\n255\n", 8 * 24, 2 * 24);
    for (unsigned y = 0; y < 48; ++y) for (unsigned x = 0; x < 192; ++x) {
        unsigned p = (y / 24) * 8 + x / 24, edge = x % 24 < 2 || y % 24 < 2;
        uint32_t c = edge ? 0x202020 : host_rgb[p];
        /* Show the LED output scaled up from the 160 peak for visibility. */
        unsigned r = ((c >> 16) & 255) * 255 / 160, g = ((c >> 8) & 255) * 255 / 160, b = (c & 255) * 255 / 160;
        unsigned char rgb[3] = {(unsigned char)(r > 255 ? 255 : r), (unsigned char)(g > 255 ? 255 : g), (unsigned char)(b > 255 ? 255 : b)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

/* ------------------------------------------------------------- tests */
static void test_theory(void)
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

static void test_buttons(void)
{
    boot();
    static const struct { unsigned id, cc; } momentary[] = {
        {KK_BTN_PLAY, 20}, {KK_BTN_GLOBE, 21}, {KK_BTN_FADER_BANK, 22}, {KK_BTN_PARA, 23}, {KK_BTN_RIGHT, 27}};
    /* HOME (LEFT) = Undo on release (0.51: it doubles as a shift for PARA). */
    host_clear(); btn(KK_BTN_LEFT, 1);
    CHECK(out_count(USB, CC15, 24) == 0 && kk_led_button(KK_BTN_LEFT) == 2, "HOME press: lit, Undo waits for release");
    btn(KK_BTN_LEFT, 0);
    CHECK(out_count(USB, CC15, 24) == 2 && last_out(USB, CC15, 0) == 24 && last_out(USB, CC15, 1) == (24 | 127 << 8), "HOME release -> Undo 127/0");
    for (unsigned i = 0; i < 5; ++i) {
        host_clear(); btn(momentary[i].id, 1);
        CHECK(out_count(USB, CC15, momentary[i].cc) == 1 && last_out(USB, CC15, 0) == (int)(momentary[i].cc | 127 << 8), "press cc %u", momentary[i].cc);
        CHECK(kk_led_button(momentary[i].id) == 2, "momentary bright %u", momentary[i].id);
        btn(momentary[i].id, 0);
        CHECK(out_count(USB, CC15, momentary[i].cc) == 2 && last_out(USB, CC15, 0) == (int)momentary[i].cc, "release cc %u", momentary[i].cc);
        CHECK(kk_led_button(momentary[i].id) == 1, "momentary dim %u", momentary[i].id);
        CHECK(out_count(BLE, CC15, -1) == 0, "AUTO: no duplicate on BLE while USB is ready");
    }
    /* KNOB BANK (0.31): Session Record, momentary CC25 127/0, LED momentary. */
    host_clear(); btn(KK_BTN_KNOB_BANK, 1);
    CHECK(last_out(USB, CC15, 0) == (25 | 127 << 8) && kk_led_button(KK_BTN_KNOB_BANK) == 2, "session record press");
    btn(KK_BTN_KNOB_BANK, 0);
    CHECK(out_count(USB, CC15, 25) == 2 && last_out(USB, CC15, 0) == 25 && kk_led_button(KK_BTN_KNOB_BANK) == 1, "session record release");
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
    tap(KK_BTN_REC); CHECK(kk.page == KK_PAGE_MAIN, "REC does not open BT");
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

static void test_key_scale(void)
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

static void test_velocity_faders(void)
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
    /* Fader 4 brightness. */
    fader(3, 0); settle();
    unsigned lit = 0; for (unsigned p = 0; p < 16; ++p) lit |= host_rgb[p];
    CHECK(lit == 0, "Fader 4 at 0: pads off");
    pad(5, 100); frames(2, 2);
    CHECK(host_rgb[4] == 0, "Fader 4 at 0: pressed pad still dark"); pad(5, 0);
    fader(3, 127); pad(5, 100); frames(2, 2); uint32_t full = host_rgb[4];
    CHECK(((full >> 16) & 255) == KK_RGB_PEAK, "instant attack (%06x)", full);
    fader(3, 64); settle(); uint32_t half = host_rgb[4];
    CHECK(((full >> 16) & 255) == KK_RGB_PEAK && ((half >> 16) & 255) * 2 >= KK_RGB_PEAK - 4 && ((half >> 16) & 255) * 2 <= KK_RGB_PEAK + 4, "brightness scales (%06x %06x)", full, half);
    pad(5, 0); frames(10, 3);
    unsigned released = (host_rgb[4] >> 16) & 255;
    CHECK(released >= 8 && released <= 12, "release returns to idle at once (%u)", released);
    settle();
    unsigned idle = (host_rgb[4] >> 16) & 255;
    CHECK(idle >= 8 && idle <= 12, "spare idle 12.5%% = half of chord 25%% (%u)", idle);
    fader(3, 10); CHECK(kk.screen_dim, "LCD dims at <= 10%%"); fader(3, 127); CHECK(!kk.screen_dim, "LCD normal");
    /* Every channel sent to the pads is a multiple of 4, in every mode and at
     * every Fader 4 position (values with bit 0/1 set flash on hardware). */
    {
        static const uint8_t modes[] = {KK_BTN_SEQ, KK_BTN_SCALE, KK_BTN_FX, KK_BTN_CHORD};
        unsigned odd = 0;
        for (unsigned m = 0; m <= 4; ++m) {
            for (unsigned v = 0; v <= 127; v += 9) {
                fader(3, v); pad(1, 90); pad(5, 90); frames(3, 3); pad(1, 0); pad(5, 0); frames(3, 3);
                for (unsigned p = 0; p < 16; ++p) odd |= host_rgb[p] & 0x030303;
            }
            if (m < 4) tap(modes[m]);
        }
        tap(KK_BTN_CHORD); fader(3, 127);
        CHECK(!odd && kk.mode == KK_MODE_NORMAL, "all pad values are multiples of 4 (%06x)", odd);
    }
    /* Idle chord pads keep their hue (the blue MIN pad is clearly blue). */
    settle();
    CHECK((host_rgb[1] & 255) > 2 * ((host_rgb[1] >> 16) & 255) && (host_rgb[1] & 255) >= 15, "MIN pad idle is blue (%06x)", host_rgb[1]);
    /* Fader 1 is animation speed while SEQ REC is held. */
    fader(0, 127); btn(KK_BTN_REC, 1); frames(3, 5);
    CHECK(kk_velocity_override() == 0 && kk.anim_speed == 2000, "REC: fader 1 = speed (%u)", kk.anim_speed);
    fader(0, 0); frames(2, 5); CHECK(kk.anim_speed == 1000, "0%% = half max");
    fader(0, 14); frames(2, 5); CHECK(kk.anim_speed < 80, "1-10%% very slow (%u)", kk.anim_speed);
    host_clear(); pad(5, 100); pad(5, 0); CHECK(host_out_n == 0, "pads silent during light show");
    btn(KK_BTN_REC, 0); fader(0, 127);
    CHECK(kk_velocity_override() == 127, "velocity restored after REC release");
    fader(0, 0);
}

static void test_pad_modes(void)
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

static void test_spare_and_refcount(void)
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

static void test_arp_keys_and_persistence(void)
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
        for (unsigned lvl = 1; lvl <= 5; ++lvl) {
            kk.pad_sensitivity = (uint8_t)lvl;
            for (unsigned v = 2; v <= 127; ++v) mono &= kk_pad_curve(v) >= kk_pad_curve(v - 1);
        }
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
static void test_fuzz(void)
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

static void test_display(void)
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
    btn(KK_BTN_REC, 1); fader(0, 70);
    for (unsigned p = 0; p < 8; ++p) {
        frames(40, 5); char name[32]; snprintf(name, sizeof name, "15_lightshow_%u", kk.anim_pattern + 1); shot(name);
        btn(KK_BTN_REC, 0); btn(KK_BTN_REC, 1);
    }
    btn(KK_BTN_REC, 0);
}

/* ------------------------------------------------------------ 0.31 */
static void host_in(uint8_t a, uint8_t b, uint8_t c) { uint8_t m[3] = {a, b, c}; fm1_app_midi_received(1, m, 3); }
static void test_031(void)
{
    host_ms = 1000; groove_midi_init(); groove_screen_init(); groove_midi_poll();
    CHECK(kk.octave == -2 && kk.pad_octave == 0 && kk_active_octave() == -2, "power-on: keys -2, pads 0");
    fader(0, 0); fader(1, 0); fader(2, 0); fader(3, 127); frames(800, 2); host_clear();
    key(12, 100); CHECK(last_out(USB, ON1, 0) == (36 | 100 << 8), "second C at octave -2 = C2 (36)"); key(12, 0);
    tap(KK_BTN_FX); CHECK(kk_active_octave() == 0, "Drum Mode shows the pad octave");
    host_clear(); pad(1, 100); pad(1, 0); CHECK(out_count(USB, ON10, 36) == 1, "drum pad 1 still 36 at power-on");
    tap(KK_BTN_OCT_UP); CHECK(kk.pad_octave == 1 && kk.octave == -2, "OCT in Drum Mode moves the pads only");
    tap(KK_BTN_OCT_DOWN); tap(KK_BTN_FX);
    kk_set_octave(0);

    /* Chord LOCK (pad 13). */
    host_clear(); pad(2, 100); pad(9, 100); pad(13, 100); pad(13, 0); pad(2, 0); pad(9, 0);
    CHECK(kk.chord_lock && kk.chord_mods == (KK_MOD_MIN | KK_MOD_7), "LOCK keeps MIN+7 after release (%x)", kk.chord_mods);
    key(12, 100); uint8_t notes[64]; unsigned n; notes_on(USB, ON1, notes, &n);
    CHECK(n == 4 && notes[0] == 60 && notes[1] == 63 && notes[2] == 67 && notes[3] == 70, "locked Cm7 from one key");
    key(12, 0);
    pad(9, 100); pad(9, 0); CHECK(kk.chord_mods == KK_MOD_MIN, "locked: tapping the active extension clears it");
    pad(10, 100); pad(10, 0); CHECK(kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "and a new one turns on");
    pad(1, 100); pad(1, 0); CHECK(kk.chord_mods == (KK_MOD_MAJ | KK_MOD_MAJ7), "locked: MAJ replaces MIN (%x)", kk.chord_mods);
    pad(4, 100); pad(4, 0); CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_MAJ7), "SUS4 replaces MAJ");
    pad(11, 100); pad(11, 0); CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_6), "6 replaces MAJ7");
    pad(9, 100); pad(12, 100); pad(12, 0); pad(9, 0);
    CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_7 | KK_MOD_9), "7 and 9 pressed together combine (%x)", kk.chord_mods);
    pad(2, 100); pad(2, 0); pad(10, 100); pad(10, 0);
    CHECK(kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "back to MIN + MAJ7");
    settle(); uint32_t locked = host_rgb[12];
    CHECK((locked & 0xff) > KK_RGB_PEAK * 3 / 4, "LOCK pad bright (%06x)", locked);
    pad(13, 100); pad(13, 0); CHECK(!kk.chord_lock && !kk.chord_mods, "LOCK off releases the latch");
    settle(); CHECK((host_rgb[12] & 0xff) * 2 < (locked & 0xff), "LOCK pad idle is clearly dimmer (%06x)", host_rgb[12]);
    /* 0.42 palette: MAJ warm amber, MIN blue, DIM purple, SUS4 teal. */
    CHECK((host_rgb[0] >> 16) > (host_rgb[0] & 255) * 4, "MAJ idle warm (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] & 255) > ((host_rgb[1] >> 16) & 255) * 3, "MIN idle blue (%06x)", host_rgb[1]);
    CHECK((host_rgb[2] & 255) > ((host_rgb[2] >> 8) & 255) * 3 && (host_rgb[2] >> 16) > ((host_rgb[2] >> 8) & 255) * 2, "DIM idle purple (%06x)", host_rgb[2]);
    CHECK(((host_rgb[3] >> 8) & 255) > (host_rgb[3] >> 16) * 4, "SUS4 idle teal (%06x)", host_rgb[3]);

    /* 0.34: playing keys and chords in Normal Mode changes no pad colour at
     * all, so KamoteKeys sends the pads no frame on a key press. */
    settle(); host_rgb_changes = 0;
    for (unsigned k = 0; k < 37; k += 3) { key(k, 100); frames(5, 3); key(k, 0); frames(5, 3); }
    pad(9, 100); frames(3, 3); unsigned after_pad = host_rgb_changes;
    for (unsigned k = 0; k < 37; k += 5) { key(k, 90); frames(5, 3); key(k, 0); frames(5, 3); }
    pad(9, 0); frames(3, 3);
    CHECK(after_pad == 1 && host_rgb_changes == 2, "key presses change no pad colour (%u, %u)", after_pad, host_rgb_changes);

    /* Spare pads are not lit by chords from the keys (the 0.30 'random lights'). */
    pad(1, 100); key(31, 100); settle();   /* G5 major: G B D = 79 83 86 -> 86 is spare pad 7's note */
    CHECK(kk_midi_count(0, 86) == 1 && !kk_pad_lit(6), "spare pad 7 stays idle while a chord sounds 86");
    key(31, 0); pad(1, 0);

    /* Clip Mode. */
    host_clear(); tap(KK_BTN_CHORD);
    CHECK(kk.mode == KK_MODE_CLIP && kk_led_button(KK_BTN_CHORD) == 2, "CHORD -> Clip Mode");
    CHECK(out_count(USB, 0xBF, KK_CLIP_SYNC_CC) == 1, "entering Clip Mode asks Live for the state");
    host_clear(); pad(3, 110);
    CHECK(out_count(USB, 0x9F, 38) == 1 && host_local_n == 0, "pad 3 -> note 38 ch16, never the synth");
    pad(3, 0); CHECK(out_count(USB, 0x8F, 38) == 1, "release -> note off");
    host_clear(); tap(KK_BTN_ARP); pad(3, 100); frames(300, 2); pad(3, 0); tap(KK_BTN_ARP);
    CHECK(out_count(USB, 0x9F, 38) == 1, "ARP does not touch clip launches");
    host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBF, KK_CLIP_STATE_CC + 0, KK_CLIP_PLAYING); host_in(0xBF, KK_CLIP_STATE_CC + 1, KK_CLIP_RECORDING);
    host_in(0xBF, KK_CLIP_STATE_CC + 2, KK_CLIP_STOPPED); host_in(0xBF, KK_CLIP_STATE_CC + 3, KK_CLIP_QUEUED);
    CHECK(kk_midi_host_protocol() == 1 && kk_midi_clip_state(1) == KK_CLIP_RECORDING, "feedback recorded");
    CHECK(host_local_n == 0, "channel 16 feedback never reaches the synth");
    settle();
    CHECK(((host_rgb[0] >> 8) & 255) >= KK_RGB_PEAK * 9 / 10 && (host_rgb[0] >> 16) < 4, "playing = bright green (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] >> 16) >= KK_RGB_PEAK * 9 / 10 && (host_rgb[1] & 0xffff) == 0, "recording = bright red (%06x)", host_rgb[1]);
    CHECK((host_rgb[2] & 255) >= KK_RGB_PEAK / 4 && (host_rgb[2] & 255) <= KK_RGB_PEAK / 2 && (host_rgb[2] >> 16) <= 8, "stopped = dim blue (%06x)", host_rgb[2]);
    CHECK(((host_rgb[4] >> 16) & 255) <= 8, "empty/unknown = very dim (%06x)", host_rgb[4]);
    unsigned a = 0, b = 255;
    for (unsigned i = 0; i < 20; ++i) { frames(10, 5); unsigned r = (host_rgb[3] >> 16) & 255; if (r > a) a = r; if (r < b) b = r; }
    CHECK(a >= KK_RGB_PEAK * 9 / 10 && b <= KK_RGB_PEAK / 4, "queued blinks amber (%u..%u)", b, a);
    frames(40, 5); shot("16_clip_mode");
    host_ready_ports = 0; frames(5, 2);
    CHECK(kk_midi_host_protocol() == 0 && kk_midi_clip_state(0) == KK_CLIP_UNKNOWN, "disconnect forgets stale clip state");
    host_clear(); host_ready_ports = 4; frames(5, 2);
    CHECK(out_count(BLE, 0xBF, KK_CLIP_SYNC_CC) == 1, "reconnect (BLE) re-requests the state");
    host_ready_ports = 6; frames(5, 2);
    /* 0.50: first black key (C#) held + clip = OVERDUB (note 52+), second
     * (D#) = DELETE (note 68+); the keys themselves play nothing. */
    host_clear(); key(1, 100);
    CHECK(host_out_n == 0 && host_local_n == 0 && kk.clip_action == 1, "C# in Clip Mode: OVERDUB held, no note");
    frames(20, 5); shot("17_clip_overdub");
    pad(3, 100); pad(3, 0);
    CHECK(out_count(USB, 0x9F, 52 + 2) == 1 && out_count(USB, 0x8F, 52 + 2) == 1 && out_count(USB, 0x9F, 38) == 0, "OVERDUB + pad 3 -> note 54");
    host_clear(); key(3, 100); pad(10, 100); key(1, 0); pad(10, 0);
    CHECK(kk.clip_action == 2 && out_count(USB, 0x9F, 68 + 9) == 1 && out_count(USB, 0x8F, 68 + 9) == 1, "DELETE wins + pad 10 -> note 77, its own off");
    key(3, 0); CHECK(kk.clip_action == 0, "keys released: launch again");
    /* 0.55/0.56: third black key (F#) held = SELECT (note 84+); fourth (G#) =
     * launch scene; top G# / A# = scene up / down (ch 15 CC 30 / 29 / 28). */
    host_clear(); key(6, 100); pad(5, 100); pad(5, 0); key(6, 0);
    CHECK(out_count(USB, 0x9F, 84 + 4) == 1 && out_count(USB, 0x90, -1) == 0, "F# + pad 5 -> select note 88, F# silent");
    host_clear(); key(34, 100); key(34, 0); key(32, 100); key(32, 0); key(8, 100); key(8, 0);
    CHECK(out_count(USB, CC15, KK_CC_SCENE_DOWN) == 2 && out_count(USB, CC15, KK_CC_SCENE_UP) == 2 &&
          out_count(USB, CC15, KK_CC_SCENE_LAUNCH) == 2 && out_count(USB, 0x90, -1) == 0, "top A# / G# and low G# -> scene down / up / launch, no notes");
    host_clear(); key(30, 100); key(30, 0);
    CHECK(out_count(USB, 0x90, 48 + 30) == 1, "top F# plays its note again in Clip Mode");
    host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_in(0xBF, KK_HOST_SCENE_CC, 4);
    CHECK(kk_midi_host_scene() == 4, "grid follows the script's scene offset");
    frames(20, 5); shot("18_clip_scene5");
    host_in(0xBF, KK_HOST_SCENE_CC, 0);
    host_clear(); pad(3, 100); pad(3, 0); CHECK(out_count(USB, 0x9F, 38) == 1, "plain launch after release");
    host_clear(); key(1, 100); tap(KK_BTN_CHORD); key(1, 0);
    CHECK(host_out_n == 0 || out_count(USB, 0x90, -1) == 0, "OVERDUB key held across leaving Clip Mode stays silent");
    key(1, 100); key(1, 0); CHECK(out_count(USB, 0x90, 49) == 1, "outside Clip Mode C# plays its note");
    tap(KK_BTN_CHORD);
    tap(KK_BTN_CHORD); CHECK(kk.mode == KK_MODE_NORMAL, "CHORD again -> Normal");

    /* Move-style: host notes light Pad Scale / Drum pads. */
    tap(KK_BTN_FX); settle(); uint32_t idle = host_rgb[0];
    host_in(0x99, 36, 100); frames(3, 3);
    CHECK(kk_pad_lit(0) && host_rgb[0] != idle && (host_rgb[0] >> 16) == KK_RGB_PEAK, "Ableton kick lights pad 1");
    host_in(0x89, 36, 0); frames(10, 3);
    CHECK(!kk_pad_lit(0) && host_rgb[0] == idle, "then returns to idle");
    tap(KK_BTN_FX);
    kk_midi_set_local(0); host_clear(); host_in(0x90, 60, 100);
    CHECK(host_local_n == 0, "internal synth off: host notes stay silent"); host_in(0x80, 60, 0); kk_midi_set_local(1);

    /* 0.42: the light show crossfades in (no one-frame jump = no flash). */
    {
        uint32_t before = host_rgb[0]; unsigned steps = 0, prev = before;
        btn(KK_BTN_REC, 1);
        for (unsigned i = 0; i < 10; ++i) { frames(1, 25); if (host_rgb[0] != prev) ++steps; prev = host_rgb[0]; }
        CHECK(steps >= 4, "light show fades in over several frames (%u)", steps);
        btn(KK_BTN_REC, 0); frames(20, 25);
    }
    /* SEQ REC light show keeps refreshing while the LCD is redrawing. */
    btn(KK_BTN_REC, 1); fader(0, 127);
    uint32_t seen[8]; unsigned changes = 0;
    for (unsigned i = 0; i < 8; ++i) { frames(10, 4); seen[i] = host_rgb[5]; if (i && seen[i] != seen[i - 1]) ++changes; }
    CHECK(changes >= 6, "pads animate at ~30 fps (%u/7 samples changed)", changes);
    uint32_t before = kk.anim_phase; frames(100, 5);  /* 500 ms at 2 Hz = one full cycle */
    uint32_t moved = kk.anim_phase - before;
    CHECK(moved > 0xe0000000U || moved < 0x20000000U, "2 Hz show completes a cycle in 500 ms (%08x)", moved);
    btn(KK_BTN_REC, 0);
}

/* 0.54: ARP follows Ableton's tempo (Remote Script CC 102/103 on ch 16). */
static void test_live_tempo(void)
{
    boot();
    CHECK(kk_tempo_x10() == 1200, "own tempo 120 without Live");
    host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBF, KK_HOST_TEMPO_MSB_CC, 985 >> 7); host_in(0xBF, KK_HOST_TEMPO_LSB_CC, 985 & 127);
    CHECK(kk_tempo_x10() == 985 && kk_tempo_bpm() == 99, "Live tempo 98.5 has priority (%u)", kk_tempo_x10());
    kk_edit_bpm(10); CHECK(kk.bpm == 120 && kk_tempo_x10() == 985, "tempo knob ignored while Live sets it");
    tap(KK_BTN_ARP); host_clear(); key(0, 100); key(4, 100);
    frames(1218, 1);                     /* 4 steps of 1/8 at 98.5 BPM = 1218 ms */
    unsigned on = out_count(USB, ON1, -1);
    key(0, 0); key(4, 0); tap(KK_BTN_ARP); frames(100, 2);
    CHECK(on == 4 || on == 5, "ARP steps at Live's tempo (%u in 1218 ms)", on);
    host_ready_ports = 0; frames(5, 2);
    CHECK(kk_tempo_x10() == 1200, "Live gone: back to the own tempo");
    host_ready_ports = 6; frames(5, 2);
    host_in(0xBF, KK_HOST_TEMPO_MSB_CC, 1400 >> 7); host_in(0xBF, KK_HOST_TEMPO_LSB_CC, 1400 & 127);
    CHECK(kk_tempo_x10() == 1200, "tempo without the script's hello is ignored");
}

int main(void)
{
    test_theory();
    test_buttons();
    test_key_scale();
    test_velocity_faders();
    test_pad_modes();
    test_spare_and_refcount();
    test_arp_keys_and_persistence();
    test_fuzz();
    test_display();
    test_031();
    test_live_tempo();
    printf("%u checks, %u failures\n", checks, failures);
    return failures != 0;
}

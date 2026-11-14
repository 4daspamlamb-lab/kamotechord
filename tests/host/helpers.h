/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Shared helpers for the KamoteKeys host tests: drive the panel (key, pad,
 * btn, tap, fader, frames, boot, settle), read what was sent (out_count,
 * last_out, notes_on) and save LCD screenshots (shot). */
#ifndef KK_TEST_HELPERS_H
#define KK_TEST_HELPERS_H
#include "stubs.h"
#include "kk_perform.h"
#include "kk_midi.h"
#include "kk_led.h"
#include "kk_theory.h"
#include "instrument.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern unsigned failures, checks;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

void kamotkeys_settings_read(uint8_t d[16]);
void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n);
int kamotkeys_settings_apply(const uint8_t d[16]);

/* ------------------------------------------------------------- driving */
static inline void frames(unsigned n, unsigned step_ms)
{
    for (unsigned i = 0; i < n; ++i) { host_ms += step_ms; groove_midi_poll(); groove_screen_control(); }
}
static inline void key(unsigned id, unsigned vel) { smk37_control_event(SMK37_KEY, id, (int)vel); groove_midi_poll(); }
static inline void pad(unsigned n, unsigned vel) { smk37_control_event(SMK37_PAD, n - 1, (int)vel); groove_midi_poll(); }
static inline void btn(unsigned id, unsigned down) { host_ms += 1; smk37_control_event(SMK37_BUTTON, id, (int)down); groove_midi_poll(); }
static inline void tap(unsigned id) { btn(id, 1); btn(id, 0); }
/* 0.59: KNOB BANK + FADER BANK held together = light show. */
static inline void show_on(void) { btn(KK_BTN_KNOB_BANK, 1); btn(KK_BTN_FADER_BANK, 1); }
static inline void show_off(void) { btn(KK_BTN_FADER_BANK, 0); btn(KK_BTN_KNOB_BANK, 0); }
static inline void fader(unsigned id, unsigned v) { smk37_control_event(SMK37_FADER, id, (int)v); }
static inline void pedal(unsigned v) { smk37_control_event(SMK37_PEDAL, 0, (int)v); groove_midi_poll(); }
static inline void boot(void)
{
    host_ms = 1000; groove_midi_init(); groove_screen_init(); groove_midi_poll();
    fader(0, 0); fader(1, 0); fader(2, 0); fader(3, 127);
    frames(800, 2);            /* finish the Aurora boot and its LED fade */
    kk_set_octave(0);          /* most checks below use the 0.30 key octave */
    host_clear();
}
/* LED output fades out (~140 ms time constant): let it settle. */
static inline void settle(void) { frames(400, 3); }
/* External messages on one port matching status (high nibble + channel). */
static inline unsigned out_count(unsigned port, unsigned status, int data1)
{
    unsigned n = 0;
    for (unsigned i = 0; i < host_out_n; ++i)
        if (host_out[i].port == port && host_out[i].b[0] == status && (data1 < 0 || host_out[i].b[1] == data1)) ++n;
    return n;
}
static inline int last_out(unsigned port, unsigned status, unsigned skip)
{
    for (unsigned i = host_out_n; i--;)
        if (host_out[i].port == port && host_out[i].b[0] == status) { if (!skip--) return host_out[i].b[1] | host_out[i].b[2] << 8; }
    return -1;
}
static inline void notes_on(unsigned port, unsigned status, uint8_t *notes, unsigned *n)
{
    *n = 0;
    for (unsigned i = 0; i < host_out_n; ++i)
        if (host_out[i].port == port && host_out[i].b[0] == status) notes[(*n)++] = host_out[i].b[1];
}
enum { USB = 1, BLE = 2, ON1 = 0x90, OFF1 = 0x80, ON10 = 0x99, OFF10 = 0x89, CC1 = 0xB0, CC15 = 0xBE };
/* Messages posted to the internal synth matching status and data1. */
static inline unsigned local_count(unsigned status, int data1)
{
    unsigned n = 0;
    for (unsigned i = 0; i < host_local_n; ++i)
        if (host_local[i].b[0] == status && (data1 < 0 || host_local[i].b[1] == data1)) ++n;
    return n;
}

/* -------------------------------------------------------- screenshots */
static inline void shot(const char *name)
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

static inline void host_in(uint8_t a, uint8_t b, uint8_t c) { uint8_t m[3] = {a, b, c}; fm1_app_midi_received(1, m, 3); }

/* Test groups, one per file (called from test_main.c). */
void test_theory(void);
void test_buttons(void);
void test_key_scale(void);
void test_velocity_faders(void);
void test_pad_modes(void);
void test_spare_and_refcount(void);
void test_arp_keys_and_persistence(void);
void test_fuzz(void);
void test_display(void);
void test_031(void);
void test_ripple(void);
void test_step_mode(void);
void test_live_tempo(void);

void test_bluetooth(void);
#endif

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys LCD interface and control-task entry points (module
 * "kamotkeys_display"). It keeps the file name app/screen.c because the SDK
 * build script (sdk/Build.ps1, hash-locked by sdk/manifest.json) compiles
 * this SDK entry unit natively (-fno-lto, -Oz).
 *
 * Rendering model: the screen is a fixed set of rectangular widgets. Each
 * frame computes a cheap signature per widget; only changed widgets are
 * redrawn, one 240x16 band at a time into a 7.5 KiB buffer that is sent by
 * nonblocking DMA (fm1_screen_submit). Nothing here waits for the LCD, so
 * keys, pads, ARP and MIDI keep running while the display catches up. */
#include "kamotkeys/kk_perform.h"
#include "kamotkeys/kk_midi.h"
#include "kamotkeys/kk_led.h"
#include "instrument.h"
#include "fm_patch.h"
#include "version.h"
#include "fm1_engine_control.h"
#include <string.h>


/* ---------------------------------------------------------------- colour */
#define C565(r, g, b) ((uint16_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | ((b) >> 3)))
enum {
    BG = C565(6, 9, 16), PANEL = C565(18, 26, 40), PANEL2 = C565(28, 38, 56),
    TEXT = C565(226, 234, 242), DIM = C565(112, 124, 142), FAINT = C565(52, 62, 80),
    TEAL = C565(0, 196, 180), CYAN = C565(0, 226, 255), GREEN = C565(70, 232, 120),
    ORANGE = C565(255, 150, 40), AMBER = C565(255, 196, 60), RED = C565(255, 70, 70),
    WHITE_KEY = C565(214, 220, 228), BLACK_KEY = C565(22, 26, 34), KEY_LINE = C565(120, 128, 140)
};
static uint16_t c565(uint32_t rgb)
{
    return C565((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}
static uint16_t blend565(uint16_t a, uint16_t b, unsigned t /* 0..255 toward b */)
{
    unsigned ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((((ar * (255 - t) + br * t) / 255) << 11) | (((ag * (255 - t) + bg * t) / 255) << 5) |
                      ((ab * (255 - t) + bb * t) / 255));
}

/* ----------------------------------------------------------------- font */
/* 5x7 ASCII 32..126, column major, bit 0 at the top. */
static const uint8_t font[95][5] = {
    {0,0,0,0,0},{0,0,0x5f,0,0},{0,7,0,7,0},{0x14,0x7f,0x14,0x7f,0x14},{0x24,0x2a,0x7f,0x2a,0x12},
    {0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1c,0x22,0x41,0},{0,0x41,0x22,0x1c,0},
    {8,0x2a,0x1c,0x2a,8},{8,8,0x3e,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},
    {0x20,0x10,8,4,2},{0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},
    {6,0x49,0x49,0x29,0x1e},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,8,0},{2,1,0x51,9,6},{0x32,0x49,0x79,0x41,0x3e},{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,1,1},{0x3e,0x41,0x41,0x51,0x32},
    {0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
    {0x7f,2,4,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},
    {0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
    {0x7f,0x20,0x18,0x20,0x7f},{0x63,0x14,8,0x14,0x63},{3,4,0x78,4,3},{0x61,0x51,0x49,0x45,0x43},{0,0x7f,0x41,0x41,0},
    {2,4,8,0x10,0x20},{0,0x41,0x41,0x7f,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},{0,1,2,4,0},
    {0x20,0x54,0x54,0x54,0x78},{0x7f,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7f},{0x38,0x54,0x54,0x54,0x18},
    {8,0x7e,9,1,2},{8,0x14,0x54,0x54,0x3c},{0x7f,8,4,4,0x78},{0,0x44,0x7d,0x40,0},{0x20,0x40,0x44,0x3d,0},
    {0,0x7f,0x10,0x28,0x44},{0,0x41,0x7f,0x40,0},{0x7c,4,0x18,4,0x78},{0x7c,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,8},{8,0x14,0x14,0x18,0x7c},{0x7c,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},{4,0x3f,0x44,0x40,0x20},
    {0x3c,0x40,0x40,0x20,0x7c},{0x1c,0x20,0x40,0x20,0x1c},{0x3c,0x40,0x30,0x40,0x3c},{0x44,0x28,0x10,0x28,0x44},{0xc,0x50,0x50,0x50,0x3c},
    {0x44,0x64,0x54,0x4c,0x44},{0,8,0x36,0x41,0},{0,0,0x7f,0,0},{0,0x41,0x36,8,0},{2,1,2,4,2}};

/* ---------------------------------------------------------- band canvas */
enum { BAND_ROWS = 16 };
static uint16_t band[FM1_SCREEN_WIDTH * BAND_ROWS];
static int clip_x0, clip_y0, clip_x1, clip_y1;   /* current band window, [x0,x1) */
static unsigned stride;

static void fill(int x, int y, int w, int h, uint16_t c)
{
    int x0 = x > clip_x0 ? x : clip_x0, x1 = x + w < clip_x1 ? x + w : clip_x1;
    int y0 = y > clip_y0 ? y : clip_y0, y1 = y + h < clip_y1 ? y + h : clip_y1;
    for (int row = y0; row < y1; ++row) {
        uint16_t *p = band + (row - clip_y0) * (int)stride + (x0 - clip_x0);
        for (int col = x0; col < x1; ++col) *p++ = c;
    }
}
static inline void dot(int x, int y, uint16_t c)
{
    if (x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1)
        band[(y - clip_y0) * (int)stride + (x - clip_x0)] = c;
}
static unsigned glyph_on(const uint8_t *g, int gx, int gy)
{
    return gx >= 0 && gx < 5 && gy >= 0 && gy < 7 && ((g[gx] >> gy) & 1U);
}
/* Text with transparent background. Scales >= 3 round diagonal steps so
 * large note and chord names read smoothly instead of as blocky pixels. */
static int text(int x, int y, const char *s, unsigned scale, uint16_t color)
{
    int w = 6 * (int)scale, h = 7 * (int)scale;
    if (y >= clip_y1 || y + h <= clip_y0) { while (*s) { x += w; ++s; } return x; }
    for (; *s; ++s, x += w) {
        if (x >= clip_x1) { while (*s) { x += w; ++s; } break; }
        if (x + w <= clip_x0) continue;
        unsigned c = (unsigned char)*s;
        const uint8_t *g = font[c >= 32 && c < 127 ? c - 32 : 0];
        int s_ = (int)scale;
        if (scale < 3) {
            for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx)
                if (glyph_on(g, gx, gy)) fill(x + gx * s_, y + gy * s_, s_, s_, color);
            continue;
        }
        /* Large text uses a light stroke: each glyph pixel is a (scale-1)
         * square joined to its right/lower neighbours, and diagonal steps
         * get a bridge, so names read as clean lines instead of blocks. */
        int k = s_ - 1;
        for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx) {
            if (!glyph_on(g, gx, gy)) continue;
            int px = x + gx * s_, py = y + gy * s_;
            fill(px, py, k, k, color);
            if (glyph_on(g, gx + 1, gy)) fill(px + k, py, s_ - k, k, color);
            if (glyph_on(g, gx, gy + 1)) fill(px, py + k, k, s_ - k, color);
            if (glyph_on(g, gx + 1, gy + 1) && !glyph_on(g, gx + 1, gy) && !glyph_on(g, gx, gy + 1))
                fill(px + s_ / 2, py + s_ / 2, k, k, color);
            if (glyph_on(g, gx - 1, gy + 1) && !glyph_on(g, gx - 1, gy) && !glyph_on(g, gx, gy + 1))
                fill(px - s_ / 2, py + s_ / 2, k, k, color);
        }
    }
    return x;
}
static int text_width(const char *s, unsigned scale) { return (int)strlen(s) * 6 * (int)scale - (int)scale; }
static void text_center(int cx, int y, const char *s, unsigned scale, uint16_t color)
{
    text(cx - text_width(s, scale) / 2, y, s, scale, color);
}
static void text_right(int right, int y, const char *s, unsigned scale, uint16_t color)
{
    text(right - text_width(s, scale), y, s, scale, color);
}
/* Rounded "pill" rectangle (1 px corner cut). */
static void pill(int x, int y, int w, int h, uint16_t c)
{
    fill(x + 1, y, w - 2, h, c); fill(x, y + 1, 1, h - 2, c); fill(x + w - 1, y + 1, 1, h - 2, c);
}
static char *decimal(char *out, int value)
{
    char tmp[12]; unsigned n = 0, neg = value < 0;
    unsigned v = neg ? (unsigned)-value : (unsigned)value;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    char *p = out;
    if (neg) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}
static char *append(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

/* ----------------------------------------------------------- UI state */
enum { W_HEADER, W_INFO, W_BIG, W_SCALE, W_PIANO, W_STATUS, W_BODY, W_BOOT, W_BOOT_TEXT, W_FULL, WIDGETS };
struct widget { uint8_t x, y, w, h; };
static const struct widget widgets[WIDGETS] = {
    {0, 0, 240, 20}, {0, 20, 240, 28}, {0, 48, 240, 66}, {0, 114, 240, 24},
    {0, 138, 240, 68}, {0, 206, 240, 34}, {0, 20, 240, 220}, {0, 52, 240, 136}, {0, 196, 240, 30}, {0, 0, 240, 240}};
static uint32_t wanted[WIDGETS], drawn[WIDGETS];
static unsigned active_mask;                         /* widgets of the current page */
static int job = -1; static unsigned job_row; static uint32_t job_sig;

static uint32_t boot_started, frame_ms, guard_ms, popup_until, now_ms;
static unsigned booting, shown_page = 255, shown_dim;
static char popup[28];
static unsigned battery_adc;
#ifdef KK_DEVICE_FM1
static unsigned master_adc;
static uint32_t raw_keys, raw_buttons;
#else
static unsigned master = 127;
#endif
/* Piano view: x offset of the left screen edge in world pixels, x16. */
enum { WK = 14, BK = 9, PIANO_TOP = 141, WHITE_H = 62, BLACK_H = 38 };
static int32_t view_x16, view_from16, view_to16; static uint32_t view_started;
static int piano_offset;   /* latched for every band of one redraw: no tearing */
static char big_name[16]; static uint8_t big_kind;       /* 0 none, 1 note, 2 chord, 3 pad, 4 drum */
static uint8_t sounding[24]; static unsigned sounding_n;
static int chord_root_shown = -1;
/* Bluetooth menu (same rows and actions as SMK37 OPEN's BLUETOOTH page). */
static unsigned ble_view, ble_action, ble_device, ble_forget, ble_seconds, ble_stage;
static int ble_result; static uint32_t ble_stage_since;
static struct fm1_ble_status ble_status;
static struct fm1_ble_device ble_devices[FM1_BLE_MAX_DEVICES];

static void say(const char *message, unsigned ms)
{
    strncpy(popup, message, sizeof popup - 1); popup[sizeof popup - 1] = 0;
    popup_until = now_ms + ms; if (!popup_until) popup_until = 1;
}

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
static void draw_big(void)
{
    const struct widget *w = &widgets[W_BIG];
    fill(0, w->y, 240, w->h, BG);
    char buf[24];
    if (kk.mode == KK_MODE_CLIP && kk.clip_action) {
        /* 0.50: OVERDUB (first black key) / DELETE (second) held. */
        text_center(120, w->y + 14, kk.clip_action == 2 ? "DELETE" : kk.clip_action == 3 ? "SELECT" : "OVERDUB", 3,
                    kk.clip_action == 2 ? RED : kk.clip_action == 3 ? CYAN : AMBER);
        text_center(120, w->y + 50, "PRESS A CLIP", 1, TEXT);
        return;
    }
    if (kk.mode == KK_MODE_CLIP) {
        static const char *const states[] = {"EMPTY", "STOPPED", "PLAYING", "QUEUED", "RECORDING", "QUEUED REC", "EMPTY (ARMED)"};
        unsigned st = kk_midi_clip_state(kk.clip_pad);
        const char *name = !kk_midi_host_protocol() ? "CLIPS" : st < 7 ? states[st] : "--";
        static const uint16_t colours[7] = {DIM, C565(60, 120, 255), GREEN, AMBER, RED, RED, DIM};
        text_center(120, w->y + 14, name, text_width(name, 3) <= 228 ? 3 : 2,
                    kk_midi_host_protocol() && st < 7 ? colours[st] : TEXT);
        char *p = append(buf, "TRACK "); p = decimal(p, kk.clip_pad % 8 + 1);
        p = append(p, "  SCENE "); decimal(p, (int)(kk_midi_host_scene() + kk.clip_pad / 8 + 1));
        text_center(120, w->y + 50, kk_midi_host_protocol() ? buf : "LOAD THE KAMOTEKEYS REMOTE SCRIPT", 1, DIM);
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
    char buf[32], *p;
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

/* ------------------------------------------------------------- pages */
static void body_title(const char *title, uint16_t colour)
{
    fill(0, 20, 240, 220, BG);
    text(8, 28, title, 2, colour);
    fill(8, 46, 224, 1, PANEL2);
}
static void row(unsigned i, const char *label, const char *value, unsigned selected)
{
    int y = 56 + (int)i * 24;
    if (selected) pill(6, y - 3, 228, 20, PANEL);
    text(12, y + 3, label, 1, DIM);
    text_right(228, y, value, 2, selected ? CYAN : TEXT);
}
static uint32_t patch_name_program = 999; static char patch_name[11];
static void draw_patch(void)
{
    body_title("PATCH", TEAL);
    char buf[16];
    decimal(buf, kk.program + 1);
    text(12, 60, buf, 5, CYAN);
    text(12, 104, patch_name, 2, TEXT);
    row(3, KK_K2 " INTERNAL SYNTH", kk_midi_local() ? "ON" : "OFF", 0);
    static const char *const ports[KK_PORTS_MODES] = {"AUTO", "ALL", "USB", "BLE"};
    row(4, KK_K3 " MIDI OUT", ports[kk_midi_ports_mode()], 0);
#ifdef KK_FIXED_VELOCITY
    char vel[8]; decimal(vel, kk.velocity);
    row(5, KK_K4 " VELOCITY", vel, 0);
#else
    static const char *const curves[KK_PAD_SENSITIVITY_MAX] = {"1 HARD", "2", "3 NORMAL", "4", "5", "6", "7 LIGHTEST"};
    row(5, KK_K4 " PAD SENSITIVITY", curves[(kk.pad_sensitivity ? kk.pad_sensitivity : 3) - 1], 0);
#endif
    text(12, 198, KK_K1 " PATCH   MIDI OUT AUTO = USB WHEN", 1, DIM);
    text(12, 208, "CONNECTED, ELSE BLE", 1, DIM);
    text(12, 222, KK_TXT_PATCH_BACK, 1, FAINT);
}
static void draw_arp(void)
{
    body_title("ARPEGGIATOR", CYAN);
    char buf[8], *p;
    row(0, KK_K1 " MODE", kk.arp.mode == KK_ARP_LATCH ? "LATCH" : "NORMAL", 0);
    row(1, KK_K2 " DIRECTION", kk_direction_name(kk.arp.direction), 0);
    row(2, KK_K3 " DIVISION", kk_division_name(kk.arp.division), 0);
    p = decimal(buf, kk.arp.gate); append(p, "%"); row(3, KK_K4 " GATE", buf, 0);
    p = decimal(buf, kk.arp.swing); append(p, "%"); row(4, KK_K5 " SWING", buf, 0);
    decimal(buf, kk.arp.octaves); row(5, KK_K6 " OCTAVES", buf, 0);
    text(12, 210, kk.arp_on ? "ARP ON" : "ARP OFF", 1, kk.arp_on ? GREEN : DIM);
    text(12, 222, "TAP ARP: BACK   SAVED AUTOMATICALLY", 1, FAINT);
}
static void draw_repeat(void)
{
    body_title("NOTE REPEAT", GREEN);
    row(0, KK_K1 " DIVISION", kk_division_name(kk.repeat_division), 0);
    char buf[8]; decimal(buf, kk_tempo_bpm()); row(1, kk_midi_host_tempo_x10() ? "TEMPO (FROM LIVE)" : "TEMPO (MAIN E1)", buf, 0);
    text(12, 210, kk.repeat_on ? "REPEAT ON" : "REPEAT OFF", 1, kk.repeat_on ? GREEN : DIM);
    text(12, 222, "TAP NOTE REPEAT: BACK", 1, FAINT);
}
static void draw_ble(void)
{
    static const char *const states[] = {"OFF", "IDLE", "WAITING FOR DEVICE", "SCANNING", "CONNECTING",
                                         "FINDING MIDI", "CONNECTED", "MIDI READY", "DISCONNECTING"};
    static const char *const errors[] = {"", "STACK BUSY", "TIMED OUT", "NOT BLE MIDI", "GATT FAILED",
                                         "LINK LOST", "PAIR FAILED", "KEY MISSING", "PAIR NOT SAVED"};
    struct fm1_bluetooth_status radio;
    fm1_bluetooth_get_status(&radio);
    unsigned flags = groove_settings_radio_flags(), on = (flags & GROOVE_RADIO_ON) != 0;
    unsigned midi = flags & GROOVE_RADIO_MIDI ? 1U + groove_settings_role_value() : 0;
    const struct fm1_ble_status *status = &ble_status;
    body_title(ble_view ? "MIDI HOST" : "BLUETOOTH", CYAN);
    char line[40];
    const char *state = !on ? "BLUETOOTH OFF" : !radio.ready ? "RADIO NOT READY" : status->pending ? "APPLYING SETTINGS"
        : !midi ? "MIDI OFF" : status->state == FM1_BLE_OFF ? "STARTING MIDI"
        : status->state <= FM1_BLE_DISCONNECTING ? states[status->state] : "UNKNOWN";
    if (on && radio.ready && (status->pending || status->state == FM1_BLE_SCANNING ||
        status->state == FM1_BLE_CONNECTING || status->state == FM1_BLE_DISCOVERING)) {
        char *p = append(line, state); p = append(p, " "); p = decimal(p, (int)ble_seconds); append(p, "S");
        state = line;
    }
    text(8, 52, state, 1, on ? GREEN : DIM);
    if (!ble_view) {
        static const char *const labels[] = {KK_K1 " BLUETOOTH", KK_K2 " BT AUDIO", KK_K3 " BLE ROLE", KK_K4 " CLOCK"};
        unsigned audio = (flags & GROOVE_RADIO_AUDIO) != 0;
        const char *names[4] = {on ? "ON" : "OFF", audio ? "ON" : "OFF",
                                midi == 2 ? "HOST" : midi == 1 ? "DEVICE" : "OFF", groove_clock_mode() ? "MIDI" : "INT"};
        for (unsigned i = 0; i < 4; ++i) {
            int y = 66 + (int)i * 28;
            text(8, y + 4, labels[i], 1, DIM);
            text(146, y, names[i], 2, i && i < 3 && !on ? DIM : CYAN);
        }
        const char *a = !on || !audio ? "AUDIO OFF" : radio.audio_playing ? "AUDIO PLAYING"
            : radio.audio_connected ? "AUDIO CONNECTED" : "AUDIO WAITING";
        text(8, 182, a, 1, radio.audio_playing ? GREEN : DIM);
        text(8, 194, ble_action ? "> FORGET MIDI PAIRS" : "> MIDI DEVICE LIST (HOST)", 1, CYAN);
    } else {
        for (unsigned i = 0; i < FM1_BLE_MAX_DEVICES; ++i) {
            int y = 64 + (int)i * 16;
            const struct fm1_ble_device *d = &ble_devices[i];
            if (i < status->count) {
                if (i == ble_device) pill(6, y - 2, 228, 14, PANEL);
                char n[4]; decimal(n, (int)i + 1);
                text(12, y + 1, n, 1, CYAN);
                text(27, y + 1, d->name[0] ? d->name : "UNNAMED MIDI DEVICE", 1, TEXT);
                decimal(n, d->rssi); text_right(230, y + 1, n, 1, DIM);
            } else if (!i)
                text(12, y + 1, status->state == FM1_BLE_SCANNING ? "LOOKING FOR MIDI DEVICES..." : "NO DEVICES - SELECT SCAN", 1, DIM);
        }
        static const char *const actions[] = {"> CONNECT SELECTED", "> SCAN", "> DISCONNECT", "> SETTINGS"};
        text(8, 196, actions[ble_action & 3], 1, CYAN);
    }
    const char *error = ble_forget == 1 ? KK_TXT_BT_FORGET
        : ble_forget == 2 && !status->pending && !status->error ? "PAIR AGAIN; FORGET ON COMPUTER TOO"
        : groove_settings_status() ? "SETTINGS NOT SAVED - RETRY"
        : ble_result == -2 ? "BUSY - RETRY" : ble_result == -3 ? "RADIO NOT READY"
        : ble_result ? "COMMAND UNAVAILABLE"
        : status->error <= KK_BLE_LAST_ERROR ? errors[status->error] : "";
    text(8, 210, error, 1, AMBER);
    text(8, 226, KK_TXT_BT_FOOTER, 1, DIM);
}
#ifdef KK_DEVICE_SMK37
extern volatile uint32_t smk37_queue_overflows, smk37_rgb_frames;
#endif
static void draw_diag(void)
{
    /* Diagnostics: full screen, no KamoteKeys branding, raw values only. */
    fill(0, 0, 240, 240, BG);
    text(8, 26, "DIAGNOSTICS", 2, AMBER);
    char line[40], *p;
    p = append(line, "FW " KK_FIRMWARE_BASE " "); p = append(p, FM1_RELEASE_VERSION); append(p, " / KK " KAMOTKEYS_VERSION);
    text(8, 48, line, 1, TEXT);
#ifdef KK_DEVICE_FM1
    p = append(line, "MASTER ADC: "); decimal(p, master_adc);
    text(8, 62, line, 1, TEXT);
#else
    for (unsigned i = 0; i < 4; ++i) {
        p = append(line, "FADER "); p = decimal(p, (int)i + 1); p = append(p, ": ");
        if (kk.fader_known & (1U << i)) decimal(p, kk.fader[i]); else append(p, "--");
        text(8 + (int)(i & 1) * 116, 62 + (int)(i / 2) * 12, line, 1, TEXT);
    }
#endif
    p = append(line, "BATTERY ADC: "); decimal(p, battery_adc);
    text(8, 90, line, 1, TEXT);
    unsigned ready = fm1_midi_ready_ports(), outs = fm1_midi_output_ports();
    p = append(line, "MIDI OUT PORTS: "); p = append(p, outs & 1 ? "DIN " : "");
    p = append(p, ready & 2 ? "USB+ " : "USB- "); append(p, ready & 4 ? "BLE+" : "BLE-");
    text(8, 104, line, 1, TEXT);
    p = append(line, "BLE STATE "); p = decimal(p, ble_status.state); p = append(p, " ERR "); decimal(p, ble_status.error);
    text(8, 118, line, 1, TEXT);
    struct fm1_engine_stats e; fm1_engine_stats_read(&e);
    p = append(line, "AUDIO UNDERRUNS "); decimal(p, (int)e.underruns); text(8, 132, line, 1, TEXT);
    p = append(line, "MIDI DROPPED IN "); p = decimal(p, (int)e.midi_dropped);
    p = append(p, " OUT "); decimal(p, (int)groove_midi_dropped()); text(8, 146, line, 1, TEXT);
#ifdef KK_DEVICE_SMK37
    p = append(line, "SCAN OVERFLOWS "); decimal(p, (int)smk37_queue_overflows); text(8, 160, line, 1, TEXT);
#endif
    p = append(line, "KEYS "); p = decimal(p, __builtin_popcountll(kk.keys)); p = append(p, "  PADS ");
    p = decimal(p, __builtin_popcount(kk.pads)); p = append(p, "  BTN "); decimal(p, (int)kk.buttons);
    text(8, 174, line, 1, TEXT);
    p = append(line, "SETTINGS STATUS "); decimal(p, groove_settings_status()); text(8, 188, line, 1, TEXT);
#ifdef KK_DEVICE_SMK37
    p = append(line, "E1 RGB TEST: "); append(p, kk_led_test_name(kk_led_test));
    text(8, 202, line, 1, kk_led_test ? AMBER : CYAN);
    p = append(line, "RGB FRAMES SENT: "); decimal(p, (int)smk37_rgb_frames);
    text(8, 212, line, 1, TEXT);
    p = append(line, "E2 RGB TIMING: "); p = decimal(p, kk.rgb_timing); append(p, kk.rgb_timing ? "" : " (ORIGINAL)");
    text(8, 222, line, 1, CYAN);
    text(150, 26, KK_TXT_DIAG_EXIT, 1, DIM);
#else
    p = append(line, "RAW KEYS "); p = decimal(p, (int)raw_keys); text(8, 202, line, 1, TEXT);
    p = append(line, "RAW BUTTONS "); decimal(p, (int)raw_buttons); text(8, 212, line, 1, TEXT);
    text(140, 26, KK_TXT_DIAG_EXIT, 1, DIM);
#endif
}

/* ------------------------------------------------------------- boot */
/* Aurora Boot: flowing ribbons inside a fixed band (never the full screen),
 * the KamoteKeys wordmark and version fading in, then a short status line. */
static void draw_boot(void)
{
    /* Understated boot (0.57): near-black, one hairline drawn out from the
     * centre in a teal-to-violet gradient with a soft glow, the wordmark
     * fading in above it, the version below; everything eases out. */
    const struct widget *w = &widgets[W_BOOT];
    uint32_t t = now_ms - boot_started;
    unsigned out = t > KK_BOOT_MS - 220 ? (t >= KK_BOOT_MS ? 0 : (KK_BOOT_MS - t) * 255 / 220) : 255;
    int centre = w->y + w->h / 2, line_y = centre + 14;
    fill(0, w->y, 240, w->h, BG);
    /* Hairline: half-length eases out over 520 ms, then holds. */
    unsigned p = t < 520 ? t * 1000 / 520 : 1000;
    unsigned ease = 1000 - (1000 - p) * (1000 - p) / 1000;          /* ease-out quad */
    int half = (int)(ease * 104 / 1000);
    for (int x = 120 - half; x < 120 + half; ++x) {
        unsigned k = (unsigned)(x - 16) * 255 / 208;                 /* 0 teal .. 255 violet */
        unsigned r = (0x00 * (255 - k) + 0x8a * k) / 255, g = (0xc8 * (255 - k) + 0x4c * k) / 255,
                 b = (0xb4 * (255 - k) + 0xff * k) / 255;
        int d = x - 120; if (d < 0) d = -d;
        unsigned edge = half ? 255 - (unsigned)d * 200 / (unsigned)half : 255;   /* brighter centre */
        unsigned a = edge * out / 255;
        dot(x, line_y, C565(r * a / 255, g * a / 255, b * a / 255));
        dot(x, line_y - 1, C565(r * a / 1530, g * a / 1530, b * a / 1530));     /* faint glow */
        dot(x, line_y + 1, C565(r * a / 1530, g * a / 1530, b * a / 1530));
    }
    /* A small travelling highlight on the line, once. */
    if (t > 200 && t < 900) {
        int hx = 120 - 104 + (int)((t - 200) * 208 / 700);
        for (int dx = -6; dx <= 6; ++dx) {
            int x = hx + dx; if (x < 120 - half || x >= 120 + half) continue;
            unsigned a = (unsigned)(6 - (dx < 0 ? -dx : dx)) * 40 * out / 255;
            dot(x, line_y, blend565(C565(30, 40, 60), TEXT, a > 255 ? 255 : a));
        }
    }
    unsigned logo = t < 300 ? 0 : t < 750 ? (t - 300) * 255 / 450 : 255;
    logo = logo * out / 255;
    if (logo) text_center(120, centre - 16, KAMOTKEYS_NAME, 3, blend565(BG, TEXT, logo));
    unsigned ver = t < 600 ? 0 : t < 900 ? (t - 600) * 255 / 300 : 255;
    ver = ver * out / 255;
    if (ver) text_center(120, line_y + 10, KAMOTKEYS_VERSION, 1, blend565(BG, DIM, ver));
}
static void draw_boot_text(void)
{
    const struct widget *w = &widgets[W_BOOT_TEXT];
    fill(0, w->y, 240, w->h, BG);
    uint32_t t = now_ms - boot_started;
    if (t < 650 || t >= KK_BOOT_MS) return;
    unsigned ready = fm1_midi_ready_ports();
    char line[40], *p = append(line, "MIDI");
    p = append(p, ready & 2 ? " . USB" : ""); append(p, ready & 4 ? " . BLE" : "");
    text_center(120, w->y + 6, "STARTING", 1, DIM);
    text_center(120, w->y + 18, line, 1, TEAL);
}

/* -------------------------------------------------------- frame/pump */
static void draw_widget(unsigned id)
{
    switch (id) {
    case W_HEADER: draw_header(); break;
    case W_INFO: draw_info(); break;
    case W_BIG: draw_big(); break;
    case W_SCALE: draw_scale(); break;
    case W_PIANO: if (kk.mode >= KK_MODE_PAD_SCALE && !kk_pad_scale_on_keys()) draw_pads(); else draw_piano(); break;   /* incl. Clip */
    case W_STATUS: draw_status(); break;
    case W_BOOT: draw_boot(); break;
    case W_BOOT_TEXT: draw_boot_text(); break;
    default:
        switch (kk.page) {
        case KK_PAGE_BT: draw_ble(); break;
        case KK_PAGE_PATCH: draw_patch(); break;
        case KK_PAGE_ARP: draw_arp(); break;
        case KK_PAGE_REPEAT: draw_repeat(); break;
        default: draw_diag(); break;
        }
    }
}
static uint32_t mixsig(uint32_t h, uint32_t v) { return (h ^ v) * 16777619U; }
static uint32_t strsig(uint32_t h, const char *s) { while (*s) h = mixsig(h, (uint8_t)*s++); return h; }
static void signatures(void)
{
    uint32_t base = 2166136261U ^ (kk.screen_dim ? 0x5a5a : 0);
    if (booting) {
        wanted[W_BOOT] = mixsig(base, now_ms / 30);
        uint32_t t = now_ms - boot_started;
        wanted[W_BOOT_TEXT] = mixsig(base, (t >= 650) + 2 * (t >= KK_BOOT_MS) + 4 * fm1_midi_ready_ports());
        return;
    }
    wanted[W_HEADER] = mixsig(base, fm1_midi_ready_ports());
    uint32_t h = mixsig(base, kk.mode); h = mixsig(h, (uint32_t)kk_active_octave()); h = mixsig(h, kk_tempo_x10());
    h = mixsig(h, kk_midi_host_protocol());
    wanted[W_INFO] = h;
    h = strsig(mixsig(base, big_kind * 2U + !!sounding_n), big_name); h = mixsig(h, kk.last_pad);
    if (kk.mode == KK_MODE_CLIP) h = mixsig(mixsig(mixsig(mixsig(h, 99), kk.clip_pad), kk_midi_clip_state(kk.clip_pad) | kk_midi_host_protocol() << 8 | kk_midi_host_scene() << 16), kk.clip_action);
    wanted[W_BIG] = h;
    wanted[W_SCALE] = mixsig(mixsig(base, kk.scale.scale), kk.scale.root);
    h = mixsig(base, kk.mode); h = mixsig(h, (uint32_t)view_x16 / 16); h = mixsig(h, (uint32_t)chord_root_shown);
    h = mixsig(h, kk.scale.scale * 16U + kk.scale.root); h = mixsig(h, (uint32_t)kk_active_octave());
    for (unsigned i = 0; i < sounding_n; ++i) h = mixsig(h, sounding[i]);
    if (kk.mode >= KK_MODE_PAD_SCALE) for (unsigned p = 0; p < KK_PADS; ++p) h = mixsig(h, kk_pad_lit(p));
    if (kk.mode == KK_MODE_CLIP) { for (unsigned p = 0; p < KK_PADS; ++p) h = mixsig(h, kk_midi_clip_state(p)); h = mixsig(h, kk_midi_host_scene()); }
    wanted[W_PIANO] = h;
    h = mixsig(base, kk.arp_on); h = mixsig(h, kk.arp.division | kk.arp.direction << 4 | kk.arp.mode << 8);
    h = mixsig(h, kk.arp.gate); h = mixsig(h, kk.sustain_on * 16U);
    h = mixsig(h, kk_velocity_override()); h = mixsig(h, kk.chord_mods | kk.chord_lock << 12); h = mixsig(h, kk_midi_local());
    if (popup_until && (int32_t)(now_ms - popup_until) < 0) h = strsig(mixsig(h, 77), popup);
    if (kk.anim_active) h = mixsig(mixsig(h, kk.anim_pattern + 100U), kk.anim_speed / 10U);
    wanted[W_STATUS] = h;
    /* Page body: hash everything shown on the open page. */
    h = mixsig(base, kk.page); h = mixsig(h, kk.program); h = strsig(h, patch_name);
    h = mixsig(h, kk_midi_local() * 8U + kk_midi_ports_mode() + kk.pad_sensitivity * 64U);
    h = mixsig(h, kk.arp.mode | kk.arp.direction << 2 | kk.arp.division << 5 | (uint32_t)kk.arp.gate << 8 |
                  (uint32_t)kk.arp.swing << 16 | (uint32_t)kk.arp.octaves << 24);
    h = mixsig(h, kk.arp_on * 2U + kk.repeat_on); h = mixsig(h, kk.repeat_division); h = mixsig(h, kk_tempo_x10());
    if (kk.page == KK_PAGE_BT) {
        h = mixsig(h, ble_view | ble_action << 2 | ble_device << 4 | ble_forget << 8);
        h = mixsig(h, (uint32_t)ble_result); h = mixsig(h, ble_seconds);
        h = mixsig(h, ble_status.role | ble_status.state << 4 | ble_status.count << 8 | ble_status.error << 12 | ble_status.pending << 16);
        h = mixsig(h, groove_settings_radio_flags() | groove_settings_role_value() << 4 | groove_clock_mode() << 6);
        struct fm1_bluetooth_status radio; fm1_bluetooth_get_status(&radio);
        h = mixsig(h, radio.ready | radio.audio_connected << 1 | radio.audio_playing << 2);
        h = mixsig(h, (uint32_t)groove_settings_status());
        for (unsigned i = 0; i < ble_status.count && i < FM1_BLE_MAX_DEVICES; ++i)
            h = mixsig(strsig(h, ble_devices[i].name), (uint8_t)ble_devices[i].rssi);
    } else if (kk.page == KK_PAGE_DIAG) {
#ifdef KK_DEVICE_SMK37
        h = mixsig(mixsig(mixsig(mixsig(h, now_ms / 250), kk_led_test), smk37_rgb_frames), kk.rgb_timing);
#else
        h = mixsig(mixsig(mixsig(mixsig(h, now_ms / 250), raw_keys), raw_buttons), master_adc);
#endif
    }
    wanted[W_BODY] = h;
    wanted[W_FULL] = h;
}
static void set_page_widgets(void)
{
    unsigned main = kk.page == KK_PAGE_MAIN;
    active_mask = booting ? (1U << W_BOOT) | (1U << W_BOOT_TEXT)
        : main ? (1U << W_HEADER) | (1U << W_INFO) | (1U << W_BIG) | (1U << W_SCALE) | (1U << W_PIANO) | (1U << W_STATUS)
        : kk.page == KK_PAGE_DIAG ? 1U << W_FULL : (1U << W_HEADER) | (1U << W_BODY);
    for (unsigned i = 0; i < WIDGETS; ++i) drawn[i] = ~wanted[i];   /* force a redraw */
    job = -1;
}
/* Render and send bands while the DMA engine keeps up, within a small time
 * budget per control poll, so the LCD never depends on a fast poll rate and
 * never delays keys, pads or MIDI by more than a few milliseconds. Input and
 * ARP processing always run before this in the same poll. */
static void pump(void)
{
    uint32_t started = fm1_millis();
    for (unsigned bands = 0; bands < 15; ++bands) {
        int pending;
        while ((pending = fm1_screen_pending()) > 0)
            if (fm1_millis() - started >= 4U) return;
        if (job < 0) {
            for (unsigned i = 0; i < WIDGETS; ++i)
                if ((active_mask & (1U << i)) && wanted[i] != drawn[i]) {
                    job = (int)i; job_row = 0; job_sig = wanted[i];
                    if (i == W_PIANO) piano_offset = view_x16 / 16;
                    break;
                }
            if (job < 0) return;
        }
        const struct widget *w = &widgets[job];
        unsigned rows = w->h - job_row < BAND_ROWS ? w->h - job_row : BAND_ROWS;
        clip_x0 = w->x; clip_x1 = w->x + w->w; clip_y0 = w->y + (int)job_row; clip_y1 = clip_y0 + (int)rows;
        stride = w->w;
        draw_widget((unsigned)job);
        unsigned count = w->w * rows;
        if (kk.screen_dim)       /* Fader 4 <= 10 %: half apparent LCD brightness */
            for (unsigned i = 0; i < count; ++i) band[i] = (uint16_t)((band[i] >> 1) & 0x7bef);
        if (fm1_screen_submit(w->x, (unsigned)clip_y0, w->w, rows, band, count) < 0) { job = -1; return; }
        job_row += rows;
        if (job_row >= w->h) { drawn[job] = job_sig; job = -1; }
        if (fm1_millis() - started >= 4U) return;
    }
}

/* ------------------------------------------------------------- input */
static int take(unsigned encoder) { int d = kk.encoder[encoder]; kk.encoder[encoder] = 0; return d; }
void kk_ui_bt_key(unsigned save)
{
    if (kk.page == KK_PAGE_DIAG) { kk.page = KK_PAGE_MAIN; return; }
    if (!save) {   /* HOME: cancel a pending confirmation, otherwise leave */
        if (ble_forget == 1) ble_forget = 0;
        else kk.page = KK_PAGE_MAIN;
        return;
    }
    unsigned flags = groove_settings_radio_flags(), role = groove_settings_role_value();
    if (!ble_view) {
        if (ble_action) {
            if (ble_forget != 1) ble_forget = 1;
            else { ble_result = fm1_ble_forget_bonds(); ble_forget = ble_result ? 0 : 2; }
        } else if ((flags & (GROOVE_RADIO_ON | GROOVE_RADIO_MIDI)) == (GROOVE_RADIO_ON | GROOVE_RADIO_MIDI) &&
                   role == FM1_BLE_CENTRAL) {
            ble_view = 1; ble_action = ble_status.count ? 0 : 1; ble_forget = 0;
            if (ble_status.state == FM1_BLE_IDLE) ble_result = fm1_ble_scan();
        } else ble_result = -1;
    } else {
        if (!ble_action) ble_result = fm1_ble_connect(ble_device);
        else if (ble_action == 1) { ble_result = fm1_ble_scan(); if (!ble_result) ble_device = 0; }
        else if (ble_action == 2) ble_result = fm1_ble_disconnect();
        else { ble_view = ble_action = ble_forget = 0; ble_result = 0; }
    }
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static void ble_input(void)
{
    int e[8];
    for (unsigned i = 0; i < 8; ++i) e[i] = i == 7 ? 0 : take(i);
    for (unsigned i = 0; i < 7; ++i) if (e[i]) ble_forget = ble_forget == 2 ? 2 : 0;
    ble_action = (unsigned)clampi((int)ble_action + e[6], 0, ble_view ? 3 : 1);
    if (!ble_view) {
        unsigned flags = groove_settings_radio_flags(), role = groove_settings_role_value();
        for (unsigned i = 0; i < 2; ++i) {
            if (e[i] > 0) flags |= 1U << i;
            else if (e[i] < 0) flags &= ~(1U << i);
        }
        if (e[2]) role = (unsigned)clampi((int)role + e[2], FM1_BLE_PERIPHERAL, FM1_BLE_CENTRAL);
        if (flags != groove_settings_radio_flags() || role != groove_settings_role_value())
            ble_result = groove_settings_radio(flags, role);
        if (e[3]) groove_settings_clock(e[3] > 0 ? GROOVE_CLOCK_MIDI : GROOVE_CLOCK_INTERNAL);
    } else {
        unsigned count = ble_status.count > FM1_BLE_MAX_DEVICES ? FM1_BLE_MAX_DEVICES : ble_status.count;
        ble_device = (unsigned)clampi((int)ble_device + e[4], 0, count ? (int)count - 1 : 0);
    }
}
static void popup_value(const char *label, const char *value)
{
    char buf[48], *p = append(buf, label); p = append(p, " "); append(p, value);
    say(buf, 1200);
}
static void main_input(void)
{
    char v[12], *p;
    int d;
    /* Knob shift (FM-1 SAVE held): K1-K3 do what the SMK-37 faders do. */
    static uint8_t shift_shown;
    if (kk.knob_shift != shift_shown) {
        shift_shown = kk.knob_shift;
        if (shift_shown) say("K1 VEL  K2 SCALE  K3 ROOT", 1500);
    }
    if (kk.knob_shift) {
        for (unsigned i = 0; i < 7; ++i) if (kk.encoder[i]) kk.knob_shift = 2;   /* any knob turned: no Play/Pause on release */
        if ((d = take(0))) { kk_edit_velocity(d); decimal(v, kk.velocity); popup_value("VELOCITY", v); }
        if ((d = take(1))) { kk_edit_scale(d); popup_value("SCALE", kk_scale_name(kk.scale.scale)); }
        if ((d = take(2))) { kk_edit_root(d); popup_value("ROOT", kk_note_name(kk.scale.root)); }
        for (unsigned i = 3; i < 7; ++i) (void)take(i);
        return;
    }
    if ((d = take(0))) {
        if (kk_midi_host_tempo_x10()) say("TEMPO FOLLOWS ABLETON", 1200);
        else { kk_edit_bpm(d > 2 || d < -2 ? d * 2 : d); decimal(v, kk.bpm); popup_value("TEMPO", v); }
    }
    if ((d = take(1))) { kk_edit_program(d); decimal(v, kk.program + 1); popup_value("PATCH", v); }
    if ((d = take(2))) { kk_edit_arp(2, d); popup_value("ARP DIV", kk_division_name(kk.arp.division)); }
    if ((d = take(3))) { kk_edit_arp(3, d); p = decimal(v, kk.arp.gate); append(p, "%"); popup_value("ARP GATE", v); }
    if ((d = take(4))) { kk_edit_arp(1, d); popup_value("ARP DIR", kk_direction_name(kk.arp.direction)); }
    if ((d = take(5))) { kk_edit_arp(4, d); p = decimal(v, kk.arp.swing); append(p, "%"); popup_value("ARP SWING", v); }
    (void)take(6);   /* E7: free (Note Repeat was replaced by SUSTAIN in 0.45) */
}
static void page_input(void)
{
    int d;
    switch (kk.page) {
    case KK_PAGE_MAIN: main_input(); break;
    case KK_PAGE_BT: ble_input(); break;
    case KK_PAGE_PATCH:
        if ((d = take(0))) kk_edit_program(d);
        if ((d = take(1))) { kk_midi_set_local(d > 0); kk_settings_touch(); }
        if ((d = take(2))) { kk_midi_set_ports((unsigned)clampi((int)kk_midi_ports_mode() + d, 0, KK_PORTS_MODES - 1)); kk_settings_touch(); }
#ifdef KK_FIXED_VELOCITY
        if ((d = take(3))) kk_edit_velocity(d);
#else
        if ((d = take(3))) { kk.pad_sensitivity = (uint8_t)clampi((kk.pad_sensitivity ? kk.pad_sensitivity : 3) + d, 1, KK_PAD_SENSITIVITY_MAX); kk_settings_touch(); }
#endif
        for (unsigned i = 4; i < 7; ++i) (void)take(i);
        break;
    case KK_PAGE_ARP:
        for (unsigned i = 0; i < 6; ++i) if ((d = take(i))) kk_edit_arp(i, d);
        (void)take(6);
        break;
    case KK_PAGE_REPEAT:
        if ((d = take(0))) kk_edit_repeat(d);
        for (unsigned i = 1; i < 7; ++i) (void)take(i);
        break;
    default:
#ifdef KK_DEVICE_FM1
        for (unsigned i = 0; i < 7; ++i) (void)take(i);
    }
#else
        if ((d = take(0))) kk_led_test = (uint8_t)clampi((int)kk_led_test + d, 0, KK_LED_TESTS - 1);
        if ((d = take(1))) {
            kk.rgb_timing = (uint8_t)clampi((int)kk.rgb_timing + d, 0, SMK37_RGB_TIMINGS - 1);
            smk37_rgb_timing(kk.rgb_timing); kk_settings_touch();
        }
        for (unsigned i = 2; i < 7; ++i) (void)take(i);
    }
    /* E8 is always MASTER volume, as on SMK37 OPEN. */
    if ((d = take(7))) master = (unsigned)clampi((int)master + d, 0, 127);
    (void)groove_master_update((master * 1023U + 63U) / 127U);
#endif
}
/* Track what is sounding on the melodic channel (the MIDI voice list is the
 * source of truth for highlights and names). */
static void follow_notes(void)
{
    static uint32_t revision = ~0U; static uint8_t mode = 255, last_pad = 255; static int16_t last_note = -2;
    if (revision == kk_midi_revision() && mode == kk.mode && last_pad == kk.last_pad && last_note == kk.last_note) return;
    revision = kk_midi_revision(); mode = kk.mode; last_pad = kk.last_pad; last_note = kk.last_note;
    sounding_n = kk_midi_sounding(KK_CH_MELODIC, sounding, sizeof sounding);
    if (kk.mode == KK_MODE_DRUM) {
        unsigned channel; int note = kk_pad_note(kk.last_pad, &channel);
        if (kk.last_note >= 0 && note >= 0) {
            char *p = append(big_name, kk_drum_label(note)); *p++ = ' '; decimal(p, note);
            big_kind = 4;
        }
        return;
    }
    if (kk.mode == KK_MODE_PAD_SCALE && kk.last_note >= 0 && !kk_pad_scale_on_keys()) {
        unsigned channel; int note = kk_pad_note(kk.last_pad, &channel);
        if (note >= 0) { kk_note_label(big_name, note); big_kind = 3; }
        if (sounding_n) piano_target(now_ms);
        return;
    }
    if (!sounding_n) return;     /* keep the last name, drawn dimmed */
    int root = kk.chord_root >= 0 && is_sounding(kk.chord_root) ? kk.chord_root : -1;
    int named = kk_chord_name(sounding, sounding_n, root, big_name);
    chord_root_shown = sounding_n > 1 ? (root >= 0 ? root : named) : -1;
    /* Highlight the detected root only if it is itself sounding. */
    if (chord_root_shown >= 0 && !is_sounding(chord_root_shown)) chord_root_shown = -1;
    big_kind = sounding_n > 1 ? 2 : 1;
    piano_target(now_ms);
}
static void refresh_patch_name(void)
{
    if (patch_name_program == kk.program) return;
    uint8_t patch[156];
    if (groove_fm_copy(0, patch, NULL)) return;
    for (unsigned i = 0; i < 10; ++i) {
        char c = (char)patch[145 + i];
        patch_name[i] = c >= 32 && c < 127 ? c : ' ';
    }
    patch_name[10] = 0;
    patch_name_program = kk.program;
}

/* ---------------------------------------------------- entry points */
void groove_screen_init(void)
{
    kk_perform_init();
#ifdef KK_DEVICE_FM1
    kk_fm1_input_reset();
#endif
    memset(wanted, 0, sizeof wanted); memset(drawn, 0, sizeof drawn);
    job = -1; popup_until = 0; big_name[0] = 0; big_kind = 0; sounding_n = 0;
#ifdef KK_DEVICE_SMK37
    master = 127;
#endif
    ble_view = ble_action = ble_device = ble_forget = 0; ble_result = 0;
    patch_name_program = 999; patch_name[0] = 0;
    fm1_controls_init();
    fm1_screen_init();                 /* SDK panel bring-up; clears to black */
    now_ms = boot_started = frame_ms = guard_ms = ble_stage_since = fm1_millis();
    /* Start the piano around the anchor (second C) octave. */
    view_x16 = view_to16 = view_from16 = (note_center(60) - 120) * 16;
    booting = 1; shown_page = 255;
}
void groove_screen_tap(uint32_t now) { (void)groove_tempo_tap(now); }
/* Workbench MMC transport: KamoteKeys never runs the internal sequencer. */
void groove_screen_mmc(unsigned command) { (void)command; }

void groove_screen_control(void)
{
    struct fm1_controls controls;
    fm1_controls_read(&controls);
    battery_adc = controls.battery;
#ifdef KK_DEVICE_FM1
    master_adc = controls.knob; raw_keys = controls.keys; raw_buttons = controls.buttons;
    kk_fm1_input(&controls);           /* keys, buttons and encoders -> KamoteKeys events */
    (void)groove_master_update(controls.knob);   /* analog MASTER knob */
#else
    /* SMK-37: the scanner dispatches every event during fm1_controls_read.
     * Encoders arrive as events; the browser workbench reports them in FM1
     * order here (SELECT, PAGE, K1..K4, PRESET). */
    static const uint8_t physical[7] = {4, 5, 0, 1, 2, 3, 6};
    for (unsigned i = 0; i < 7; ++i) kk.encoder[physical[i]] = (int16_t)(kk.encoder[physical[i]] + controls.encoders[i]);
#endif
    now_ms = fm1_millis();
    kk_perform_tick(now_ms);
    if (now_ms - guard_ms >= 250U) { guard_ms = now_ms; kk_perform_guard(); }
    uint32_t boot = now_ms - boot_started;
    if (booting && boot >= KK_BOOT_MS) { booting = 0; shown_page = 255; }
    kk_led_service(booting ? boot : boot < KK_BOOT_MS + KK_BOOT_FADE_MS ? boot : KK_BOOT_MS + KK_BOOT_FADE_MS);

    fm1_ble_get_status(&ble_status);
    unsigned stage = ble_status.state | (ble_status.pending << 8);
    if (stage != ble_stage) { ble_stage = stage; ble_stage_since = now_ms; }
    ble_seconds = (now_ms - ble_stage_since) / 1000U; if (ble_seconds > 999) ble_seconds = 999;
    if (kk.page == KK_PAGE_BT && ble_view)
        for (unsigned i = 0; i < ble_status.count && i < FM1_BLE_MAX_DEVICES; ++i) (void)fm1_ble_get_device(i, &ble_devices[i]);

    page_input();
    static uint8_t ripple_shown;
    if (kk.ripple != ripple_shown) { ripple_shown = kk.ripple; if (!booting) say(kk.ripple ? "RIPPLE LIGHTS ON" : "RIPPLE LIGHTS OFF", 1500); }
    follow_notes();
    if (kk.page == KK_PAGE_PATCH) refresh_patch_name();
    if (!booting && (kk.page != shown_page || kk.screen_dim != shown_dim)) {
        if (kk.page != shown_page && kk.page == KK_PAGE_BT) { ble_view = ble_action = ble_forget = 0; ble_result = 0; }
        shown_page = kk.page; shown_dim = kk.screen_dim;
        signatures(); set_page_widgets();
    } else if (booting && shown_page != 254) { shown_page = 254; signatures(); set_page_widgets(); }
    piano_animate(now_ms);
    if (now_ms - frame_ms >= 15U) { frame_ms = now_ms; signatures(); }
    pump();
}

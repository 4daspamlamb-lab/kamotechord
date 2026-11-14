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
#include "kamotkeys/kk_boot.h"
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

#include "kamotkeys/kk_scr_canvas.inc"
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
#define master kk.master   /* 0.77: shared with Fader 4 */
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

#include "kamotkeys/kk_scr_main.inc"
#include "kamotkeys/kk_scr_pages.inc"
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
        wanted[W_BOOT_TEXT] = mixsig(base, t >= KK_BOOT_NAME_AT && t < KK_BOOT_NAME_AT + 400 ? now_ms / 30 : (uint32_t)((t >= KK_BOOT_NAME_AT) + 2 * (t >= KK_BOOT_MS)));
        return;
    }
    wanted[W_HEADER] = mixsig(base, fm1_midi_ready_ports());
    uint32_t h = mixsig(base, kk.mode); h = mixsig(h, (uint32_t)kk_active_octave()); h = mixsig(h, kk_tempo_x10());
    h = mixsig(h, kk_midi_host_protocol());
    wanted[W_INFO] = h;
    h = strsig(mixsig(base, big_kind * 2U + !!sounding_n), big_name); h = mixsig(h, kk.last_pad);
    if (kk.mode == KK_MODE_CLIP) h = mixsig(mixsig(mixsig(mixsig(h, 99), kk.clip_pad), kk_midi_clip_state(kk.clip_pad) | kk_midi_host_protocol() << 8 | kk_midi_host_scene() << 16), kk.clip_action);
    if (kk.mode == KK_MODE_STEP)
        for (unsigned c = KK_STEP_PAGE_CC; c <= KK_STEP_INFO_LAST; ++c) h = mixsig(h, kk_midi_step_info(c) | kk_midi_host_protocol() << 8);
    { unsigned ca, cb, ck = kk_midi_copy(&ca, &cb); h = mixsig(mixsig(mixsig(h, ck), ca), cb | (unsigned)kk.clip_play << 8); }
    h = mixsig(h, kk_midi_host_tracks() | kk_midi_arrange() << 8);
    wanted[W_BIG] = h;
    wanted[W_SCALE] = mixsig(mixsig(base, kk.scale.scale), kk.scale.root);
    h = mixsig(base, kk.mode); h = mixsig(h, (uint32_t)view_x16 / 16); h = mixsig(h, (uint32_t)chord_root_shown);
    h = mixsig(h, kk.scale.scale * 16U + kk.scale.root); h = mixsig(h, (uint32_t)kk_active_octave());
    for (unsigned i = 0; i < sounding_n; ++i) h = mixsig(h, sounding[i]);
    if (kk.mode >= KK_MODE_PAD_SCALE) for (unsigned p = 0; p < KK_PADS; ++p) h = mixsig(h, kk_pad_lit(p));
    if (kk.mode == KK_MODE_CLIP) { for (unsigned p = 0; p < KK_PADS; ++p) h = mixsig(h, kk_midi_clip_state(p)); h = mixsig(h, kk_midi_host_scene()); }
    if (kk.mode == KK_MODE_STEP) {
        for (unsigned p = 0; p < KK_PADS; ++p) h = mixsig(h, kk_midi_step_code(p) | (unsigned)kk_pad_lit(p) << 8);
        for (unsigned c = KK_STEP_PAGE_CC; c <= KK_STEP_INFO_LAST; ++c) h = mixsig(h, kk_midi_step_info(c));
    }
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

#include "kamotkeys/kk_scr_input.inc"
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

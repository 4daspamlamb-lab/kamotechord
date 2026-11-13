/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Pages (patch, ARP, repeat, Bluetooth, diagnostics) and boot.
 * Part of core/screen.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
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

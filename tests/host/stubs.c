/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host stand-ins for the binary SMK37 SDK and engine, used only by the
 * KamotKeys host tests. They record MIDI output and emulate the LCD. */
#include "stubs.h"
#include "instrument.h"
#include "fm1_engine_control.h"
#include <stdio.h>
#include <string.h>

uint32_t host_ms;
struct host_msg host_out[HOST_LOG], host_local[HOST_LOG];
unsigned host_out_n, host_local_n;
unsigned host_ready_ports = 6; /* USB + BLE */
uint16_t host_fb[240 * 240];
uint32_t host_rgb[16];
uint8_t host_led[19];
unsigned host_settings_changes;
volatile uint32_t smk37_queue_overflows, smk37_rgb_frames;

uint32_t fm1_millis(void) { return host_ms; }
unsigned fm1_midi_output_ports(void) { return 6; }
unsigned fm1_midi_input_ports(void) { return 6; }
unsigned fm1_midi_ready_ports(void) { return host_ready_ports; }
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n)
{
    if (host_out_n < HOST_LOG) {
        struct host_msg *x = &host_out[host_out_n++];
        x->port = (uint8_t)port; x->n = (uint8_t)n; memcpy(x->b, m, n);
    }
    return 0;
}
int fm1_midi_post(const uint8_t *m, size_t n)
{
    if (host_local_n < HOST_LOG) {
        struct host_msg *x = &host_local[host_local_n++];
        x->port = 9; x->n = (uint8_t)n; memcpy(x->b, m, n);
    }
    return 0;
}
void host_clear(void) { host_out_n = host_local_n = 0; }

/* LCD */
static unsigned pending;
void fm1_screen_init(void) { memset(host_fb, 0, sizeof host_fb); }
int fm1_screen_pending(void) { return 0; }
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *px, size_t count)
{
    if (x + w > 240 || y + h > 240 || count < w * h) { fprintf(stderr, "bad submit %u %u %u %u\n", x, y, w, h); return -1; }
    for (unsigned r = 0; r < h; ++r) memcpy(&host_fb[(y + r) * 240 + x], px + r * w, w * 2);
    (void)pending;
    return 0;
}
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *s) { memset(s, 0, sizeof *s); s->battery = 812; }
unsigned host_rgb_changes;
void smk37_rgb_set(unsigned id, unsigned rgb) { if (host_rgb[id] != rgb) ++host_rgb_changes; host_rgb[id] = rgb; }
void smk37_rgb_service(void) {}
void smk37_rgb_touch(void) {}
unsigned host_rgb_timing;
void smk37_rgb_timing(unsigned timing) { host_rgb_timing = timing; }
void smk37_led_level(unsigned id, unsigned level) { host_led[id] = (uint8_t)level; }
void groove_settings_panel_changed(void) { ++host_settings_changes; }

/* Bluetooth */
void fm1_ble_get_status(struct fm1_ble_status *s) { memset(s, 0, sizeof *s); s->state = FM1_BLE_READY; }
int fm1_ble_get_device(unsigned i, struct fm1_ble_device *d) { (void)i; memset(d, 0, sizeof *d); return 0; }
void fm1_bluetooth_get_status(struct fm1_bluetooth_status *s) { memset(s, 0, sizeof *s); s->ready = 1; s->features = 3; }
int fm1_ble_scan(void) { return 0; }
int fm1_ble_connect(unsigned i) { (void)i; return 0; }
int fm1_ble_disconnect(void) { return 0; }
int fm1_ble_forget_bonds(void) { return 0; }
static unsigned radio_flags = 7, radio_role;
unsigned groove_settings_radio_flags(void) { return radio_flags; }
unsigned groove_settings_role_value(void) { return radio_role; }
int groove_settings_radio(unsigned f, unsigned r) { radio_flags = f; radio_role = r; return 0; }
void groove_settings_clock(unsigned m) { (void)m; }
int groove_settings_status(void) { return 0; }
unsigned groove_clock_mode(void) { return 0; }
void fm1_engine_stats_read(struct fm1_engine_stats *e) { memset(e, 0, sizeof *e); }
void fm1_engine_pause(void) {}
void fm1_engine_resume(void) {}
void fm1_engine_stop(void) {}
int groove_master_update(unsigned adc) { (void)adc; return 0; }
int groove_tempo_tap(uint32_t now) { (void)now; return 0; }

/* Engine model */
static unsigned arp[8], source[8] = {GROOVE_DX7, 0, 0, 0, 0, 0, 0, GROOVE_808}, channel[8] = {0, 1, 2, 3, 4, 5, 6, 9};
static unsigned program, tempo = 120;
unsigned groove_arp_enabled(unsigned t) { return arp[t]; }
void groove_arp_set(unsigned t, unsigned e) { arp[t] = e; }
unsigned groove_track_role(unsigned t) { (void)t; return GROOVE_TRACK_NORMAL; }
int groove_track_role_set(unsigned t, unsigned r) { (void)t; (void)r; return 0; }
unsigned groove_source_type(unsigned t) { return source[t]; }
int groove_source_set(unsigned t, unsigned s) { source[t] = s; return 0; }
unsigned groove_pad_count(unsigned t) { return source[t] == GROOVE_808 ? 11 : 0; }
unsigned groove_midi_input_channel(unsigned t) { return channel[t]; }
int groove_midi_input_set(unsigned t, unsigned c) { channel[t] = c; return 0; }
void groove_get_view(unsigned t, unsigned step, struct groove_view *v)
{ (void)t; (void)step; memset(v, 0, sizeof *v); v->program = (uint8_t)program; v->tempo = (uint16_t)tempo; }
void groove_control_edit(unsigned t, int p, int v, int c) { (void)t; (void)v; (void)c; program = (unsigned)((int)program + p); }
void groove_tempo_edit(int d) { tempo = (unsigned)((int)tempo + d); }
uint32_t groove_project_revision(void) { return 1; }
unsigned host_engine_program(void) { return program; }
unsigned host_engine_tempo(void) { return tempo; }
int groove_fm_copy(unsigned t, uint8_t patch[156], struct groove_fm_controls *c)
{ (void)t; (void)c; memset(patch, ' ', 156); memcpy(patch + 145, "E.PIANO 1 ", 10); return 0; }
void groove_sequence_release(unsigned t) { (void)t; }

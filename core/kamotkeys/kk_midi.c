/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys central MIDI note engine. */
#include "kk_midi.h"
#include "kk_config.h"
#include "fm1.h"
#include <string.h>

/* Owner counts for the note channels KamoteKeys plays: 1, 10 and 16 (Clip). */
static uint8_t counts[4][128];
/* Written by the MIDI routing task, read by the control task: single bytes. */
static volatile uint8_t remote_notes[2][128];          /* host note activity */
static volatile uint8_t clip_states[16] = {
    KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN,
    KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN,
    KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN,
    KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN, KK_CLIP_UNKNOWN};
static volatile uint8_t host_protocol;
static volatile uint16_t host_tempo;   /* Live tempo x 10, 0 = none */
static uint8_t tempo_msb, host_scene, host_highlight = 127;
static uint8_t step_codes[16], step_info[KK_STEP_BANK_CC - KK_STEP_PAGE_CC + 1];
static volatile uint32_t input_revision;
static uint8_t ports_mode = KK_PORTS_AUTO, local_on = 1;
static uint32_t revision;

static int slot(unsigned channel)
{
    return channel == KK_CH_MELODIC ? 0 : channel == KK_CH_DRUM ? 1 : channel == KK_CH_CLIP ? 2 : channel == KK_CH_STEP ? 3 : -1;
}
void kk_midi_init(void) { memset(counts, 0, sizeof counts); ++revision; }

unsigned kk_midi_port_mask(void)
{
    unsigned outputs = fm1_midi_output_ports(), ready = fm1_midi_ready_ports();
    unsigned din = outputs & (1U << FM1_MIDI_DIN);
    switch (ports_mode) {
    case KK_PORTS_ALL: return outputs;
    case KK_PORTS_USB: return din | (1U << FM1_MIDI_USB);
    case KK_PORTS_BLE: return din | (1U << FM1_MIDI_BLE);
    default:
        /* AUTO: one computer-facing port only, so a Mac that sees both the
         * USB and the Bluetooth port never receives every command twice. */
        return din | ((ready & (1U << FM1_MIDI_USB)) ? 1U << FM1_MIDI_USB : 1U << FM1_MIDI_BLE);
    }
}
static void out(const uint8_t *m, size_t n, unsigned ports, unsigned local)
{
    if (local && local_on) (void)fm1_midi_post(m, n);
    groove_midi_kk_send(m, n, ports);
}
void kk_midi_note_on(unsigned channel, unsigned note, unsigned velocity)
{
    int s = slot(channel);
    if (s < 0 || note > 127) return;
    if (!velocity) { kk_midi_note_off(channel, note); return; }
    if (velocity > 127) velocity = 127;
    if (counts[s][note]++) return;          /* already sounding for another owner */
    const uint8_t m[3] = {(uint8_t)(0x90 | channel), (uint8_t)note, (uint8_t)velocity};
    out(m, 3, kk_midi_port_mask(), channel != KK_CH_CLIP && channel != KK_CH_STEP);
    ++revision;
}
void kk_midi_note_off(unsigned channel, unsigned note)
{
    int s = slot(channel);
    if (s < 0 || note > 127 || !counts[s][note]) return;
    if (--counts[s][note]) return;          /* another owner still holds it */
    const uint8_t m[3] = {(uint8_t)(0x80 | channel), (uint8_t)note, 0};
    /* Note Off goes to every port: the router drops it where the note was
     * never sent, and an AUTO port change between On and Off cannot hang. */
    out(m, 3, fm1_midi_output_ports(), channel != KK_CH_CLIP && channel != KK_CH_STEP);
    ++revision;
}
void kk_midi_cc(unsigned channel, unsigned controller, unsigned value)
{
    const uint8_t m[3] = {(uint8_t)(0xb0 | (channel & 15)), (uint8_t)(controller & 127), (uint8_t)(value & 127)};
    /* Ableton commands are external only; they must never edit the synth. */
    out(m, 3, kk_midi_port_mask(), channel != KK_CH_ABLETON && channel != KK_CH_CLIP && channel != KK_CH_STEP);
}
void kk_midi_message(const uint8_t *m, size_t n) { out(m, n, kk_midi_port_mask(), 1); }
void kk_midi_all_off(void)
{
    static const uint8_t channels[4] = {KK_CH_MELODIC, KK_CH_DRUM, KK_CH_CLIP, KK_CH_STEP};
    for (unsigned s = 0; s < 3; ++s)
        for (unsigned note = 0; note < 128; ++note)
            if (counts[s][note]) { counts[s][note] = 1; kk_midi_note_off(channels[s], note); }
}
unsigned kk_midi_count(unsigned channel, unsigned note)
{
    int s = slot(channel);
    return s < 0 || note > 127 ? 0 : counts[s][note];
}
unsigned kk_midi_sounding(unsigned channel, uint8_t *notes, unsigned max)
{
    int s = slot(channel);
    unsigned n = 0;
    if (s < 0) return 0;
    for (unsigned note = 0; note < 128 && n < max; ++note) if (counts[s][note]) notes[n++] = (uint8_t)note;
    return n;
}
uint32_t kk_midi_revision(void) { return revision; }
void kk_midi_set_ports(unsigned mode) { if (mode < KK_PORTS_MODES) ports_mode = (uint8_t)mode; }
unsigned kk_midi_ports_mode(void) { return ports_mode; }
void kk_midi_set_local(unsigned on)
{
    if (!on && local_on) {
        /* Silence the synth without touching the external note state. */
        static const uint8_t off[2][3] = {{0xb0 | KK_CH_MELODIC, 123, 0}, {0xb0 | KK_CH_DRUM, 123, 0}};
        (void)fm1_midi_post(off[0], 3); (void)fm1_midi_post(off[1], 3);
    }
    local_on = !!on;
}
unsigned kk_midi_local(void) { return local_on; }

/* ------------------------------------------------------------- input */
int kk_midi_input(unsigned source, const uint8_t *m, size_t n)
{
    (void)source;
    if (!m || !n || m[0] < 0x80 || m[0] >= 0xf0) return 0;
    unsigned type = m[0] & 0xf0U, ch = m[0] & 15U;
    unsigned d1 = n > 1 ? m[1] & 127U : 0, d2 = n > 2 ? m[2] & 127U : 0;
    if (ch == KK_CH_STEP && type == 0xb0) {
        /* Step Mode feedback from the Remote Script: pad colours and info. */
        if (d1 >= KK_STEP_CODE_CC && d1 < KK_STEP_CODE_CC + 16U) step_codes[d1 - KK_STEP_CODE_CC] = (uint8_t)d2;
        else if (d1 >= KK_STEP_PAGE_CC && d1 <= KK_STEP_BANK_CC) step_info[d1 - KK_STEP_PAGE_CC] = (uint8_t)d2;
        ++input_revision;
        return 1;
    }
    if (ch == KK_CH_CLIP) {
        /* Clip Mode feedback from the Remote Script: never a synth event. */
        if (type == 0xb0 && d1 >= KK_CLIP_STATE_CC && d1 < KK_CLIP_STATE_CC + 16U) {
            clip_states[d1 - KK_CLIP_STATE_CC] = (uint8_t)(d2 <= KK_CLIP_EMPTY_ARMED ? d2 : KK_CLIP_EMPTY);
            ++input_revision;
        } else if (type == 0xb0 && d1 == KK_CLIP_HELLO_CC) {
            host_protocol = (uint8_t)d2; if (!d2) host_tempo = 0; ++input_revision;
        } else if (type == 0xb0 && d1 == KK_HOST_HIGHLIGHT_CC) {
            host_highlight = (uint8_t)d2; ++input_revision;
        } else if (type == 0xb0 && d1 == KK_HOST_SCENE_CC) {
            host_scene = (uint8_t)d2; ++input_revision;
        } else if (type == 0xb0 && d1 == KK_HOST_TEMPO_MSB_CC) {
            tempo_msb = (uint8_t)d2;
        } else if (type == 0xb0 && d1 == KK_HOST_TEMPO_LSB_CC) {
            unsigned t = (unsigned)tempo_msb << 7 | d2;
            host_tempo = (uint16_t)(t >= 200 && t <= 9990 ? t : 0);   /* 20.0 .. 999.0 BPM */
            ++input_revision;
        }
        return 1;
    }
    int s = ch == KK_CH_MELODIC ? 0 : ch == KK_CH_DRUM ? 1 : -1;
    if (s >= 0) {
        if (type == 0x90 || type == 0x80) {
            remote_notes[s][d1] = (uint8_t)(type == 0x90 && d2);
            ++input_revision;
        } else if (type == 0xb0 && (d1 == 120 || d1 == 123)) {
            for (unsigned k = 0; k < 128; ++k) remote_notes[s][k] = 0;
            ++input_revision;
        }
    }
    /* Internal synth off: host notes light pads but stay silent here too. */
    return !local_on && type < 0xf0;
}
unsigned kk_midi_remote(unsigned channel, unsigned note)
{
    int s = channel == KK_CH_MELODIC ? 0 : channel == KK_CH_DRUM ? 1 : -1;
    return s >= 0 && note < 128 && remote_notes[s][note];
}
void kk_midi_remote_clear(void)
{
    for (unsigned s = 0; s < 2; ++s) for (unsigned k = 0; k < 128; ++k) remote_notes[s][k] = 0;
    for (unsigned p = 0; p < 16; ++p) clip_states[p] = KK_CLIP_UNKNOWN;
    host_protocol = 0; host_tempo = 0; host_scene = 0; host_highlight = 127;
    for (unsigned p = 0; p < 16; ++p) step_codes[p] = KK_STEPC_OUTSIDE;
    for (unsigned i = 0; i < sizeof step_info; ++i) step_info[i] = 0;
    ++input_revision;
}
unsigned kk_midi_host_tempo_x10(void) { return host_protocol ? host_tempo : 0; }
unsigned kk_midi_host_scene(void) { return host_protocol ? host_scene : 0; }
int kk_midi_host_highlight(void) { return host_protocol && host_highlight < 16 ? host_highlight : -1; }
unsigned kk_midi_clip_state(unsigned pad) { return pad < 16 ? clip_states[pad] : KK_CLIP_UNKNOWN; }
unsigned kk_midi_host_protocol(void) { return host_protocol; }
uint32_t kk_midi_input_revision(void) { return input_revision; }
void kk_midi_clip_sync_request(void) { kk_midi_cc(KK_CH_CLIP, KK_CLIP_SYNC_CC, 127); }
void kk_midi_step_mode(unsigned on) { kk_midi_cc(KK_CH_STEP, KK_STEP_SYNC_CC, on ? 127 : 0); }
unsigned kk_midi_step_code(unsigned pad) { return pad < 16 ? step_codes[pad] : KK_STEPC_OUTSIDE; }
unsigned kk_midi_step_info(unsigned cc)
{
    return cc >= KK_STEP_PAGE_CC && cc <= KK_STEP_BANK_CC ? step_info[cc - KK_STEP_PAGE_CC] : 0;
}

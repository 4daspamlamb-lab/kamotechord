/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Engine guard and saved settings.
 * Part of core/kk_controls.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* ----------------------------------------------------------- engine guard */
/* Engine values last pushed by the guard; reset with the rest of the state. */
static uint8_t synced_program = 255;
static uint16_t synced_bpm;
static uint32_t program_revision, synced_revision;

/* Settings layout (16 bytes, little endian), stored by app/project.c in the
 * journaled tuning sector. Octave is intentionally not stored. Bytes 10, 14
 * and 15 are device settings: SMK-37 (version 1) Repeat division, LED timing,
 * pad sensitivity; FM-1 (version 0x46) scale, root, velocity. */
#ifdef KK_DEVICE_FM1
enum { KK_SETTINGS_VERSION = 0x46, KK_SETTINGS_BYTES = 16 };
#else
enum { KK_SETTINGS_VERSION = 1, KK_SETTINGS_BYTES = 16 };
#endif
void kamotkeys_settings_read(uint8_t d[KK_SETTINGS_BYTES])
{
    memset(d, 0, KK_SETTINGS_BYTES);
    d[0] = KK_SETTINGS_VERSION; d[1] = (uint8_t)kk.bpm; d[2] = (uint8_t)(kk.bpm >> 8);
    d[3] = kk.arp_on; d[4] = kk.arp.mode; d[5] = kk.arp.direction; d[6] = kk.arp.division;
    d[7] = kk.arp.gate; d[8] = kk.arp.swing; d[9] = kk.arp.octaves;
    d[11] = kk.program; d[12] = (uint8_t)kk_midi_ports_mode(); d[13] = (uint8_t)kk_midi_local();
#ifdef KK_DEVICE_FM1
    d[10] = kk.scale.scale; d[14] = kk.scale.root; d[15] = kk.velocity;
#else
    d[10] = kk.repeat_division; d[14] = kk.rgb_timing; d[15] = kk.pad_sensitivity;
#endif
}
int kamotkeys_settings_apply(const uint8_t d[KK_SETTINGS_BYTES])
{
    unsigned bpm = d[1] | (unsigned)d[2] << 8;
    if (d[0] != KK_SETTINGS_VERSION || bpm < 30 || bpm > 300 || d[3] > 1 || d[4] >= KK_ARP_MODES ||
        d[5] >= KK_DIRS || d[6] >= KK_DIVS || d[7] < 10 || d[7] > 100 || d[8] > 50 ||
        d[9] < 1 || d[9] > 4 || d[11] > 127 || d[12] >= KK_PORTS_MODES || d[13] > 1)
        return -1;
#ifdef KK_DEVICE_FM1
    if (d[10] >= KK_SCALES || d[14] > 11 || d[15] < 1 || d[15] > 127) return -1;
#else
    if (d[10] >= KK_DIVS || d[14] >= SMK37_RGB_TIMINGS || d[15] > KK_PAD_SENSITIVITY_MAX) return -1;
#endif
    kk.bpm = (uint16_t)bpm;
    kk.arp = (struct kk_arp_settings){d[4], d[5], d[6], d[7], d[8], d[9]};
    kk.program = d[11];
    kk_midi_set_ports(d[12]); kk_midi_set_local(d[13]);
#ifdef KK_DEVICE_FM1
    kk.scale.scale = d[10]; kk.scale.root = d[14]; kk.velocity = d[15];
#else
    kk.repeat_division = d[10];
    kk.rgb_timing = d[14]; smk37_rgb_timing(kk.rgb_timing);
    kk.pad_sensitivity = d[15] ? d[15] : 3;   /* records before 0.44: linear */
#endif
    if (kk.arp_on != d[3]) { kk.arp_on = d[3]; reroute_all(); }
    kk_perform_guard();
    ++kk.edit_revision;
    return 0;
}
void kk_perform_guard(void)
{
    /* KamoteKeys owns arpeggiation; the engine's per-track ARP must stay off
     * or it would double every note. */
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) if (groove_arp_enabled(t)) groove_arp_set(t, 0);
    if (groove_track_role(0) == GROOVE_TRACK_OFF) (void)groove_track_role_set(0, GROOVE_TRACK_NORMAL);
    if (groove_track_role(7) == GROOVE_TRACK_OFF) (void)groove_track_role_set(7, GROOVE_TRACK_NORMAL);
    if (groove_source_type(0) != GROOVE_DX7) (void)groove_source_set(0, GROOVE_DX7);
    /* Channel 10 must reach a drum engine, never a melodic synth. */
    if (!groove_pad_count(7)) (void)groove_source_set(7, GROOVE_808);
    if (groove_midi_input_channel(0) != KK_CH_MELODIC) (void)groove_midi_input_set(0, KK_CH_MELODIC);
    if (groove_midi_input_channel(7) != KK_CH_DRUM) (void)groove_midi_input_set(7, KK_CH_DRUM);
    for (unsigned t = 1; t < 7; ++t) {
        unsigned ch = groove_midi_input_channel(t);
        if (ch == KK_CH_MELODIC || ch == KK_CH_DRUM) (void)groove_midi_input_set(t, t);
    }
    struct groove_view view;
    groove_get_view(0, 0, &view);
    if (synced_program != kk.program || program_revision != groove_project_revision()) {
        if (view.program != kk.program) groove_control_edit(0, (int)kk.program - (int)view.program, 0, 0);
        synced_program = kk.program; program_revision = groove_project_revision();
    }
    /* The engine tempo follows the KamoteKeys BPM (clock out, synced delay).
     * Edit once per change: never accumulate deltas against a stale view. */
    unsigned bpm = kk_tempo_bpm();
    if (bpm > 300) bpm = 300;                          /* engine range */
    if (synced_bpm != bpm || synced_revision != groove_project_revision()) {
        if (view.tempo && view.tempo != bpm) groove_tempo_edit((int)bpm - (int)view.tempo);
        synced_bpm = (uint16_t)bpm; synced_revision = groove_project_revision();
    }
}

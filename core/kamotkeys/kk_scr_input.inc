/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Screen input: BT keys, encoders, popups, note following.
 * Part of core/screen.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
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

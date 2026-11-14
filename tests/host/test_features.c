/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: 0.54+: ripple lights, Step Mode, Live tempo. */
#include "helpers.h"

void test_ripple(void)
{
    boot(); settle();
    unsigned pattern = kk.anim_pattern;
    show_on(); frames(140, 10); show_off();
    CHECK(!kk.ripple, "a 1.4 s light show does not toggle ripples");
    pattern = kk.anim_pattern; frames(20, 10);
    host_clear(); show_on(); frames(310, 10);
    CHECK(kk.ripple && kk.anim_pattern == pattern && !kk.anim_active, "3 s combo hold -> ripple on, pattern unchanged, show stops");
    show_off();
    CHECK(out_count(USB, CC15, 22) == 0 && out_count(USB, CC15, 25) == 0, "no Capture / Record sent");
    settle(); frames(40, 25); uint32_t idle = host_rgb[6];
    host_clear(); pad(1, 100);
    CHECK(!kk.anim_active, "pad still plays (no light show)");
    frames(16, 5);                                  /* ~80 ms: ring around pad 1 */
    CHECK(host_rgb[1] != 0 && host_rgb[1] != idle, "ripple reaches the neighbouring pad (%06x)", host_rgb[1]);
    pad(1, 0); frames(60, 25);
    CHECK(host_rgb[6] == idle, "ripple fades away (%06x vs %06x)", host_rgb[6], idle);
    host_clear(); key(36, 100); key(36, 0);
    CHECK(out_count(USB, ON1, -1) == 1, "keys still play with ripples on");
    show_on(); frames(310, 10); show_off();
    CHECK(!kk.ripple, "3 s combo hold again -> off");
}

/* 0.58: Step Mode (hold CHORD): pads and function keys on channel 14,
 * colours from the Remote Script. */
void test_step_mode(void)
{
    boot();
    host_clear(); btn(KK_BTN_CHORD, 1); host_ms += 700; frames(2, 1);
    CHECK(kk.mode == KK_MODE_STEP && out_count(USB, 0xBD, KK_STEP_SYNC_CC) == 1 && last_out(USB, 0xBD, 0) == (KK_STEP_SYNC_CC | 127 << 8),
          "hold CHORD -> Step Mode, tells the script (ch 14 CC 127 = 127)");
    btn(KK_BTN_CHORD, 0); CHECK(kk.mode == KK_MODE_STEP && kk_led_button(KK_BTN_REC) == 2, "release keeps Step Mode, SEQ REC lit");
    host_clear(); pad(3, 90); pad(3, 0);
    CHECK(out_count(USB, 0x9D, 38) == 1 && out_count(USB, 0x8D, 38) == 1 && host_local_n == 0, "pad 3 -> ch 14 note 38, never the synth");
    host_clear(); key(5, 100); key(6, 100); key(6, 0); key(8, 100); key(8, 0); key(5, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_DRUM) == 1 && out_count(USB, 0x9D, KK_STEP_PAGE_PREV) == 1 && out_count(USB, 0x9D, KK_STEP_PAGE_NEXT) == 1 &&
          out_count(USB, 0x8D, KK_STEP_DRUM) == 1, "0.73: F = DRUM (hold), F# / G# = page");
    host_clear(); key(2, 100); key(2, 0); key(4, 100); key(4, 0); key(7, 100); key(7, 0); key(9, 100); key(9, 0); key(10, 100); key(10, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_RESOLUTION) == 1 && out_count(USB, 0x9D, KK_STEP_FOLLOW) == 1 && out_count(USB, 0x9D, KK_STEP_LENGTH) == 1 &&
          out_count(USB, 0x9D, KK_STEP_BARS) == 1 && out_count(USB, 0x9D, KK_STEP_CLEAR) == 1,
          "0.73: D = RES, E = FOLLOW, G = LENGTH, A = BARS, A# = DELETE");
    host_clear(); key(0, 100); key(0, 0); key(11, 100); key(11, 0);
    CHECK(out_count(USB, 0x90, -1) == 2 && out_count(USB, 0x9D, -1) == 0, "C and B play notes");
    /* 0.62 key map: velocity = the last five white keys, repeats = top F# G# A#,
     * BARS = lowest F#; the lowest G# and third-octave C still play. */
    host_clear(); key(29, 100); key(29, 0); key(35, 100); key(35, 0); key(36, 100); key(36, 0); key(30, 100); key(30, 0); key(34, 100); key(34, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_VEL_1) == 1 && out_count(USB, 0x9D, KK_STEP_VEL_4) == 1 && out_count(USB, 0x9D, KK_STEP_NOTE_LEN) == 1 &&
          out_count(USB, 0x9D, KK_STEP_REPEAT_1) == 1 && out_count(USB, 0x9D, KK_STEP_REPEAT_3) == 1, "F..B = vel 1..4, top C = note length, F# / A# = repeat 1 / 3");
    host_clear(); key(24, 100); key(24, 0); key(28, 100); key(28, 0);
    CHECK(out_count(USB, 0x99, 48) == 1 && out_count(USB, 0x99, 52) == 1, "the key zone runs up to E3 (C3 = 48, E3 = 52)");
    host_clear(); key(1, 100); key(1, 0); key(3, 100); key(3, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_COPY) == 1 && out_count(USB, 0x9D, KK_STEP_PASTE) == 1, "C# = COPY, D# = PASTE");
    /* Second octave = key zone: drum keys (Move-style drum pads). */
    host_clear(); key(12, 100);
    CHECK(out_count(USB, 0x99, 36) == 1 && out_count(USB, 0x9C, 36) == 1 && out_count(USB, 0x8C, 36) == 1,
          "2nd-octave C = kick (ch 10 note 36) + selects pitch 36 (ch 13 tap)");
    pad(1, 90); pad(1, 0);
    CHECK(out_count(USB, 0x9D, 36) == 1 && out_count(USB, 0x8D, 36) == 1, "a pad after a drum key still sends its step");
    key(12, 0); CHECK(out_count(USB, 0x89, 36) == 1, "drum key release = note off");
    host_in(0xBD, KK_STEP_KEYOCT_CC, 65); host_clear(); key(23, 90); key(23, 0);
    CHECK(out_count(USB, 0x99, 59) == 1 && out_count(USB, 0x9C, 59) == 1, "octave from the script (+1): B = 59");
    tap(KK_BTN_ARP); host_clear(); key(14, 100); key(14, 0); tap(KK_BTN_ARP);
    CHECK(out_count(USB, 0x99, 50) == 1, "drum keys ignore the ARP");
    host_in(0xBD, KK_STEP_KEYOCT_CC, 64);
    /* OCT and SEQ go to the script in Step Mode. */
    int oct = kk.octave; host_clear(); tap(KK_BTN_OCT_UP); tap(KK_BTN_SEQ);
    CHECK(kk.octave == oct && kk.mode == KK_MODE_STEP && out_count(USB, 0x9D, KK_STEP_OCT_UP) == 1 && out_count(USB, 0x9D, KK_STEP_SCALE_KEYS) == 1,
          "Step Mode: OCT + / SEQ -> script, octave and mode unchanged");
    host_in(0xBD, KK_STEP_KEYS_CC, 1); host_clear(); key(14, 100); key(14, 0);
    int scale_note = kk_key_scale_note(14, kk.octave, &kk.scale);
    CHECK(scale_note > 0 && out_count(USB, 0x90, scale_note) == 1 && out_count(USB, 0x9C, scale_note) == 1 && kk_led_button(KK_BTN_SEQ) == 2,
          "scale keys: D plays its scale note on ch 1 and selects it, SEQ lit");
    int keys_were = (int)kk_midi_step_info(KK_STEP_KEYS_CC);
    /* 0.66: holding a repeat key + a pad previews the sound that many times. */
    host_in(0xBD, KK_STEP_KEYS_CC, 0); host_in(0xBD, KK_STEP_ACTIVE_CC, 38); host_clear();
    key(32, 100); pad(5, 90); pad(5, 0); frames(100, 2); key(32, 0);
    CHECK(out_count(USB, 0x99, 38) == 2 && out_count(USB, 0x89, 38) == 2, "repeat 2x + pad: snare previewed twice (%u)", out_count(USB, 0x99, 38));
    host_clear(); key(34, 100); pad(5, 90); pad(5, 0); frames(100, 2); key(34, 0);
    CHECK(out_count(USB, 0x99, 38) == 3, "then 3x: three times (%u)", out_count(USB, 0x99, 38));
    host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_clear(); frames(450, 5);
    CHECK(out_count(USB, 0xBD, KK_STEP_SYNC_CC) >= 1 && out_count(BLE, 0xBD, KK_STEP_SYNC_CC) >= 1, "Step Mode heartbeat on every port");
    host_in(0xBD, KK_STEP_KEYS_CC, (uint8_t)keys_were);
    /* 0.68: DRUM (G) held: pads = Drum Mode pads (same order and colours), they play and select. */
    host_in(0xBD, KK_STEP_KEYS_CC, 0); host_clear(); key(5, 100); pad(1, 100); pad(1, 0); pad(9, 100); pad(9, 0); pad(2, 100); pad(2, 0);
    CHECK(out_count(USB, 0x99, 36) == 1 && out_count(USB, 0x99, 37) == 1 && out_count(USB, 0x99, 38) == 1 &&
          out_count(USB, 0x9C, 36) == 1 && out_count(USB, 0x9C, 38) == 1 && out_count(USB, 0x9D, 36) == 0,
          "DRUM held: pad 1 / 9 / 2 = kick 36 / rim 37 / snare 38 on ch 10, selected on ch 13, no step");
    settle();
    CHECK(((host_rgb[0] >> 16) & 255) > ((host_rgb[0] >> 8) & 255) * 3 && ((host_rgb[1] >> 16) & 255) > (host_rgb[1] & 255),
          "DRUM held: Drum Mode colours (kick red %06x, snare orange %06x)", host_rgb[0], host_rgb[1]);
    key(5, 0); host_clear(); pad(1, 100); pad(1, 0);
    CHECK(out_count(USB, 0x9D, 36) == 1 && out_count(USB, 0x99, -1) == 0, "DRUM released: pads are steps again");
    /* 0.63: a locked chord + scale keys: a key picks the whole chord. */
    host_in(0xBD, KK_STEP_KEYS_CC, 1);
    kk.chord_lock = 1; kk.latched_mods = kk.chord_mods = KK_MOD_MIN;
    host_clear(); key(12, 100); key(12, 0);
    {
        int root = kk_key_scale_note(12, kk.octave, &kk.scale);
        CHECK(out_count(USB, 0x90, -1) == 3 && out_count(USB, 0x9C, -1) == 3 && out_count(USB, 0x9C, root) == 1 && host_out[0].b[0] == 0x9C &&
              host_out[2].b[0] == 0x9C, "locked MIN chord: 3 notes play, ch 13 gets all 3 note-ons first (root %d)", root);
    }
    kk.step_single = 1; host_clear(); key(12, 100); key(12, 0);
    CHECK(out_count(USB, 0x90, -1) == 1 && out_count(USB, 0x9C, -1) == 1, "chord toggle off: one note");
    kk.step_single = 0;
    kk.chord_lock = 0; kk.latched_mods = kk.chord_mods = 0;
    host_in(0xBD, KK_STEP_KEYS_CC, 0);
    /* 0.64: leaving Step Mode plays the clip (CC 127 = 0) unless PATCH is held (64). */
    host_clear(); tap(KK_BTN_REC); CHECK(kk.mode == KK_MODE_NORMAL && last_out(USB, 0xBD, 0) == KK_STEP_SYNC_CC, "exit: CC 127 = 0 (play)");
    tap(KK_BTN_REC); host_clear(); btn(KK_BTN_PATCH, 1); tap(KK_BTN_REC); btn(KK_BTN_PATCH, 0);
    CHECK(last_out(USB, 0xBD, 0) == (KK_STEP_SYNC_CC | 64 << 8) && kk.page == KK_PAGE_MAIN, "PATCH held: CC 127 = 64, no patch page");
    tap(KK_BTN_REC);
    host_clear(); host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBD, KK_STEP_CODE_CC + 0, KK_STEPC_NOTE_6); host_in(0xBD, KK_STEP_CODE_CC + 1, KK_STEPC_EMPTY);
    host_in(0xBD, KK_STEP_CODE_CC + 2, KK_STEPC_PLAY_NOTE); host_in(0xBD, KK_STEP_CODE_CC + 15, KK_STEPC_OUTSIDE);
    host_in(0xBD, KK_STEP_PAGE_CC, 1); host_in(0xBD, KK_STEP_PAGES_CC, 4);
    CHECK(host_local_n == 0, "step feedback never reaches the synth");
    settle();
    CHECK(((host_rgb[0] >> 16) & 255) > ((host_rgb[0] >> 8) & 255) * 3 / 2 && ((host_rgb[0] >> 16) & 255) >= KK_RGB_PEAK * 9 / 10, "note = bright amber (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] & 255) > ((host_rgb[1] >> 16) & 255) && (host_rgb[1] & 255) < KK_RGB_PEAK / 3, "empty = dim blue (%06x)", host_rgb[1]);
    CHECK(((host_rgb[2] >> 8) & 255) >= KK_RGB_PEAK * 9 / 10, "playhead on a note = green (%06x)", host_rgb[2]);
    CHECK(host_rgb[15] == 0, "outside the clip = off (%06x)", host_rgb[15]);
    frames(30, 5); shot("19_step_mode");
    host_clear(); tap(KK_BTN_CHORD);
    CHECK(kk.mode == KK_MODE_CLIP && out_count(USB, 0xBD, KK_STEP_SYNC_CC) == 1 && last_out(USB, 0xBD, 0) == KK_STEP_SYNC_CC, "tap CHORD -> Clip Mode, Step Mode ends (CC 127 = 0)");
    tap(KK_BTN_CHORD); CHECK(kk.mode == KK_MODE_NORMAL, "tap again -> Normal");
}

void test_live_tempo(void)
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

/* 0.69: every Step Mode and Clip Mode message over Bluetooth only, with the
 * BLE buffer often full: nothing may be lost or reordered. */
static unsigned ble_count(unsigned status, int data1) { return out_count(BLE, status, data1); }
static void ble_in(uint8_t a, uint8_t b, uint8_t c) { uint8_t m[3] = {a, b, c}; fm1_app_midi_received(2, m, 3); }
void test_bluetooth(void)
{
    boot();
    host_ready_ports = 4; frames(30, 5);                      /* Bluetooth only */
    ble_in(0xBF, KK_CLIP_HELLO_CC, 1);
    /* Step Mode in / out and every key type, with the BLE buffer full now and then. */
    host_clear(); host_ble_busy = 2; tap(KK_BTN_REC); frames(5, 5);
    CHECK(kk.mode == KK_MODE_STEP && ble_count(0xBD, KK_STEP_SYNC_CC) >= 1, "BLE: Step Mode on reaches Live despite a full buffer");
    host_clear();
    host_ble_busy = 3; pad(1, 100); pad(1, 0);                /* a step */
    host_ble_busy = 1; key(1, 100); key(1, 0);                /* COPY */
    host_ble_busy = 2; key(12, 100); key(12, 0);              /* key zone: kick (ch 10) + select (ch 13) */
    host_ble_busy = 1; key(7, 100); pad(2, 100); pad(2, 0); key(7, 0);   /* LENGTH + pad */
    host_ble_busy = 2; tap(KK_BTN_OCT_UP); tap(KK_BTN_SEQ); tap(KK_BTN_PARA); tap(KK_BTN_LEFT); tap(KK_BTN_RIGHT);
    host_ble_busy = 1; key(5, 100); pad(9, 100); pad(9, 0); key(5, 0);   /* DRUM view pad: rim */
    frames(20, 5);
    CHECK(ble_count(0x9D, 36) == 1 && ble_count(0x8D, 36) == 1, "BLE: step pad on/off");
    CHECK(ble_count(0x9D, KK_STEP_COPY) == 1 && ble_count(0x8D, KK_STEP_COPY) == 1, "BLE: COPY on/off");
    CHECK(ble_count(0x99, 36) == 1 && ble_count(0x9C, 36) == 1 && ble_count(0x8C, 36) == 1, "BLE: key zone sound + select");
    CHECK(ble_count(0x9D, KK_STEP_LENGTH) == 1 && ble_count(0x9D, 37) == 1, "BLE: LENGTH + pad");
    CHECK(ble_count(0x9D, KK_STEP_OCT_UP) == 1 && ble_count(0x9D, KK_STEP_SCALE_KEYS) == 1 && ble_count(0x9D, KK_STEP_PLAY) == 1 &&
          ble_count(0x9D, KK_STEP_PAGE_PREV) == 1 && ble_count(0x9D, KK_STEP_PAGE_NEXT) == 1, "BLE: OCT, SEQ, PARA, ◀, ▶");
    CHECK(ble_count(0x99, 37) == 1 && ble_count(0x9C, 37) == 1, "BLE: DRUM view pad plays + selects");
    CHECK(out_count(USB, 0x9D, -1) == 0, "BLE only: nothing goes to USB");
    {   /* order kept: COPY note-on comes after the step pad's note-off */
        int off = -1, copy = -1;
        for (unsigned i = 0; i < host_out_n; ++i) {
            if (host_out[i].port != BLE) continue;
            if (off < 0 && host_out[i].b[0] == 0x8D && host_out[i].b[1] == 36) off = (int)i;
            if (copy < 0 && host_out[i].b[0] == 0x9D && host_out[i].b[1] == KK_STEP_COPY) copy = (int)i;
        }
        CHECK(off >= 0 && copy > off, "BLE: messages stay in order (%d, %d)", off, copy);
    }
    /* Feedback from Live over Bluetooth drives the pads. */
    ble_in(0xBD, KK_STEP_CODE_CC + 3, KK_STEPC_NOTE_6); settle();
    CHECK(kk_midi_step_code(3) == KK_STEPC_NOTE_6 && ((host_rgb[3] >> 16) & 255) >= KK_RGB_PEAK * 9 / 10, "BLE: step colours from Live");
    host_clear(); host_ble_busy = 1; frames(450, 5);
    CHECK(ble_count(0xBD, KK_STEP_SYNC_CC) >= 1, "BLE: Step Mode heartbeat");
    host_clear(); host_ble_busy = 2; tap(KK_BTN_REC); frames(5, 5);
    CHECK(kk.mode == KK_MODE_NORMAL && ble_count(0xBD, KK_STEP_SYNC_CC) == 1, "BLE: Step Mode off reaches Live");
    /* Clip Mode over Bluetooth. */
    tap(KK_BTN_CHORD); host_clear();
    host_ble_busy = 2; pad(3, 100); pad(3, 0); key(1, 100); key(1, 0); key(8, 100); key(8, 0);
    host_ble_busy = 1; key(10, 100); pad(4, 100); pad(4, 0); key(10, 0); key(30, 100); key(30, 0); key(33, 100); key(33, 0);
    frames(20, 5);
    CHECK(ble_count(0x9F, 84 + 2) == 1 && ble_count(0x9F, KK_CLIP_COPY_NOTE) == 1 && ble_count(0x9F, KK_CLIP_ARRANGE_NOTE) == 1 &&
          ble_count(0x9F, 68 + 3) == 1 && ble_count(0xBE, KK_CC_SCENE_LAUNCH) == 2 && ble_count(0xBE, KK_CC_TRACKS_LEFT) == 2,
          "BLE: clip select, COPY, ARRANGE, DELETE, scene launch, track view");
    ble_in(0xBF, KK_CLIP_STATE_CC + 0, KK_CLIP_PLAYING); settle();
    CHECK(kk_midi_clip_state(0) == KK_CLIP_PLAYING, "BLE: clip states from Live");
    tap(KK_BTN_CHORD); host_ready_ports = 6; host_ble_busy = 0; frames(30, 5);
    /* 0.71: USB and BLE both up, the script greets on BLE: its traffic goes to BLE. */
    ble_in(0xBF, KK_CLIP_HELLO_CC, 1);
    CHECK(kk_midi_host_port() == 2 && kk_midi_rx_count(2) > 0, "the script's port is remembered (BLE)");
    tap(KK_BTN_CHORD); host_clear(); pad(3, 100); pad(3, 0); key(1, 100); key(1, 0);
    CHECK(ble_count(0x9F, 84 + 2) == 1 && out_count(USB, 0x9F, -1) == 0 && ble_count(0x9F, KK_CLIP_COPY_NOTE) == 1,
          "Clip Mode goes to the script's port (BLE), not AUTO's USB");
    tap(KK_BTN_CHORD);
    { uint8_t m[3] = {0xBF, KK_CLIP_HELLO_CC, 1}; fm1_app_midi_received(1, m, 3); }
    CHECK(kk_midi_host_port() == 1, "script greets on USB again: back to USB");
}

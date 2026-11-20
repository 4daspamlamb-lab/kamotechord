/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: 0.54+: ripple lights, Step Mode, Live tempo. */
#include "helpers.h"

void test_ripple(void)
{
    boot(); settle();
    unsigned pattern = kk.anim_pattern;
    show_on(); frames(250, 10); show_off();
    CHECK(!kk.ripple, "a long light show does not toggle ripples");
    pattern = kk.anim_pattern; frames(150, 10);
    /* 0.77: three quick light-show starts (within 1.2 s) = ripple lights on / off. */
    host_clear(); show_on(); frames(10, 10); show_off(); frames(10, 10); show_on(); frames(10, 10); show_off(); frames(10, 10);
    CHECK(!kk.ripple, "two quick starts: still a light show");
    show_on();
    CHECK(kk.ripple && kk.anim_pattern == pattern && !kk.anim_active, "third quick start -> ripple on, pattern unchanged, show stops");
    show_off();
    CHECK(out_count(USB, CC15, 22) == 0 && out_count(USB, CC15, 25) == 0, "no Capture / Record sent");
    frames(80, 5); uint32_t giant = host_rgb[3] | host_rgb[4];      /* 0.80: ~400 ms after switching on */
    settle(); frames(120, 25); uint32_t idle = host_rgb[6];
    CHECK(giant && giant != (host_rgb[3] | host_rgb[4]), "0.80: giant centre ripple when ripples turn on (%06x vs %06x)", giant, host_rgb[3] | host_rgb[4]);
    host_clear(); pad(1, 100);
    CHECK(!kk.anim_active, "pad still plays (no light show)");
    frames(16, 5);                                  /* ~80 ms: ring around pad 1 */
    CHECK(host_rgb[1] != 0 && host_rgb[1] != idle, "ripple reaches the neighbouring pad (%06x)", host_rgb[1]);
    pad(1, 0); frames(60, 25);
    CHECK(host_rgb[6] == idle, "ripple fades away (%06x vs %06x)", host_rgb[6], idle);
    host_clear(); key(24, 100); key(24, 0);
    CHECK(out_count(USB, ON1, -1) == 1, "keys still play with ripples on");
    frames(150, 10); show_on(); show_off(); show_on(); show_off(); show_on(); show_off();
    CHECK(!kk.ripple, "three quick starts again -> off");
}

/* 0.79: K1 changes Live's tempo while Live is the clock. */
void test_k1_live_tempo(void)
{
    boot(); host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBF, KK_HOST_TEMPO_MSB_CC, 1200 >> 7); host_in(0xBF, KK_HOST_TEMPO_LSB_CC, 1200 & 127);
    host_clear(); kk_edit_bpm(2);
    CHECK(last_out(USB, CC15, 0) == (KK_CC_TEMPO | 66 << 8), "K1 +2 with Live connected -> CC 37 = 66");
    unsigned bpm = kk.bpm; host_in(0xBF, KK_CLIP_HELLO_CC, 0); kk_edit_bpm(1);
    CHECK(kk.bpm == bpm + 1, "without Live: the keyboard's own tempo");
}

/* 0.58: Step Mode (hold CHORD): pads and function keys on channel 14,
 * colours from the Remote Script. */
void test_step_mode(void)
{
    boot();
    /* 0.80: hold CHORD = Clip Mode while held, back on release; a tap stays. */
    btn(KK_BTN_CHORD, 1); CHECK(kk.mode == KK_MODE_CLIP, "CHORD press -> Clip Mode at once");
    host_ms += 700; frames(2, 1); btn(KK_BTN_CHORD, 0); CHECK(kk.mode == KK_MODE_NORMAL, "held CHORD released -> back to Normal");
    tap(KK_BTN_SCALE); btn(KK_BTN_CHORD, 1); pad(2, 90); pad(2, 0); btn(KK_BTN_CHORD, 0);
    CHECK(kk.mode == KK_MODE_PAD_SCALE, "CHORD + pad, quick release -> back to Pad Scale");
    tap(KK_BTN_CHORD); CHECK(kk.mode == KK_MODE_CLIP, "tap CHORD -> stays in Clip Mode");
    tap(KK_BTN_CHORD); CHECK(kk.mode == KK_MODE_NORMAL, "tap CHORD again -> Normal");
    host_clear(); tap(KK_BTN_REC);
    CHECK(kk.mode == KK_MODE_STEP && out_count(USB, 0xBD, KK_STEP_SYNC_CC) == 1 && last_out(USB, 0xBD, 1) == (KK_STEP_SYNC_CC | 127 << 8) && last_out(USB, 0xBD, 0) == KK_STEP_AUTO_STATE_CC,
          "SEQ REC -> Step Mode, tells the script (ch 14 CC 127 = 127)");
    CHECK(kk_led_button(KK_BTN_REC) == 2, "SEQ REC lit");
    btn(KK_BTN_CHORD, 1); host_ms += 700; frames(2, 1); btn(KK_BTN_CHORD, 0);
    CHECK(kk.mode == KK_MODE_STEP, "held CHORD from Step Mode -> back to Step Mode");
    host_clear(); pad(3, 90); pad(3, 0);
    CHECK(out_count(USB, 0x9D, 38) == 1 && out_count(USB, 0x8D, 38) == 1 && host_local_n == 0, "pad 3 -> ch 14 note 38, never the synth");
    host_clear(); key(11, 100); key(6, 100); key(6, 0); key(8, 100); key(8, 0); key(11, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_DRUM) == 1 && out_count(USB, 0x9D, KK_STEP_PAGE_PREV) == 1 && out_count(USB, 0x9D, KK_STEP_PAGE_NEXT) == 1 &&
          out_count(USB, 0x8D, KK_STEP_DRUM) == 1, "0.80: B = DRUM (hold), F# / G# = page");
    host_clear(); key(2, 100); key(2, 0); key(4, 100); key(4, 0); key(7, 100); key(7, 0); key(9, 100); key(9, 0); key(10, 100); key(10, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_RESOLUTION) == 1 && out_count(USB, 0x9D, KK_STEP_FOLLOW) == 1 && out_count(USB, 0x9D, KK_STEP_LENGTH) == 1 &&
          out_count(USB, 0x9D, KK_STEP_BARS) == 1 && out_count(USB, 0x9D, KK_STEP_CLEAR) == 1,
          "0.73: D = RES, E = FOLLOW, G = LENGTH, A = BARS, A# = DELETE");
    host_clear(); key(0, 100); key(0, 0); key(5, 100); key(5, 0);
    CHECK(out_count(USB, 0x90, -1) == 0 && out_count(USB, 0x9D, KK_STEP_NOTE_LEN) == 1 && out_count(USB, 0x8D, KK_STEP_NOTE_LEN) == 1 &&
          out_count(USB, 0x9D, KK_STEP_DOUBLE) == 1, "0.99: bottom C = NOTE LENGTH (no note), F = DOUBLE");
    /* 0.91: the top octave is the Live strip in Step Mode too; the key zone is 16 drum keys. */
    host_clear(); key(33, 100); key(33, 0); key(35, 100); key(35, 0);
    CHECK(out_count(USB, CC15, KK_CC_MUTE) == 2 && out_count(USB, CC15, KK_CC_SOLO) == 2 && out_count(USB, 0x9D, -1) == 0,
          "Step Mode: top A / B = mute / solo, no step functions");
    host_clear(); key(24, 100); key(24, 0); key(27, 100); key(27, 0); key(28, 100); key(28, 0);
    CHECK(out_count(USB, 0x99, 48) == 1 && out_count(USB, 0x99, 51) == 1 && out_count(USB, 0x99, 52) == 0,
          "the key zone runs up to D#3 (16 sounds: 36 .. 51)");
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
    /* 0.91: hold a step + E1 / E2 / E3 = its velocity / length / repeats (ch 14 CC 20 / 21 / 22). */
    host_in(0xBD, KK_STEP_KEYS_CC, 0); host_clear(); unsigned bpm_was = kk.bpm, root_was = kk.scale.root;
    pad(5, 90); kk.encoder[0] = 2; kk.encoder[1] = -1; kk.encoder[2] = 1; frames(3, 20); groove_midi_poll();
    CHECK(out_count(USB, 0xBD, KK_STEP_EDIT_CC) == 1 && out_count(USB, 0xBD, KK_STEP_EDIT_CC + 1) == 1 && out_count(USB, 0xBD, KK_STEP_EDIT_CC + 2) == 1 &&
          kk.bpm == bpm_was && kk.scale.root == root_was, "step held: E1-E3 edit the step, not BPM / key");
    pad(5, 0);
    host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_clear(); frames(450, 5);
    CHECK(out_count(USB, 0xBD, KK_STEP_SYNC_CC) >= 1 && out_count(BLE, 0xBD, KK_STEP_SYNC_CC) >= 1, "Step Mode heartbeat on every port");
    host_in(0xBD, KK_STEP_KEYS_CC, (uint8_t)keys_were);
    /* 0.68: DRUM (G) held: pads = Drum Mode pads (same order and colours), they play and select. */
    host_in(0xBD, KK_STEP_KEYS_CC, 0); host_clear(); key(11, 100); pad(1, 100); pad(1, 0); pad(9, 100); pad(9, 0); pad(2, 100); pad(2, 0);
    CHECK(out_count(USB, 0x99, 36) == 1 && out_count(USB, 0x99, 37) == 1 && out_count(USB, 0x99, 38) == 1 &&
          out_count(USB, 0x9C, 36) == 1 && out_count(USB, 0x9C, 38) == 1 && out_count(USB, 0x9D, 36) == 0,
          "DRUM held: pad 1 / 9 / 2 = kick 36 / rim 37 / snare 38 on ch 10, selected on ch 13, no step");
    settle();
    CHECK(((host_rgb[0] >> 16) & 255) > ((host_rgb[0] >> 8) & 255) * 3 && ((host_rgb[1] >> 16) & 255) > (host_rgb[1] & 255),
          "DRUM held: Drum Mode colours (kick red %06x, snare orange %06x)", host_rgb[0], host_rgb[1]);
    key(11, 0); host_clear(); pad(1, 100); pad(1, 0);
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
    host_ble_busy = 1; key(11, 100); pad(9, 100); pad(9, 0); key(11, 0);   /* DRUM view pad: rim */
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
    host_ble_busy = 2; pad(3, 100); pad(3, 0); key(1, 100); key(1, 0); key(2, 100); key(2, 0);
    host_ble_busy = 1; key(10, 100); pad(4, 100); pad(4, 0); key(10, 0); key(36, 100); key(30, 100); key(30, 0); key(29, 100); key(29, 0); key(36, 0);
    frames(20, 5);
    CHECK(ble_count(0x9F, 38) == 1 && ble_count(0x9F, KK_CLIP_COPY_NOTE) == 1 && ble_count(0x9F, KK_CLIP_ARRANGE_NOTE) == 1 &&
          ble_count(0x9F, 68 + 3) == 1 && ble_count(0xBE, KK_CC_SCENE_LAUNCH) == 2 && ble_count(0xBE, KK_CC_TRACKS_LEFT) == 2,
          "BLE: clip select, COPY, ARRANGE, DELETE, SHIFT scene launch / track");
    ble_in(0xBF, KK_CLIP_STATE_CC + 0, KK_CLIP_PLAYING); settle();
    CHECK(kk_midi_clip_state(0) == KK_CLIP_PLAYING, "BLE: clip states from Live");
    tap(KK_BTN_CHORD); host_ready_ports = 6; host_ble_busy = 0; frames(30, 5);
    /* 0.71: USB and BLE both up, the script greets on BLE: its traffic goes to BLE. */
    ble_in(0xBF, KK_CLIP_HELLO_CC, 1);
    CHECK(kk_midi_host_port() == 2 && kk_midi_rx_count(2) > 0, "the script's port is remembered (BLE)");
    tap(KK_BTN_CHORD); host_clear(); pad(3, 100); pad(3, 0); key(1, 100); key(1, 0);
    CHECK(ble_count(0x9F, 38) == 1 && out_count(USB, 0x9F, -1) == 0 && ble_count(0x9F, KK_CLIP_COPY_NOTE) == 1,
          "Clip Mode goes to the script's port (BLE), not AUTO's USB");
    tap(KK_BTN_CHORD);
    { uint8_t m[3] = {0xBF, KK_CLIP_HELLO_CC, 1}; fm1_app_midi_received(1, m, 3); }
    CHECK(kk_midi_host_port() == 1, "script greets on USB again: back to USB");
}

/* 0.86: top-octave Live keys, track auto-repeat, Pad Scale octave lighting, key / scale saved. */
void test_top_keys_and_key_memory(void)
{
    boot();
    static const struct { uint8_t key, cc; } map[] = {
        {34, KK_CC_CLIP_DELETE}, {32, KK_CC_FINISH}, {30, KK_CC_CLIP_PLAY},
        {35, KK_CC_SOLO}, {33, KK_CC_MUTE}, {31, KK_CC_ARM}, {29, KK_CC_QUANTIZE}};   /* 0.87 order */
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; ++i) {
        host_clear(); key(map[i].key, 100); key(map[i].key, 0);
        CHECK(out_count(USB, CC15, map[i].cc) == 2 && out_count(USB, ON1, -1) == 0,
              "Normal: top key %u -> CC %u press + release, no note", map[i].key, map[i].cc);
    }
    host_clear(); key(28, 100); key(28, 0); CHECK(out_count(USB, ON1, -1) == 1, "top E plays a note again (0.87)");
    /* 0.88: top C = SHIFT (silent); held: G# / A# scene up / down, F / G track left / right. */
    host_clear(); key(36, 100); CHECK(host_out_n == 0, "top C alone: silent");
    key(32, 100); key(32, 0); key(34, 100); key(34, 0); key(29, 100); key(29, 0); key(31, 100); key(31, 0);
    CHECK(out_count(USB, CC15, KK_CC_SCENE_UP) == 2 && out_count(USB, CC15, KK_CC_SCENE_DOWN) == 2 &&
          out_count(USB, CC15, KK_CC_TRACKS_LEFT) == 2 && out_count(USB, CC15, KK_CC_TRACKS_RIGHT) == 2 &&
          out_count(USB, CC15, KK_CC_FINISH) == 0 && out_count(USB, CC15, KK_CC_CLIP_DELETE) == 0 &&
          out_count(USB, CC15, KK_CC_QUANTIZE) == 0 && out_count(USB, CC15, KK_CC_ARM) == 0,
          "SHIFT + G# / A# / F / G = scene up / down, track left / right");
    host_clear(); key(33, 100); key(33, 0); key(35, 100); key(35, 0); key(30, 100); key(30, 0);
    CHECK(out_count(USB, CC15, KK_CC_CLIP_DUPLICATE) == 2 && out_count(USB, CC15, KK_CC_LOOP_DOUBLE) == 2 && out_count(USB, CC15, KK_CC_SCENE_LAUNCH) == 2,
          "0.91 SHIFT + A / B / F# = duplicate clip / double loop / launch scene");
    /* SHIFT + E: fixed length OFF -> 1 -> 2 -> 4 -> 8 -> OFF, nothing sent; KNOB BANK sends it first. */
    host_clear(); unsigned lens[6];
    for (unsigned i = 0; i < 6; ++i) { lens[i] = kk_rec_bars(); key(28, 100); key(28, 0); }
    CHECK(lens[0] == 0 && lens[1] == 1 && lens[2] == 2 && lens[3] == 4 && lens[4] == 8 && lens[5] == 0 && host_out_n == 0,
          "SHIFT + E cycles the fixed length silently");
    key(28, 100); key(28, 0); key(28, 100); key(28, 0);   /* the loop left 1 bar: two more = 4 bars */
    key(36, 0); host_clear(); tap(KK_BTN_KNOB_BANK); groove_midi_poll();
    CHECK(out_count(USB, CC15, KK_CC_REC_LENGTH) == 1 && last_out(USB, CC15, 2) == (KK_CC_REC_LENGTH | 4 << 8) && out_count(USB, CC15, KK_CC_RECORD) == 2,
          "KNOB BANK: length 4 then Session Record");
    { uint8_t saved[16]; kamotkeys_settings_read(saved); kk.rec_length = 0;
      CHECK(kamotkeys_settings_apply(saved) == 0 && kk_rec_bars() == 4, "fixed length saved"); kk.rec_length = 0; }
    key(36, 100);
    key(36, 0);
    /* 0.90: PATCH + top G = arm without disarming the others; the Patch page stays closed. */
    host_clear(); btn(KK_BTN_PATCH, 1); key(31, 100); key(31, 0); btn(KK_BTN_PATCH, 0);
    CHECK(out_count(USB, CC15, KK_CC_ARM_ADD) == 2 && out_count(USB, CC15, KK_CC_ARM) == 0 && kk.page == KK_PAGE_MAIN,
          "PATCH + G -> CC 51, no Patch page");
    key(36, 100);
    key(36, 0); host_clear(); key(32, 100); key(32, 0); CHECK(out_count(USB, CC15, KK_CC_FINISH) == 2, "SHIFT released: G# = finish again");
    host_clear(); key(25, 100); key(25, 0); CHECK(out_count(USB, ON1, -1) == 1, "top C# still plays");
    tap(KK_BTN_SEQ); host_clear(); key(33, 100); key(33, 0);
    CHECK(out_count(USB, CC15, KK_CC_MUTE) == 2, "Key Scale: top A = MUTE too"); tap(KK_BTN_SEQ);
    tap(KK_BTN_REC); host_clear(); key(33, 100); key(33, 0);
    CHECK(out_count(USB, CC15, KK_CC_MUTE) == 2 && out_count(USB, 0x9D, -1) == 0, "0.91 Step Mode: top A = MUTE too"); tap(KK_BTN_REC);
    /* Holding SHIFT + G repeats the track move every 0.5 s (any mode). */
    tap(KK_BTN_CHORD); host_clear(); key(36, 100); key(31, 100);
    CHECK(out_count(USB, CC15, KK_CC_TRACKS_RIGHT) == 1, "SHIFT + G press: one track right");
    host_ms += 400; frames(2, 1); CHECK(out_count(USB, CC15, KK_CC_TRACKS_RIGHT) == 1, "no repeat before 0.5 s");
    host_ms += 200; frames(2, 1); host_ms += 500; frames(2, 1); host_ms += 500; frames(2, 1);
    unsigned presses = 0;
    for (unsigned i = 0; i < host_out_n; ++i) presses += host_out[i].b[0] == CC15 && host_out[i].b[1] == KK_CC_TRACKS_RIGHT && host_out[i].b[2] == 127;
    CHECK(presses == 4, "held 1.6 s: 3 repeats (%u presses)", presses);
    key(31, 0); key(36, 0); host_clear(); host_ms += 600; frames(2, 1); CHECK(out_count(USB, CC15, KK_CC_TRACKS_RIGHT) == 0, "released: stops");
    tap(KK_BTN_CHORD);
    /* Pad Scale: a key lights the pads with the same note in any octave. */
    kk.scale.scale = 0; kk.scale.root = 0; tap(KK_BTN_SCALE); settle();
    unsigned lit_before = 0; for (unsigned p = 0; p < KK_PADS; ++p) lit_before += kk_pad_lit(p);
    kk.octave = -2; key(0, 100);   /* C1, octaves below the pads */
    unsigned lit = 0; for (unsigned p = 0; p < KK_PADS; ++p) lit += kk_pad_lit(p);
    CHECK(lit_before == 0 && lit == 0, "0.89 Pad Scale: a C in another octave lights no pad (%u)", lit);
    key(0, 0); kk.octave = 0;
    /* The selected clip's notes (ch 16 notes from the script) light their pad, and never sound. */
    unsigned ch; int n0 = kk_pad_note(0, &ch); host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_clear();
    host_in(0x9F, (uint8_t)n0, 100); host_in(0x9F, (uint8_t)(n0 + 12 * 4), 100);
    lit = 0; for (unsigned p = 0; p < KK_PADS; ++p) lit += kk_pad_lit(p);
    CHECK(kk_pad_lit(0) && lit == 1 && host_local_n == 0, "clip note lights exactly its pad (%u lit), no sound", lit);
    host_in(0x8F, (uint8_t)n0, 0); CHECK(!kk_pad_lit(0), "clip note off: pad dark again");
    host_in(0x8F, (uint8_t)(n0 + 48), 0); host_in(0xBF, KK_CLIP_HELLO_CC, 0);
    tap(KK_BTN_SCALE);
    /* Key and scale are saved (SMK-37). */
    kk.scale.scale = 10; kk.scale.root = 9;
    uint8_t saved[16]; kamotkeys_settings_read(saved);
    kk.scale.scale = 0; kk.scale.root = 0;
    CHECK(kamotkeys_settings_apply(saved) == 0 && kk.scale.scale == 10 && kk.scale.root == 9, "A Minor Pentatonic restored");
    kk.scale.root = 11; kamotkeys_settings_read(saved); kk.scale.root = 0;
    CHECK(kamotkeys_settings_apply(saved) == 0 && kk.scale.root == 11, "root B (bit 3) restored");
    saved[3] &= 0x3F; saved[4] = 0; kk.scale.scale = 3;
    CHECK(kamotkeys_settings_apply(saved) == 0 && kk.scale.scale == 3, "older record: key / scale untouched");
    saved[3] |= 0x80; saved[4] = 30; CHECK(kamotkeys_settings_apply(saved) < 0, "invalid scale rejected");
    kk.scale.scale = 0; kk.scale.root = 0;
}

/* 0.94: latched light show and ripples from the ARP. */
void test_show_latch(void)
{
    boot(); settle();
    host_clear(); btn(KK_BTN_GLOBE, 1); host_ms += 3100; frames(2, 1); btn(KK_BTN_GLOBE, 0); frames(2, 1);
    CHECK(kk.anim_active && kk.show_latched && host_out_n == 0, "GLOBE held 3 s: the light show stays on");
    /* Pads 1 + 9 held: next pattern every 5 s; pads 8 + 16: every 40 s. Nothing sounds. */
    unsigned pat = kk.anim_pattern; host_clear();
    pad(1, 100); pad(9, 100); pad(1, 0); pad(9, 0); groove_midi_poll();
    CHECK(kk.show_latched && kk.show_cycle_s == 5 && host_out_n == 0, "pads 1 + 9: cycles every 5 s (%u), silent", kk.show_cycle_s);
    host_ms += 4000; frames(2, 1); CHECK(kk.anim_pattern == pat, "not yet after 4 s");
    host_ms += 1500; frames(2, 1); CHECK(kk.anim_pattern == (pat + 1) % KK_ANIMATIONS, "next pattern after 5 s");
    pad(16, 100); pad(8, 100); pad(16, 0); pad(8, 0);
    CHECK(kk.show_cycle_s == 40, "pads 8 + 16: every 40 s (%u)", kk.show_cycle_s);
    pad(3, 100); pad(4, 100); pad(3, 0); pad(4, 0);
    CHECK(kk.show_cycle_s == 40, "two pads in different columns: unchanged");
    /* GLOBE held 3 s again: back to normal. */
    btn(KK_BTN_GLOBE, 1); host_ms += 3100; frames(2, 1);
    CHECK(!kk.anim_active && !kk.show_latched, "GLOBE held 3 s again: light show off");
    btn(KK_BTN_GLOBE, 0); frames(2, 1);
    CHECK(!kk.anim_active && !kk.show_latched, "and stays off after release");
    host_clear(); tap(KK_BTN_KNOB_BANK); groove_midi_poll();
    CHECK(out_count(USB, CC15, KK_CC_RECORD) == 2, "KNOB BANK records");
    /* Ripples follow the ARP: 2 s after the key press its own ring is long gone, the arpeggio keeps them coming. */
    host_ms += 2000; show_on(); show_off(); show_on(); show_off(); show_on(); show_off();
    CHECK(kk.ripple, "ripples on");
    settle(); frames(400, 5); uint32_t idle[KK_PADS]; for (unsigned p = 0; p < KK_PADS; ++p) idle[p] = host_rgb[p];
    tap(KK_BTN_ARP); key(12, 100); frames(400, 5);
    unsigned changed = 0;
    for (unsigned f = 0; f < 40; ++f) { frames(1, 5); for (unsigned p = 0; p < KK_PADS; ++p) changed += host_rgb[p] != idle[p]; }
    CHECK(changed > 0, "ARP notes keep sending ripples (%u)", changed);
    key(12, 0); tap(KK_BTN_ARP);
}

/* 0.96: every light-show pattern, any speed, pattern changes and ripples:
 * no pad jumps hard on or off from one 5 ms frame to the next. */
static unsigned max_step(unsigned n)
{
    unsigned worst = 0; uint32_t last[KK_PADS];
    for (unsigned p = 0; p < KK_PADS; ++p) last[p] = host_rgb[p];
    for (unsigned f = 0; f < n; ++f) {
        frames(1, 5);
        for (unsigned p = 0; p < KK_PADS; ++p) {
            for (unsigned sh = 0; sh < 24; sh += 8) {
                int d = (int)((host_rgb[p] >> sh) & 255) - (int)((last[p] >> sh) & 255);
                if (d < 0) d = -d;
                if ((unsigned)d > worst) worst = (unsigned)d;
            }
            last[p] = host_rgb[p];
        }
    }
    return worst;
}
void test_smooth_show(void)
{
    boot(); settle();
    fader(3, 127); fader(0, 127); frames(20, 5);           /* full brightness, fastest show */
    unsigned worst = 0, w;
    btn(KK_BTN_GLOBE, 1); w = max_step(100); if (w > worst) worst = w;   /* fade in */
    btn(KK_BTN_GLOBE, 0); w = max_step(100); if (w > worst) worst = w;   /* fade out */
    CHECK(worst <= 32, "light show fades in and out (worst step %u)", worst);
    worst = 0;
    btn(KK_BTN_GLOBE, 1); frames(650, 5); btn(KK_BTN_GLOBE, 0); frames(200, 5);   /* latched */
    CHECK(kk.show_latched, "latched");
    for (unsigned i = 0; i < KK_ANIMATIONS; ++i) {
        w = max_step(400); if (w > worst) worst = w;     /* 2 s of the pattern at top speed */
        btn(KK_BTN_GLOBE, 1); w = max_step(4); if (w > worst) worst = w; btn(KK_BTN_GLOBE, 0);   /* next pattern */
    }
    CHECK(worst <= 32, "every pattern at top speed and every pattern change is smooth (worst step %u)", worst);
    fader(0, 0); frames(20, 5); worst = max_step(400);
    CHECK(worst <= 32, "slowest speed smooth too (worst step %u)", worst);
    btn(KK_BTN_GLOBE, 1); frames(700, 5); btn(KK_BTN_GLOBE, 0); frames(300, 5);
    CHECK(!kk.show_latched && !kk.anim_active, "unlatched");
    /* Ripples ease in too. */
    show_on(); show_off(); show_on(); show_off(); show_on(); show_off(); frames(600, 5);
    CHECK(kk.ripple, "ripples on");
    key(12, 127); worst = max_step(60); key(12, 0); w = max_step(300); if (w > worst) worst = w;   /* a key: the pads only show the ring */
    CHECK(worst <= 40, "a ripple rises and fades without a hard jump (worst step %u)", worst);
    show_on(); show_off(); show_on(); show_off(); show_on(); show_off(); frames(600, 5);
}

/* 0.98: battery gauge along a Li-ion curve (Elite readings). */
int kk_battery_percent(unsigned adc);
void test_battery_curve(void)
{
    CHECK(kk_battery_percent(664) == 100 && kk_battery_percent(640) == 100, "off the charger: 100%%");
    int a = kk_battery_percent(635); CHECK(a >= 90 && a < 100, "a minute later: 9x%% (%d)", a);
    int b = kk_battery_percent(552); CHECK(b >= 5 && b <= 12, "after hours of play: nearly empty (%d)", b);
    CHECK(kk_battery_percent(527) == 0 && kk_battery_percent(500) == 0, "empty");
    CHECK(kk_battery_percent(812) == 101 && kk_battery_percent(0) == -1, "USB power / no reading");
    int last = -1, ok = 1; for (unsigned v = 527; v <= 660; ++v) { int p = kk_battery_percent(v); if (p < last) ok = 0; last = p; }
    CHECK(ok, "never goes down as the voltage rises");
}


/* 0.99: Pad Scale works on its own: a key shows the keyboard and its name,
 * a pad brings the pad grid back. */
void test_pad_scale_keys(void)
{
    boot(); settle();
    tap(KK_BTN_SCALE); settle();
    CHECK(kk.mode == KK_MODE_PAD_SCALE && !kk.key_shown, "Pad Scale opens on the pads");
    pad(3, 100); frames(20, 20); pad(3, 0);
    key(16, 100); frames(40, 20);
    CHECK(kk.key_shown, "a key: the screen shows the keys");
    shot("pad_scale_key");
    key(16, 0); frames(10, 20);
    pad(5, 100); frames(20, 20); pad(5, 0);
    CHECK(!kk.key_shown, "a pad: back to the pad grid");
    shot("pad_scale_pad");
    tap(KK_BTN_SCALE);
}

/* 1.01: Key Scale and Pad Scale together; keys light the scale pads. */
void test_key_and_pad_scale(void)
{
    boot(); settle();
    tap(KK_BTN_SEQ); CHECK(kk.mode == KK_MODE_KEY_SCALE, "SEQ: Key Scale");
    tap(KK_BTN_SCALE);
    CHECK(kk.mode == KK_MODE_PAD_SCALE && kk.keys_scale && kk_keys_in_scale(), "SCALE with Key Scale on: both on");
    CHECK(kk_led_button(KK_BTN_SEQ) == 2 && kk_led_button(KK_BTN_SCALE) == 2, "both LEDs lit");
    frames(20, 20); shot("pad_key_scale");
    /* keys play the scale: white keys = scale degrees, black keys silent */
    CHECK(kk.scale.scale == 0 && kk.scale.root == 0, "C major");
    unsigned ch; host_clear(); key(13, 100); key(13, 0);
    CHECK(out_count(USB, 0x90, -1) == 0, "black key silent in Key Scale");
    /* a key in another octave lights one pad, the same name */
    kk.octave = -2; key(14, 100);
    int n = (int)last_out(USB, 0x90, 0) & 127;
    unsigned lit = 0, which = 99;
    for (unsigned p = 0; p < KK_PADS; ++p) if (kk_pad_lit(p)) { ++lit; which = p; }
    CHECK(lit == 1 && kk_pad_note(which, &ch) % 12 == n % 12, "key %d lights one pad of that name (%u lit)", n, lit);
    CHECK(!kk.key_shown, "the screen keeps the pad grid");
    key(14, 0);
    lit = 0; for (unsigned p = 0; p < KK_PADS; ++p) lit += kk_pad_lit(p);
    CHECK(lit == 0, "released: dark");
    kk.octave = 0; key(12, 100);       /* in the pads' octave: its own pad */
    n = (int)last_out(USB, 0x90, 0) & 127; lit = 0; which = 99;
    for (unsigned p = 0; p < KK_PADS; ++p) if (kk_pad_lit(p)) { ++lit; which = p; }
    CHECK(lit == 1 && kk_pad_note(which, &ch) == n, "same octave: exactly its pad");
    key(12, 0);
    /* SEQ in Pad Scale: keys back to normal, pads stay */
    tap(KK_BTN_SEQ); CHECK(kk.mode == KK_MODE_PAD_SCALE && !kk.keys_scale, "SEQ: keys normal, pads stay");
    tap(KK_BTN_SEQ); CHECK(kk.keys_scale, "SEQ again: keys in scale");
    tap(KK_BTN_SCALE); CHECK(kk.mode == KK_MODE_KEY_SCALE && !kk.keys_scale, "SCALE: pads off, Key Scale stays");
    tap(KK_BTN_SEQ); CHECK(kk.mode == KK_MODE_NORMAL, "SEQ: normal");
    tap(KK_BTN_SCALE); tap(KK_BTN_FX); CHECK(kk.mode == KK_MODE_DRUM && !kk.keys_scale, "another mode clears it");
    tap(KK_BTN_FX);
}

/* 1.02 CHORD KEYS: every key plays the chosen chord; voicings; saved. */
void test_chord_keys(void)
{
    boot(); settle();
    uint8_t n[64]; unsigned c;
    tap(KK_BTN_PATCH);
    smk37_control_event(SMK37_ENCODER, 6, 1); frames(3, 2);
    CHECK(kk.chord_keys == 1, "PATCH E7: CHORD KEYS m9");
    frames(20, 20); shot("patch_chord_keys");
    tap(KK_BTN_PATCH);
    kk.octave = 0;
    host_clear(); key(21, 100);                                  /* A */
    notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[0] == 69 && n[1] == 72 && n[2] == 76 && n[3] == 79 && n[4] == 83, "A -> Am9: A C E G B (%u notes)", c);
    frames(20, 20); shot("chord_keys_am9");
    key(21, 0);
    for (unsigned k = 0; k < 128; ++k) CHECK(!kk_midi_count(0, k), "released %u", k);
    /* the same shape on every key */
    host_clear(); key(14, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[0] == 62 && n[1] == 65 && n[4] == 76, "D -> Dm9 (same shape)"); key(14, 0);
    /* chord pads still win */
    host_clear(); pad(1, 100); key(21, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 3, "a chord pad overrides CHORD KEYS (%u)", c); key(21, 0); pad(1, 0);
    /* SCALE 9TH in C major: A -> Am9, C -> Cmaj9, G -> G9 */
    kk.chord_keys = 14;
    host_clear(); key(19, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[0] == 67 && n[1] == 71 && n[2] == 74 && n[3] == 77 && n[4] == 81, "SCALE 9TH: G -> G9 (G B D F A)"); key(19, 0);
    host_clear(); key(12, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[3] == 71 && n[4] == 74, "SCALE 9TH: C -> Cmaj9"); key(12, 0);
    /* voicings */
    kk.chord_keys = 1; kk.chord_voicing = 1;
    host_clear(); key(21, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[0] == 57 && n[1] == 72, "SPREAD: root an octave down"); key(21, 0);
    kk.chord_voicing = 2;
    host_clear(); key(21, 100); notes_on(USB, ON1, n, &c);
    CHECK(c == 5 && n[0] == 67 && n[1] == 69 && n[4] == 83, "DROP 2: G (2nd from top) dropped below the root"); key(21, 0);
    { uint8_t saved[16]; kamotkeys_settings_read(saved); kk.chord_keys = 0; kk.chord_voicing = 0;
      CHECK(kamotkeys_settings_apply(saved) == 0 && kk.chord_keys == 1 && kk.chord_voicing == 2, "saved"); }
    kk.chord_keys = 0; kk.chord_voicing = 0; kk.octave = -2;
}

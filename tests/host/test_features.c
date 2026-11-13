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
    host_clear(); key(0, 100); key(2, 100); key(2, 0); key(0, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_DRUM) == 1 && out_count(USB, 0x9D, KK_STEP_PAGE_PREV) == 1 && out_count(USB, 0x8D, KK_STEP_DRUM) == 1,
          "C = DRUM (hold), D = page prev");
    host_clear(); key(24, 100); key(24, 0); key(1, 100); key(1, 0);
    CHECK(out_count(USB, 0x9D, KK_STEP_VEL_1) == 1 && out_count(USB, 0x90, 49) == 1, "C4 = velocity 1, C# still plays");
    host_clear(); host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBD, KK_STEP_CODE_CC + 0, KK_STEPC_NOTE_6); host_in(0xBD, KK_STEP_CODE_CC + 1, KK_STEPC_EMPTY);
    host_in(0xBD, KK_STEP_CODE_CC + 2, KK_STEPC_PLAY_NOTE); host_in(0xBD, KK_STEP_CODE_CC + 15, KK_STEPC_OUTSIDE);
    host_in(0xBD, KK_STEP_PAGE_CC, 1); host_in(0xBD, KK_STEP_PAGES_CC, 4);
    CHECK(host_local_n == 0, "step feedback never reaches the synth");
    settle();
    CHECK(((host_rgb[0] >> 16) & 255) > ((host_rgb[0] >> 8) & 255) * 3 / 2 && ((host_rgb[0] >> 16) & 255) >= KK_RGB_PEAK * 9 / 10, "note = bright amber (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] & 255) > ((host_rgb[1] >> 16) & 255) && (host_rgb[1] & 255) < KK_RGB_PEAK / 3, "empty = dim blue (%06x)", host_rgb[1]);
    CHECK(((host_rgb[2] >> 8) & 255) >= KK_RGB_PEAK * 9 / 10, "playhead on a note = green (%06x)", host_rgb[2]);
    CHECK(host_rgb[15] && host_rgb[15] < host_rgb[1] + 0x100000, "outside the clip = very dim red (%06x)", host_rgb[15]);
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

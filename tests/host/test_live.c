/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: 0.31+: Ableton feedback, Clip Mode, host notes, light show timing. */
#include "helpers.h"

void test_031(void)
{
    host_ms = 1000; groove_midi_init(); groove_screen_init(); groove_midi_poll();
    CHECK(kk.octave == -2 && kk.pad_octave == 0 && kk_active_octave() == -2, "power-on: keys -2, pads 0");
    fader(0, 0); fader(1, 0); fader(2, 0); fader(3, 127); frames(800, 2); host_clear();
    key(12, 100); CHECK(last_out(USB, ON1, 0) == (36 | 100 << 8), "second C at octave -2 = C2 (36)"); key(12, 0);
    tap(KK_BTN_FX); CHECK(kk_active_octave() == 0, "Drum Mode shows the pad octave");
    host_clear(); pad(1, 100); pad(1, 0); CHECK(out_count(USB, ON10, 36) == 1, "drum pad 1 still 36 at power-on");
    tap(KK_BTN_OCT_UP); CHECK(kk.pad_octave == 1 && kk.octave == -2, "OCT in Drum Mode moves the pads only");
    tap(KK_BTN_OCT_DOWN); tap(KK_BTN_FX);
    kk_set_octave(0);

    /* Chord LOCK (pad 13). */
    host_clear(); pad(2, 100); pad(9, 100); pad(13, 100); pad(13, 0); pad(2, 0); pad(9, 0);
    CHECK(kk.chord_lock && kk.chord_mods == (KK_MOD_MIN | KK_MOD_7), "LOCK keeps MIN+7 after release (%x)", kk.chord_mods);
    key(12, 100); uint8_t notes[64]; unsigned n; notes_on(USB, ON1, notes, &n);
    CHECK(n == 4 && notes[0] == 60 && notes[1] == 63 && notes[2] == 67 && notes[3] == 70, "locked Cm7 from one key");
    key(12, 0);
    pad(9, 100); pad(9, 0); CHECK(kk.chord_mods == KK_MOD_MIN, "locked: tapping the active extension clears it");
    pad(10, 100); pad(10, 0); CHECK(kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "and a new one turns on");
    pad(1, 100); pad(1, 0); CHECK(kk.chord_mods == (KK_MOD_MAJ | KK_MOD_MAJ7), "locked: MAJ replaces MIN (%x)", kk.chord_mods);
    pad(4, 100); pad(4, 0); CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_MAJ7), "SUS4 replaces MAJ");
    pad(11, 100); pad(11, 0); CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_6), "6 replaces MAJ7");
    pad(9, 100); pad(12, 100); pad(12, 0); pad(9, 0);
    CHECK(kk.chord_mods == (KK_MOD_SUS4 | KK_MOD_7 | KK_MOD_9), "7 and 9 pressed together combine (%x)", kk.chord_mods);
    pad(2, 100); pad(2, 0); pad(10, 100); pad(10, 0);
    CHECK(kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "back to MIN + MAJ7");
    /* 0.59: the lock carries into Key Scale and back; other modes drop it. */
    tap(KK_BTN_SEQ);
    CHECK(kk.mode == KK_MODE_KEY_SCALE && kk.chord_lock && kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "LOCK survives Key Scale (%x)", kk.chord_mods);
    host_clear(); key(12, 100); notes_on(USB, ON1, notes, &n); key(12, 0);
    CHECK(n >= 3, "Key Scale + locked chord plays a chord (%u notes)", n);
    tap(KK_BTN_SEQ);
    CHECK(kk.mode == KK_MODE_NORMAL && kk.chord_lock && kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "and back to Normal");
    /* 0.62: the lock survives every mode, and keys keep playing the chord. */
    tap(KK_BTN_FX); host_clear(); key(12, 100); notes_on(USB, ON1, notes, &n); key(12, 0);
    CHECK(kk.mode == KK_MODE_DRUM && kk.chord_lock && n >= 3, "LOCK kept in Drum Mode, keys play the chord (%u)", n);
    tap(KK_BTN_REC); CHECK(kk.mode == KK_MODE_STEP && kk.chord_lock && !kk_pad_lit(12), "kept in Step Mode, LOCK pad not lit there");
    tap(KK_BTN_REC); tap(KK_BTN_CHORD); tap(KK_BTN_CHORD);
    CHECK(kk.mode == KK_MODE_NORMAL && kk.chord_lock && kk.chord_mods == (KK_MOD_MIN | KK_MOD_MAJ7), "back in Normal with the same lock");
    settle(); uint32_t locked = host_rgb[12];
    CHECK((locked & 0xff) > KK_RGB_PEAK * 3 / 4, "LOCK pad bright (%06x)", locked);
    pad(13, 100); pad(13, 0); CHECK(!kk.chord_lock && !kk.chord_mods, "LOCK off releases the latch");
    settle(); CHECK((host_rgb[12] & 0xff) * 2 < (locked & 0xff), "LOCK pad idle is clearly dimmer (%06x)", host_rgb[12]);
    /* 0.42 palette: MAJ warm amber, MIN blue, DIM purple, SUS4 teal. */
    CHECK((host_rgb[0] >> 16) > (host_rgb[0] & 255) * 4, "MAJ idle warm (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] & 255) > ((host_rgb[1] >> 16) & 255) * 3, "MIN idle blue (%06x)", host_rgb[1]);
    CHECK((host_rgb[2] & 255) > ((host_rgb[2] >> 8) & 255) * 3 && (host_rgb[2] >> 16) > ((host_rgb[2] >> 8) & 255) * 2, "DIM idle purple (%06x)", host_rgb[2]);
    CHECK(((host_rgb[3] >> 8) & 255) > (host_rgb[3] >> 16) * 4, "SUS4 idle teal (%06x)", host_rgb[3]);

    /* 0.34: playing keys and chords in Normal Mode changes no pad colour at
     * all, so KamoteKeys sends the pads no frame on a key press. */
    settle(); host_rgb_changes = 0;
    for (unsigned k = 0; k < 37; k += 3) { key(k, 100); frames(5, 3); key(k, 0); frames(5, 3); }
    pad(9, 100); frames(3, 3); unsigned after_pad = host_rgb_changes;
    for (unsigned k = 0; k < 37; k += 5) { key(k, 90); frames(5, 3); key(k, 0); frames(5, 3); }
    pad(9, 0); frames(3, 3);
    CHECK(after_pad == 1 && host_rgb_changes == 2, "key presses change no pad colour (%u, %u)", after_pad, host_rgb_changes);

    /* Spare pads are not lit by chords from the keys (the 0.30 'random lights'). */
    pad(1, 100); key(31, 100); settle();   /* G5 major: G B D = 79 83 86 -> 86 is spare pad 7's note */
    CHECK(kk_midi_count(0, 86) == 1 && !kk_pad_lit(6), "spare pad 7 stays idle while a chord sounds 86");
    key(31, 0); pad(1, 0);

    /* Clip Mode. */
    host_clear(); tap(KK_BTN_CHORD);
    CHECK(kk.mode == KK_MODE_CLIP && kk_led_button(KK_BTN_CHORD) == 2, "CHORD -> Clip Mode");
    CHECK(out_count(USB, 0xBF, KK_CLIP_SYNC_CC) == 1, "entering Clip Mode asks Live for the state");
    /* 0.65: a plain press selects (note 84+) until F# toggles pads to play (36+). */
    host_clear(); pad(3, 110);
    CHECK(out_count(USB, 0x9F, 84 + 2) == 1 && host_local_n == 0, "pad 3 -> select note 86 ch16 (default), never the synth");
    pad(3, 0); CHECK(out_count(USB, 0x8F, 84 + 2) == 1, "release -> note off");
    host_clear(); key(6, 100); key(6, 0); pad(3, 110); pad(3, 0);
    CHECK(kk.clip_play && host_out_n == 2 && out_count(USB, 0x9F, 38) == 1, "F# toggles pads to play (silent key): pad 3 -> launch note 38");
    host_clear(); tap(KK_BTN_ARP); pad(3, 100); frames(300, 2); pad(3, 0); tap(KK_BTN_ARP);
    CHECK(out_count(USB, 0x9F, 38) == 1, "ARP does not touch clip launches");
    {
        uint8_t d[16]; kamotkeys_settings_read(d); kk.clip_play = 0;
        CHECK(kamotkeys_settings_apply(d) == 0 && kk.clip_play == 1, "select / play is saved");
    }
    host_in(0xBF, KK_CLIP_HELLO_CC, 1);
    host_in(0xBF, KK_CLIP_STATE_CC + 0, KK_CLIP_PLAYING); host_in(0xBF, KK_CLIP_STATE_CC + 1, KK_CLIP_RECORDING);
    host_in(0xBF, KK_CLIP_STATE_CC + 2, KK_CLIP_STOPPED); host_in(0xBF, KK_CLIP_STATE_CC + 3, KK_CLIP_QUEUED);
    CHECK(kk_midi_host_protocol() == 1 && kk_midi_clip_state(1) == KK_CLIP_RECORDING, "feedback recorded");
    CHECK(host_local_n == 0, "channel 16 feedback never reaches the synth");
    settle();
    CHECK(((host_rgb[0] >> 8) & 255) >= KK_RGB_PEAK * 9 / 10 && (host_rgb[0] >> 16) < 4, "playing = bright green (%06x)", host_rgb[0]);
    CHECK((host_rgb[1] >> 16) >= KK_RGB_PEAK * 9 / 10 && (host_rgb[1] & 0xffff) == 0, "recording = bright red (%06x)", host_rgb[1]);
    CHECK((host_rgb[2] & 255) >= KK_RGB_PEAK / 4 && (host_rgb[2] & 255) <= KK_RGB_PEAK / 2 && (host_rgb[2] >> 16) <= 8, "stopped = dim blue (%06x)", host_rgb[2]);
    CHECK(((host_rgb[4] >> 16) & 255) <= 8, "empty/unknown = very dim (%06x)", host_rgb[4]);
    unsigned a = 0, b = 255;
    for (unsigned i = 0; i < 20; ++i) { frames(10, 5); unsigned r = (host_rgb[3] >> 16) & 255; if (r > a) a = r; if (r < b) b = r; }
    CHECK(a >= KK_RGB_PEAK * 9 / 10 && b <= KK_RGB_PEAK / 4, "queued blinks amber (%u..%u)", b, a);
    frames(40, 5); shot("16_clip_mode");
    host_ready_ports = 0; frames(5, 2);
    CHECK(kk_midi_host_protocol() == 0 && kk_midi_clip_state(0) == KK_CLIP_UNKNOWN, "disconnect forgets stale clip state");
    host_clear(); host_ready_ports = 4; frames(30, 5);
    CHECK(out_count(BLE, 0xBF, KK_CLIP_SYNC_CC) == 1, "reconnect (BLE) re-requests the state");
    host_ready_ports = 6; frames(5, 2);
    /* 0.65 keys: C# COPY / D# PASTE = ch 16 notes 100 / 101 (press + release);
     * A# held = DELETE (note 68+); G# silent; top F# / G# / A# = scene launch / up / down. */
    host_clear(); key(1, 100); key(1, 0); key(3, 100); key(3, 0);
    CHECK(out_count(USB, 0x9F, KK_CLIP_COPY_NOTE) == 1 && out_count(USB, 0x8F, KK_CLIP_COPY_NOTE) == 1 &&
          out_count(USB, 0x9F, KK_CLIP_PASTE_NOTE) == 1 && host_local_n == 0, "C# / D# -> COPY / PASTE notes");
    host_clear(); key(10, 100); pad(10, 100); pad(10, 0);
    CHECK(kk.clip_action == 2 && out_count(USB, 0x9F, 68 + 9) == 1 && out_count(USB, 0x8F, 68 + 9) == 1, "A# + pad 10 -> delete note 77");
    frames(20, 5); shot("17_clip_delete");
    key(10, 0); CHECK(kk.clip_action == 0, "key released: back to the plain press");
    host_clear(); key(8, 100); key(8, 0);
    CHECK(out_count(USB, 0x9F, KK_CLIP_ARRANGE_NOTE) == 1 && out_count(USB, 0x8F, KK_CLIP_ARRANGE_NOTE) == 1, "0.66: G# = Scene Arrange (note 102)");
    host_clear(); key(2, 100); key(2, 0); key(4, 100); key(4, 0);
    CHECK(out_count(USB, CC15, KK_CC_TRACKS_LEFT) == 2 && out_count(USB, CC15, KK_CC_TRACKS_RIGHT) == 2 && out_count(USB, 0x90, -1) == 0,
          "D / E: track view left / right (CC 31 / 32), no notes");
    host_in(0xBF, KK_HOST_TRACKS_CC, 3); host_in(0xBF, KK_HOST_ARRANGE_CC, 1); host_in(0xBF, KK_CLIP_STATE_CC + 0, KK_CLIP_ARR_ON);
    host_in(0xBF, KK_CLIP_STATE_CC + 1, KK_CLIP_ARR_OFF); settle();
    CHECK(kk_midi_host_tracks() == 3 && kk_midi_arrange() && ((host_rgb[0] >> 8) & 255) > ((host_rgb[1] >> 8) & 255) * 3,
          "Scene Arrange: track on bright green, off dim (%06x / %06x)", host_rgb[0], host_rgb[1]);
    frames(40, 5); shot("17c_scene_arrange");
    host_in(0xBF, KK_HOST_ARRANGE_CC, 0); host_in(0xBF, KK_HOST_TRACKS_CC, 0);
    host_clear(); key(34, 100); key(34, 0); key(32, 100); key(32, 0); key(30, 100); key(30, 0);
    CHECK(out_count(USB, CC15, KK_CC_SCENE_DOWN) == 2 && out_count(USB, CC15, KK_CC_SCENE_UP) == 2 &&
          out_count(USB, CC15, KK_CC_SCENE_LAUNCH) == 2 && out_count(USB, 0x90, -1) == 0, "top A# / G# / F# -> scene down / up / launch, no notes");
    host_in(0xBF, KK_COPY_KIND_CC, KK_COPY_CLIP | 64); host_in(0xBF, KK_COPY_A_CC, 3); host_in(0xBF, KK_COPY_B_CC, 2);
    { unsigned ca, cb; CHECK(kk_midi_copy(&ca, &cb) == (KK_COPY_CLIP | 64) && ca == 3 && cb == 2, "clipboard from the script"); }
    frames(40, 5); shot("17b_clip_paste");
    host_in(0xBF, KK_COPY_KIND_CC, 0);
    key(6, 100); key(6, 0);                                     /* back to select */
    /* 0.57: Live's highlighted slot shows violet (not amber/yellow). */
    host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_in(0xBF, KK_CLIP_STATE_CC + 5, KK_CLIP_STOPPED); host_in(0xBF, KK_HOST_HIGHLIGHT_CC, 5);
    settle();
    { unsigned r = (host_rgb[5] >> 16) & 255, g = (host_rgb[5] >> 8) & 255, b = host_rgb[5] & 255;
      CHECK(kk_midi_host_highlight() == 5 && b > r && r > g * 2 && b >= KK_RGB_PEAK / 3, "highlighted pad violet (%06x)", host_rgb[5]); }
    host_in(0xBF, KK_HOST_HIGHLIGHT_CC, 127); settle();
    CHECK(kk_midi_host_highlight() < 0 && (host_rgb[5] >> 16) < 8, "no highlight: stopped = dim blue again (%06x)", host_rgb[5]);
    host_in(0xBF, KK_CLIP_HELLO_CC, 1); host_in(0xBF, KK_HOST_SCENE_CC, 4);
    CHECK(kk_midi_host_scene() == 4, "grid follows the script's scene offset");
    frames(20, 5); shot("18_clip_scene5");
    host_in(0xBF, KK_HOST_SCENE_CC, 0);
    host_clear(); pad(3, 100); pad(3, 0); CHECK(out_count(USB, 0x9F, 84 + 2) == 1, "plain press after release: select");
    host_clear(); key(1, 100); tap(KK_BTN_CHORD); key(1, 0);
    CHECK(host_out_n == 0 || out_count(USB, 0x90, -1) == 0, "OVERDUB key held across leaving Clip Mode stays silent");
    key(1, 100); key(1, 0); CHECK(out_count(USB, 0x90, 49) == 1, "outside Clip Mode C# plays its note");
    tap(KK_BTN_CHORD);
    tap(KK_BTN_CHORD); CHECK(kk.mode == KK_MODE_NORMAL, "CHORD again -> Normal");

    /* Move-style: host notes light Pad Scale / Drum pads. */
    tap(KK_BTN_FX); settle(); uint32_t idle = host_rgb[0];
    host_in(0x99, 36, 100); frames(3, 3);
    CHECK(kk_pad_lit(0) && host_rgb[0] != idle && (host_rgb[0] >> 16) == KK_RGB_PEAK, "Ableton kick lights pad 1");
    host_in(0x89, 36, 0); frames(10, 3);
    CHECK(!kk_pad_lit(0) && host_rgb[0] == idle, "then returns to idle");
    tap(KK_BTN_FX);
    kk_midi_set_local(0); host_clear(); host_in(0x90, 60, 100);
    CHECK(host_local_n == 0, "internal synth off: host notes stay silent"); host_in(0x80, 60, 0); kk_midi_set_local(1);

    /* 0.42: the light show crossfades in (no one-frame jump = no flash). */
    {
        uint32_t before = host_rgb[0]; unsigned steps = 0, prev = before;
        show_on();
        for (unsigned i = 0; i < 10; ++i) { frames(1, 25); if (host_rgb[0] != prev) ++steps; prev = host_rgb[0]; }
        CHECK(steps >= 4, "light show fades in over several frames (%u)", steps);
        show_off(); frames(20, 25);
    }
    /* The light show keeps refreshing while the LCD is redrawing. */
    show_on(); fader(0, 127);
    uint32_t seen[8]; unsigned changes = 0;
    for (unsigned i = 0; i < 8; ++i) { frames(10, 4); seen[i] = host_rgb[5]; if (i && seen[i] != seen[i - 1]) ++changes; }
    CHECK(changes >= 6, "pads animate at ~30 fps (%u/7 samples changed)", changes);
    uint32_t before = kk.anim_phase; frames(100, 5);  /* 500 ms at 2 Hz = one full cycle */
    uint32_t moved = kk.anim_phase - before;
    CHECK(moved > 0xe0000000U || moved < 0x20000000U, "2 Hz show completes a cycle in 500 ms (%08x)", moved);
    show_off();
}

/* 0.54: ARP follows Ableton's tempo (Remote Script CC 102/103 on ch 16). */
/* 0.59: holding KNOB BANK + FADER BANK for 3 s toggles ripple lights;
 * pads keep working. */

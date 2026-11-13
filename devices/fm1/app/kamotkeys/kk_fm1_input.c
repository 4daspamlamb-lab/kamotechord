/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys FM-1 input adapter: turns the FM-1 panel state (27 keys F..G,
 * 14 buttons, 7 encoders; fm1_controls_read) into KamoteKeys events, and
 * decides the on/off key and button LEDs.
 *
 * Keys (see docs/CONTROLS_FM1.md):
 *  - Normal and Key Scale Mode, split keyboard: the lowest nine white keys
 *    are the chord pads MAJ MIN DIM SUS4 | 7 MAJ7 6 9 | LOCK, the six black
 *    keys between them are the spare CC pads (CC 102-107), and keys 15..26
 *    (G#..G) play notes.
 *  - Pad Scale, Drum and Clip Mode: white keys 01..16 are pads 1..16 and the
 *    black keys are silent.
 * A key's release always goes to whatever its press went to, even if the
 * mode changed while it was held. */
#include "kk_perform.h"
#include "kk_midi.h"
#include "kk_led.h"
#include "kk_theory.h"
#include "fm1.h"
#include <string.h>

/* White keys of the FM-1 keyboard (key indexes), labelled 01..16. */
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
/* Split zone: white keys 01..09 -> chord pads, black keys -> spare pads. */
static const uint8_t chord_pad_of_white[9] = {0, 1, 2, 3, 8, 9, 10, 11, 12};
static const uint8_t zone_black_keys[6] = {1, 3, 5, 8, 10, 13};
static const uint8_t spare_pad_of_black[6] = {4, 5, 6, 7, 13, 14};

/* FM-1 buttons: OCT-, OCT+, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP,
 * SEQ, PLAY, REC -> KamoteKeys functions. LFO is special (tap = settings,
 * hold = Bluetooth menu, longer = diagnostics). */
enum { FM1_LFO = 5, FM1_BUTTONS = 14, LFO_HOLD_MS = 600 };
static const uint8_t button_map[FM1_BUTTONS] = {
    KK_BTN_OCT_DOWN, KK_BTN_OCT_UP, KK_BTN_FX, KK_BTN_SCALE, KK_BTN_NOTE_REPEAT, KK_BTN_PATCH,
    KK_BTN_CHORD, KK_BTN_GLOBE, KK_BTN_LEFT, KK_BTN_RIGHT, KK_BTN_ARP, KK_BTN_SEQ,
    KK_BTN_PLAY, KK_BTN_KNOB_BANK};
/* fm1_controls encoders: SELECT, ALGORITHM, K1..K4, PRESETS -> E5, E6,
 * E1..E4, E7 (index into kk.encoder). */
static const uint8_t encoder_map[7] = {4, 5, 0, 1, 2, 3, 6};

enum { AS_NONE, AS_KEY, AS_PAD };
static uint8_t key_as[KK_KEYS], key_id[KK_KEYS];
static uint32_t keys_seen, buttons_seen, lfo_at;
static uint8_t lfo_bt;

static int white_of(unsigned key)
{
    for (unsigned i = 0; i < 16; ++i) if (white_keys[i] == key) return (int)i;
    return -1;
}
static unsigned pad_mode(void)
{
    return kk.mode == KK_MODE_PAD_SCALE || kk.mode == KK_MODE_DRUM || kk.mode == KK_MODE_CLIP;
}
/* What a key is right now: AS_KEY with key id, AS_PAD with pad index, or none. */
static unsigned classify(unsigned key, unsigned *id)
{
    int white = white_of(key);
    if (pad_mode()) {
        if (white < 0) return AS_NONE;
        *id = (unsigned)white; return AS_PAD;
    }
    if (key >= KK_SPLIT_KEY) { *id = key; return AS_KEY; }
    if (white >= 0) { *id = chord_pad_of_white[white]; return AS_PAD; }
    for (unsigned i = 0; i < 6; ++i) if (zone_black_keys[i] == key) { *id = spare_pad_of_black[i]; return AS_PAD; }
    return AS_NONE;
}

void kk_fm1_input_reset(void)
{
    memset(key_as, 0, sizeof key_as); memset(key_id, 0, sizeof key_id);
    keys_seen = buttons_seen = 0; lfo_at = 0; lfo_bt = 0;
}

static void key_change(unsigned key, unsigned down)
{
    if (down) {
        unsigned id = 0, as = classify(key, &id);
        key_as[key] = (uint8_t)as; key_id[key] = (uint8_t)id;
        if (as == AS_KEY) kk_control_event(KK_EV_KEY, id, (int)kk.velocity);
        else if (as == AS_PAD) kk_control_event(KK_EV_PAD, id, (int)kk.velocity);
    } else {
        if (key_as[key] == AS_KEY) kk_control_event(KK_EV_KEY, key_id[key], 0);
        else if (key_as[key] == AS_PAD) kk_control_event(KK_EV_PAD, key_id[key], 0);
        key_as[key] = AS_NONE;
    }
}

static void button_change(unsigned b, unsigned down, uint32_t now)
{
    if (b == FM1_LFO) {
        if (down) { lfo_at = now ? now : 1; lfo_bt = 0; return; }
        if (lfo_bt) kk_control_event(KK_EV_BUTTON, KK_BTN_BT, 0);
        else {
            /* A tap closes the Bluetooth menu / diagnostics, else toggles settings. */
            unsigned id = kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG ? KK_BTN_BT : KK_BTN_PATCH;
            kk_control_event(KK_EV_BUTTON, id, 1); kk_control_event(KK_EV_BUTTON, id, 0);
        }
        lfo_at = 0; lfo_bt = 0;
        return;
    }
    kk_control_event(KK_EV_BUTTON, button_map[b], (int)down);
}

void kk_fm1_input(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    uint32_t keys = c->keys & ((1U << KK_KEYS) - 1U), changed = keys ^ keys_seen;
    /* Releases first, so a key moving between roles never overlaps. */
    for (unsigned k = 0; k < KK_KEYS; ++k)
        if ((changed >> k) & 1U && !((keys >> k) & 1U)) key_change(k, 0);
    for (unsigned k = 0; k < KK_KEYS; ++k)
        if ((changed >> k) & 1U && ((keys >> k) & 1U)) key_change(k, 1);
    keys_seen = keys;
    uint32_t buttons = c->buttons & ((1U << FM1_BUTTONS) - 1U), bchanged = buttons ^ buttons_seen;
    for (unsigned b = 0; b < FM1_BUTTONS; ++b)
        if ((bchanged >> b) & 1U) button_change(b, (buttons >> b) & 1U, now);
    buttons_seen = buttons;
    /* LFO held: open the Bluetooth menu (as a held BT button, so holding on
     * reaches diagnostics like a long BT press on the SMK-37). */
    if (lfo_at && !lfo_bt && now - lfo_at >= LFO_HOLD_MS) { lfo_bt = 1; kk_control_event(KK_EV_BUTTON, KK_BTN_BT, 1); }
    for (unsigned e = 0; e < 7; ++e)
        if (c->encoders[e]) kk_control_event(KK_EV_ENCODER, encoder_map[e], c->encoders[e]);
}

/* ------------------------------------------------------------------ LEDs */
static unsigned blink(unsigned period_ms) { return (fm1_millis() / period_ms) & 1U; }
static unsigned pad_led(unsigned pad)
{
    if (kk.mode == KK_MODE_CLIP) {
        if (kk_pad_lit(pad)) return 1;
        switch (kk_midi_clip_state(pad)) {
        case KK_CLIP_PLAYING: return 1;
        case KK_CLIP_RECORDING: return blink(100);
        case KK_CLIP_QUEUED: case KK_CLIP_QUEUED_RECORD: return blink(250);
        default: return 0;
        }
    }
    if (kk_pad_lit(pad)) return 1;
    if (kk.mode == KK_MODE_PAD_SCALE) {          /* roots stay lit as a guide */
        unsigned channel;
        int note = kk_pad_note(pad, &channel);
        return note >= 0 && (unsigned)(note % 12) == kk.scale.root % 12;
    }
    return 0;
}
unsigned kk_fm1_key_led(unsigned key, uint32_t boot_ms)
{
    if (key >= KK_KEYS) return 0;
    if (boot_ms < KK_BOOT_MS) {                  /* boot: a light sweeps up and back */
        unsigned span = KK_BOOT_MS / 2, t = boot_ms < span ? boot_ms : KK_BOOT_MS - boot_ms;
        unsigned head = t * (KK_KEYS + 2) / span;
        return key + 2 >= head && key <= head;
    }
    unsigned id = 0, as = classify(key, &id);
    if (as == AS_PAD) return pad_led(id);
    if (as == AS_KEY) return (unsigned)((kk.keys >> key) & 1U);
    return 0;
}
unsigned kk_fm1_button_led(unsigned b)
{
    if (b >= FM1_BUTTONS) return 0;
    if (b == FM1_LFO)
        return kk.page == KK_PAGE_PATCH || kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG || ((buttons_seen >> b) & 1U);
    return kk_led_button(button_map[b]) == 2;
}

/* FM-1 LED output: buttons 0..13, keys 14..40; written only when they change. */
void kk_led_service(uint32_t boot_ms)
{
    static uint8_t shown[41];
    static uint8_t started;
    if (!started) { for (unsigned i = 0; i < 41; ++i) shown[i] = 2; started = 1; }
    for (unsigned i = 0; i < 41; ++i) {
        unsigned on = i < 14 ? (boot_ms < KK_BOOT_MS ? 0 : kk_fm1_button_led(i)) : kk_fm1_key_led(i - 14, boot_ms);
        on = !!on;
        if (on != shown[i]) { shown[i] = (uint8_t)on; fm1_led_set(i, (int)on); }
    }
}

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys FM-1 input adapter: turns the FM-1 panel state (27 keys F..G,
 * 14 buttons, 7 encoders; fm1_controls_read) into KamoteKeys events, and
 * decides the on/off key and button LEDs. See docs/FM1.md.
 *
 * Keys:
 *  - Normal and Key Scale Mode: the lowest nine keys, white and black
 *    (F..C#), are the chord keys MAJ MIN DIM SUS4 7 MAJ7 6 9 LOCK; keys
 *    9..26 (D..G) play.
 *  - Pad Scale Mode (0.52): the chord keys stay; keys 9..26, white and
 *    black, play consecutive scale notes (with chords). Root keys stay lit.
 *  - Drum Mode: white keys 01..16 are pads 1..16, black keys are silent.
 *  - Step Mode (hold LFO): white keys 01..16 are steps, black keys are the
 *    sequencer functions (kk_device.h).
 *  - Clip Mode: white keys 01..16 are clips; black keys held: F# = OVERDUB,
 *    G# = DELETE, A# = SELECT (+ a clip); C# (4th black key) = launch the
 *    selected scene; top D# / F# = scene up / down (handled by the core).
 * A key's release always goes to whatever its press went to, even if the
 * mode changed while it was held.
 *
 * Buttons sit where their SMK-37 counterparts sit (right block, 2 x 6):
 *   FX=ARP  SEL=SUSTAIN  ENV=PAD SCALE  LFO=CLIP  EDIT=STOP  GLO=BT menu
 *   HOME    SAVE         ARP=DRUM       SEQ=KEY SCALE  PLAY=START  REC=RECORD
 * HOME tap = settings page (or back to the main screen); HOME + OCT- / OCT+
 * = Undo / Redo; HOME + REC = Capture MIDI; HOME + SAVE = metronome. SAVE
 * (where the SMK-37 has PARA) tapped = Play/Pause; held: K1-K3 set
 * velocity, scale and root (the SMK-37 faders). In the Bluetooth menu HOME and SAVE
 * are its back / confirm keys. */
#include "kk_perform.h"
#include "kk_midi.h"
#include "kk_led.h"
#include "kk_theory.h"
#include "fm1.h"
#include <string.h>

/* White keys of the FM-1 keyboard (key indexes), labelled 01..16. */
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
/* Chord keys 0..8 (F F# G G# A A# B C C#) -> chord pads. */
static const uint8_t chord_pad_of_key[KK_SPLIT_KEY] = {0, 1, 2, 3, 8, 9, 10, 11, 12};

enum { FM1_OCT_DOWN = 0, FM1_OCT_UP = 1, FM1_HOME = 8, FM1_SAVE = 9, FM1_REC = 13, FM1_BUTTONS = 14, NONE = 255 };
static const uint8_t button_map[FM1_BUTTONS] = {
    KK_BTN_OCT_DOWN, KK_BTN_OCT_UP,
    KK_BTN_ARP, KK_BTN_NOTE_REPEAT, KK_BTN_SCALE, KK_BTN_CHORD, KK_BTN_GLOBE, KK_BTN_BT,   /* FX SEL ENV LFO EDIT GLO */
    NONE, NONE, KK_BTN_FX, KK_BTN_SEQ, KK_BTN_PLAY, KK_BTN_KNOB_BANK};                    /* HOME SAVE ARP SEQ PLAY REC */
/* fm1_controls encoders: SELECT, ALGORITHM, K1..K4, PRESETS -> E5, E6,
 * E1..E4, E7 (index into kk.encoder). */
static const uint8_t encoder_map[7] = {4, 5, 0, 1, 2, 3, 6};

enum { AS_NONE, AS_KEY, AS_PAD };
static uint8_t key_as[KK_KEYS], key_id[KK_KEYS], sent[FM1_BUTTONS];
static uint32_t keys_seen, buttons_seen;
static uint8_t home_down, home_used;

static int white_of(unsigned key)
{
    for (unsigned i = 0; i < 16; ++i) if (white_keys[i] == key) return (int)i;
    return -1;
}
/* What a key is right now: AS_KEY with key id, AS_PAD with pad index, or none. */
static unsigned classify(unsigned key, unsigned *id)
{
    int white = white_of(key);
    if (kk.mode == KK_MODE_STEP) {          /* white keys = steps, black keys = functions */
        if (white >= 0) { *id = (unsigned)white; return AS_PAD; }
        *id = key; return kk_step_key_function(key) >= 0 ? AS_KEY : AS_NONE;
    }
    if (kk.mode == KK_MODE_DRUM || kk.mode == KK_MODE_CLIP) {
        if (white >= 0) { *id = (unsigned)white; return AS_PAD; }
        if (kk.mode == KK_MODE_CLIP && (key == KK_CLIP_OVERDUB_KEY || key == KK_CLIP_DELETE_KEY || key == KK_CLIP_SELECT_KEY ||
                                        key == KK_SCENE_LAUNCH_KEY || key == KK_SCENE_UP_KEY || key == KK_SCENE_DOWN_KEY)) {
            *id = key; return AS_KEY;                    /* Clip Mode action / scene keys */
        }
        return AS_NONE;
    }
    if (key < KK_SPLIT_KEY) { *id = chord_pad_of_key[key]; return AS_PAD; }
    *id = key; return AS_KEY;
}

void kk_fm1_input_reset(void)
{
    memset(key_as, 0, sizeof key_as); memset(key_id, 0, sizeof key_id); memset(sent, NONE, sizeof sent);
    keys_seen = buttons_seen = 0; home_down = home_used = 0;
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

static void tap(unsigned id) { kk_control_event(KK_EV_BUTTON, id, 1); kk_control_event(KK_EV_BUTTON, id, 0); }
static unsigned menu_page(void) { return kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG; }
static void button_change(unsigned b, unsigned down)
{
    if (!down) {                         /* a release goes where its press went */
        if (b == FM1_HOME && sent[b] == NONE) {
            if (home_down && !home_used) {
                if (kk.page == KK_PAGE_MAIN) tap(KK_BTN_PATCH);          /* settings */
                else { kk.page = KK_PAGE_MAIN; ++kk.edit_revision; }    /* back */
            }
            home_down = 0;
        }
        if (b == FM1_SAVE && sent[b] == NONE) {
            /* SAVE tapped without turning a knob = PARA: Play/Pause. */
            if (kk.knob_shift == 1) tap(KK_BTN_PARA);
            kk.knob_shift = 0; ++kk.edit_revision;
        }
        if (sent[b] != NONE && sent[b] != 254) kk_control_event(KK_EV_BUTTON, sent[b], 0);
        sent[b] = NONE;
        return;
    }
    unsigned id = button_map[b];
    if (b == FM1_HOME) {
        if (menu_page()) id = KK_BTN_LEFT;                               /* menu back key */
        else { home_down = 1; home_used = 0; id = NONE; }
    } else if (b == FM1_SAVE) {
        if (kk.page == KK_PAGE_BT) id = KK_BTN_RIGHT;                    /* menu confirm key */
        else if (home_down) {                                            /* HOME + SAVE = metronome */
            kk_control_event(KK_EV_BUTTON, KK_BTN_LEFT, 1); tap(KK_BTN_PARA); kk_control_event(KK_EV_BUTTON, KK_BTN_LEFT, 0);
            home_used = 1; id = NONE; sent[b] = 254;                     /* swallow the release */
            return;
        }
        else { kk.knob_shift = 1; ++kk.edit_revision; id = NONE; }
    } else if (home_down && (b == FM1_OCT_DOWN || b == FM1_OCT_UP || b == FM1_REC)) {
        id = b == FM1_OCT_DOWN ? KK_BTN_LEFT : b == FM1_OCT_UP ? KK_BTN_RIGHT : KK_BTN_FADER_BANK;   /* Undo / Redo / Capture */
        home_used = 1;
    }
    sent[b] = (uint8_t)id;
    if (id != NONE) kk_control_event(KK_EV_BUTTON, id, 1);
}

void kk_fm1_input(const struct fm1_controls *c)
{
    uint32_t keys = c->keys & ((1U << KK_KEYS) - 1U), changed = keys ^ keys_seen;
    /* Releases first, so a key moving between roles never overlaps. */
    for (unsigned k = 0; k < KK_KEYS; ++k)
        if ((changed >> k) & 1U && !((keys >> k) & 1U)) key_change(k, 0);
    for (unsigned k = 0; k < KK_KEYS; ++k)
        if ((changed >> k) & 1U && ((keys >> k) & 1U)) key_change(k, 1);
    keys_seen = keys;
    uint32_t buttons = c->buttons & ((1U << FM1_BUTTONS) - 1U), bchanged = buttons ^ buttons_seen;
    for (unsigned b = 0; b < FM1_BUTTONS; ++b)          /* releases first here too */
        if ((bchanged >> b) & 1U && !((buttons >> b) & 1U)) button_change(b, 0);
    for (unsigned b = 0; b < FM1_BUTTONS; ++b)
        if ((bchanged >> b) & 1U && ((buttons >> b) & 1U)) button_change(b, 1);
    buttons_seen = buttons;
    for (unsigned e = 0; e < 7; ++e)
        if (c->encoders[e]) kk_control_event(KK_EV_ENCODER, encoder_map[e], c->encoders[e]);
}

/* ------------------------------------------------------------------ LEDs */
static unsigned blink(unsigned period_ms) { return (fm1_millis() / period_ms) & 1U; }
static unsigned pad_led(unsigned pad)
{
    if (kk.mode == KK_MODE_STEP) {          /* on/off rendering of the step colour codes */
        unsigned code = kk_midi_step_code(pad);
        if (kk_pad_lit(pad)) return 1;
        if (code == KK_STEPC_PLAY_EMPTY || code == KK_STEPC_PLAY_NOTE) return blink(90);      /* playhead */
        if ((code >= KK_STEPC_NOTE_1 && code <= KK_STEPC_TIE) || code == KK_STEPC_DRUM_ACTIVE) return 1;
        if (code == KK_STEPC_DRUM_LOADED) return blink(400);
        return 0;
    }
    if (kk.mode == KK_MODE_CLIP) {
        if (kk_pad_lit(pad)) return 1;
        switch (kk_midi_clip_state(pad)) {
        case KK_CLIP_PLAYING: return 1;
        case KK_CLIP_RECORDING: return blink(100);
        case KK_CLIP_QUEUED: case KK_CLIP_QUEUED_RECORD: return blink(250);
        default: return (int)pad == kk_midi_host_highlight() ? blink(600) : 0;   /* highlighted: slow blink */
        }
    }
    return kk_pad_lit(pad);   /* chord keys: held / latched; drum pads: sounding */
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
    if (as == AS_KEY && kk.mode == KK_MODE_PAD_SCALE) {   /* held or sounding; roots lit as a guide */
        int note = kk_pad_scale_note(key - KK_SPLIT_KEY, kk.pad_octave, &kk.scale);
        if ((kk.keys >> key) & 1U) return 1;
        return note >= 0 && ((unsigned)(note % 12) == kk.scale.root % 12 || kk_midi_count(KK_CH_MELODIC, (unsigned)note));
    }
    if (as == AS_KEY) return (unsigned)((kk.keys >> key) & 1U);
    return 0;
}
unsigned kk_fm1_button_led(unsigned b)
{
    if (b >= FM1_BUTTONS) return 0;
    unsigned pressed = (buttons_seen >> b) & 1U;
    if (b == FM1_HOME) return pressed || kk.page == KK_PAGE_PATCH;
    if (b == FM1_SAVE) return pressed;
    if (sent[b] != NONE && sent[b] != button_map[b]) return 1;   /* HOME combo held */
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

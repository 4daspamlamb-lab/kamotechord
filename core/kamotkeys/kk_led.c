/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys LED logic shared by every device: each pad's logical colour and
 * level per mode (also drawn on the LCD pad map) and the button LED levels.
 * Pad indexes are logical pad numbers minus one (top row 1-8 left to right,
 * bottom row 9-16). Devices send the result to their own LEDs: SMK-37 RGB
 * pads (app/kamotkeys/kk_led_rgb.c), FM-1 on/off key LEDs (kk_fm1_input.c). */
#include "kk_led_util.h"
#include "kk_perform.h"
#include "kk_midi.h"

/* ---------------------------------------------------------------- maths */
/* sin8: phase 0..255 -> 0..255 (128 = zero crossing). Quarter table. */
static const uint8_t quarter[65] = {
    128,131,134,137,140,143,146,149,152,156,159,162,165,168,171,174,
    176,179,182,185,188,191,193,196,199,201,204,206,209,211,213,216,
    218,220,222,224,226,228,230,232,234,236,237,239,240,242,243,245,
    246,247,248,249,250,251,252,252,253,254,254,255,255,255,255,255,255};
unsigned kk_sin8(unsigned phase)
{
    phase &= 255;
    if (phase < 64) return quarter[phase];
    if (phase < 128) return quarter[128 - phase];
    if (phase < 192) return 256 - quarter[phase - 128] > 255 ? 255 : 256 - quarter[phase - 128];
    return 256 - quarter[256 - phase] > 255 ? 255 : 256 - quarter[256 - phase];
}
/* -------------------------------------------------------- mode palettes */
/* Chord pads (0.42): top row = chord type, coloured by mood (major warm
 * amber, minor blue, diminished purple, sus teal); bottom row = extensions,
 * one magenta-to-pink family that gets lighter as the extension rises
 * (7, maj7, 6, 9); LOCK is neutral white. */
static const uint32_t chord_colours[KK_PADS] = {
    0xffa010, 0x1c50ff, 0x8a1cff, 0x00d0a0, 0, 0, 0, 0,     /* MAJ MIN DIM SUS4 */
    0xff0060, 0xff2ca0, 0xff60c8, 0xffa0e8, 0xffffff, 0, 0, 0};/* 7 MAJ7 6 9 LOCK */
static const uint32_t spare_colours[KK_PADS] = {
    0, 0, 0, 0, 0xff0000, 0xff6000, 0xffd000, 0x00ff30,     /* pads 5-8   */
    0, 0, 0, 0, 0, 0x00e0ff, 0x0030ff, 0xa000ff};           /* pads 14-16 */
static const uint32_t drum_colours[9] = {
    0xff0000, /* kick   red    */ 0xff6400, /* snare  orange */
    0xffdc00, /* hi-hat yellow */ 0x003cff, /* toms   blue   */
    0x9600ff, /* rim    purple */ 0xff3ca0, /* clap   pink   */
    0x00c8aa, /* cymbal teal   */ 0x3cff3c, /* perc   green  */
    0x505050};
enum { ROOT_CYAN = 0x60f0ff, GREEN = 0x00ff3c };

/* Levels in permille of programmed brightness (0.43: resting pads visible
 * but calm; pressed/sounding pads about 4x brighter). Pad Scale roots rest
 * more than twice as bright as the other scale notes. */
enum { IDLE_CHORD = 250, IDLE_SPARE = 125,   /* spare = half of chord (0.48) */ IDLE_SCALE = 220, IDLE_ROOT = 500, IDLE_DRUM = 220, LIT = 1000 };

/* Clip Mode: the colour follows Ableton's real slot state. */
static uint32_t clip_colour(unsigned pad, unsigned *level)
{
    unsigned state = kk_midi_clip_state(pad), held = kk_pad_lit(pad);
    unsigned blink = (fm1_millis() / 250U) & 1U;   /* 2 Hz */
    uint32_t c;
    switch (state) {
    case KK_CLIP_STOPPED: c = 0x1050ff; *level = 400; break;
    case KK_CLIP_PLAYING: c = 0x00ff40; *level = LIT; break;
    case KK_CLIP_QUEUED: c = 0xffa000; *level = blink ? LIT : 250; break;
    case KK_CLIP_RECORDING: c = 0xff0000; *level = LIT; break;
    case KK_CLIP_QUEUED_RECORD: c = 0xff0000; *level = blink ? LIT : 250; break;
    case KK_CLIP_EMPTY_ARMED: c = 0xff2000; *level = 120; break;
    case KK_CLIP_EMPTY: c = 0xb0b0b0; *level = 70; break;
    default: c = 0x8090a0; *level = 40; break;                 /* no Remote Script yet */
    }
    if (held && *level < 600) { *level = 600; if (state == KK_CLIP_EMPTY || state >= KK_CLIP_UNKNOWN) c = 0xffffff; }
    return c;
}

uint32_t kk_led_logical(unsigned pad, unsigned *level)
{
    unsigned lit = kk_pad_lit(pad);
    if (kk.mode == KK_MODE_CLIP) return clip_colour(pad, level);
    if (kk.mode == KK_MODE_PAD_SCALE) {
        unsigned channel;
        int note = kk_pad_note(pad, &channel);
        unsigned root = note >= 0 && (unsigned)(note % 12) == kk.scale.root % 12;
        *level = note < 0 ? 0 : lit ? LIT : root ? IDLE_ROOT : IDLE_SCALE;
        return root ? ROOT_CYAN : GREEN;
    }
    if (kk.mode == KK_MODE_DRUM) {
        unsigned channel;
        int note = kk_pad_note(pad, &channel);
        *level = note < 0 ? 0 : lit ? LIT : IDLE_DRUM;
        return drum_colours[note < 0 ? 8 : kk_drum_family(note)];
    }
    if (spare_colours[pad]) { *level = lit ? LIT : IDLE_SPARE; return spare_colours[pad]; }
    *level = lit ? LIT : IDLE_CHORD;
    return chord_colours[pad];
}
uint32_t kk_led_pad_base(unsigned pad)
{
    unsigned level;
    return kk_led_logical(pad, &level);
}

unsigned kk_led_button(unsigned id)
{
    unsigned pressed = (kk.buttons >> id) & 1U;
    switch (id) {
    case KK_BTN_CHORD: return kk.mode == KK_MODE_CLIP ? 2 : 1;        /* Clip Mode */
    case KK_BTN_ARP: return kk.arp_on || pressed ? 2 : 1;
    case KK_BTN_NOTE_REPEAT: return kk.sustain_on || pressed ? 2 : 1;  /* SUSTAIN */
    case KK_BTN_SCALE: return kk.mode == KK_MODE_PAD_SCALE ? 2 : 1;   /* Pad Scale only */
    case KK_BTN_SEQ: return kk.mode == KK_MODE_KEY_SCALE ? 2 : 1;     /* Key Scale only */
    case KK_BTN_FX: return kk.mode == KK_MODE_DRUM ? 2 : 1;
    case KK_BTN_REC: return kk.anim_active ? 2 : 1;
    case KK_BTN_BT: return kk.page == KK_PAGE_BT || kk.page == KK_PAGE_DIAG || pressed ? 2 : 1;
    case KK_BTN_PATCH: return kk.page == KK_PAGE_PATCH || pressed ? 2 : 1;
    default: return pressed ? 2 : 1;   /* momentary, incl. KNOB BANK (state not shown) */
    }
}

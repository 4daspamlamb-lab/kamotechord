/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamotKeys centralized pad/button LED renderer and procedural light shows.
 * Pad indexes are logical pad numbers minus one (top row 1-8 left to right,
 * bottom row 9-16); app/board/panel.c maps them onto the LED chain. */
#include "kk_led.h"
#include "kk_perform.h"
#include "kk_midi.h"

void smk37_rgb_set(unsigned id, unsigned rgb);
void smk37_rgb_service(void);
void smk37_rgb_touch(void);
uint8_t kk_led_test;
const char *kk_led_test_name(unsigned test)
{
    static const char *const names[KK_LED_TESTS] = {"OFF", "STATIC BARS (NO REFRESH)", "BARS RESENT", "ALL OFF", "BIT TEST"};
    return names[test % KK_LED_TESTS];
}

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
static uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }
static uint32_t scale(uint32_t c, unsigned level /* 0..255 */)
{
    return rgb(((c >> 16) & 255) * level / 255, ((c >> 8) & 255) * level / 255, (c & 255) * level / 255);
}
static uint32_t mix(uint32_t a, uint32_t b, unsigned t /* 0..255 toward b */)
{
    unsigned r = (((a >> 16) & 255) * (255 - t) + ((b >> 16) & 255) * t) / 255;
    unsigned g = (((a >> 8) & 255) * (255 - t) + ((b >> 8) & 255) * t) / 255;
    unsigned l = ((a & 255) * (255 - t) + (b & 255) * t) / 255;
    return rgb(r, g, l);
}
/* Interpolated sine on a 16-bit phase: smooth even at very slow speeds. */
static unsigned sin16(unsigned phase)
{
    unsigned i = (phase >> 8) & 255, f = phase & 255;
    int a = (int)kk_sin8(i), b = (int)kk_sin8(i + 1);
    return (unsigned)(a + (b - a) * (int)f / 256);
}
/* Fully saturated hue, 0..65535 around the wheel, interpolated. */
static uint32_t hue16(unsigned h)
{
    h &= 0xffff;
    unsigned sector = h / 10923U, f = (h - sector * 10923U) * 255U / 10923U;
    switch (sector) {
    case 0: return rgb(255, f, 0);
    case 1: return rgb(255 - f, 255, 0);
    case 2: return rgb(0, 255, f);
    case 3: return rgb(0, 255 - f, 255);
    case 4: return rgb(f, 0, 255);
    default: return rgb(255, 0, 255 - f);
    }
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
enum { IDLE_CHORD = 250, IDLE_SPARE = 300, IDLE_SCALE = 220, IDLE_ROOT = 500, IDLE_DRUM = 220, LIT = 1000 };

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

static uint32_t logical(unsigned pad, unsigned *level)
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
    return logical(pad, &level);
}

/* ------------------------------------------------------------ animation */
/* Light-show pattern colour at full scale. t = 0..65535 per cycle. All
 * positions are fixed-point, so motion glides between pads instead of
 * stepping; the output filter adds the fading trails. */
static uint32_t show(unsigned pattern, unsigned pad, unsigned t)
{
    unsigned row = pad / 8, col = pad % 8;
    /* Perimeter position: top row left->right, bottom row right->left. */
    unsigned ring = row ? 15 - col : col;
    switch (pattern) {
    case 0: /* rainbow sweep */
        return hue16(col * 3072U + row * 1536U - t);
    case 1: { /* vertical pulse: rows breathe in counter-phase, hue drifts */
        unsigned b = sin16(t + row * 32768U);
        return scale(hue16(t / 2 + col * 768U), b * b / 255);
    }
    case 2: { /* spiral/perimeter comet with a smooth tail (1/256 pad units) */
        unsigned head = (t * 16U) >> 8, dist = (head - ring * 256U + 4096U) % 4096U;
        unsigned b = dist < 1536U ? 255U - dist * 255U / 1536U : 0;
        return scale(hue16(t + ring * 2048U), b * b / 255);
    }
    case 3: { /* checkerboard: hold, then a quick smooth cross */
        int a = ((int)sin16(t) - 128) * 3 + 128;
        unsigned k = a < 0 ? 0 : a > 255 ? 255 : (unsigned)a, parity = (row + col) & 1;
        return mix(0xff0060, 0x00b4ff, parity ? k : 255 - k);
    }
    case 4: { /* diagonal wave */
        unsigned b = sin16(t * 2U - (col + row * 2U) * 5120U);
        return scale(hue16(43690U + (col + row) * 2560U + t / 4), b);
    }
    case 5: { /* breathing colours */
        unsigned b = sin16(t * 3U + 49152U);
        return scale(hue16(t + pad * 512U), 30 + b * 225 / 255);
    }
    case 6: { /* opposing comets: top row -> right, bottom row -> left */
        unsigned head = (t * 8U) >> 8, pos = row ? 7 - col : col, dist = (head - pos * 256U + 2048U) % 2048U;
        unsigned b = dist < 896U ? 255U - dist * 255U / 896U : 0;
        return scale(row ? 0x00ffb0 : 0xff2080, b * b / 255);
    }
    default: { /* colour wave */
        unsigned b = 40 + sin16(t * 2U + col * 8192U) * 215 / 255;
        return scale(hue16(t + col * 4096U + row * 16384U), b);
    }
    }
}
/* Aurora Flow: deep blue, teal, cyan, purple and a hint of pink. */
static uint32_t aurora(unsigned pad, unsigned ms)
{
    static const uint32_t palette[6] = {0x0a1450, 0x00a08c, 0x00dcff, 0x6a28d8, 0xd04aa0, 0x0a1450};
    unsigned row = pad / 8, col = pad % 8;
    unsigned position = (col * 72 + row * 110 + ms * 255 / 700 + kk_sin8(ms / 5 + col * 20) / 4) % 1280;
    unsigned index = position / 256, t = position % 256;
    uint32_t c = mix(palette[index], palette[index + 1], t);
    unsigned b = 110 + kk_sin8(ms / 3 + row * 90 + col * 25) * 145 / 255;
    return scale(c, b);
}

/* --------------------------------------------------------------- output */
/* Proven on hardware: pads were calm only when every channel value was a
 * multiple of 4 (0.38; test bars), and flashed on key presses otherwise
 * (0.37, 0.39). Values with bit 0 or 1 set are misread by this unit's pad
 * LEDs. So round every channel to a multiple of 4; a lit channel is at
 * least 4. With the 160 peak the colours keep their hue. */
static unsigned led_level(unsigned v)
{
    if (!v) return 0;
    v = (v + 2U) & ~3U;
    return v < 4U ? 4U : v > 252U ? 252U : v;
}
static uint32_t output(uint32_t c, unsigned permille)
{
    if (!(kk.fader_known & 8)) return 0;     /* never flash before Fader 4 is read */
    /* programmed peak x level x Fader 4 */
    uint32_t f = (uint32_t)KK_RGB_PEAK * permille / 1000U * kk.brightness;   /* 0..peak*256 */
    unsigned r = (((c >> 16) & 255) * f + 32767) / (255U * 256U);
    unsigned g = (((c >> 8) & 255) * f + 32767) / (255U * 256U);
    unsigned b = ((c & 255) * f + 32767) / (255U * 256U);
    return rgb(led_level(r), led_level(g), led_level(b));
}
/* Light-show crossfade (0.42): 0 = normal colours, 255 = animation. Rises
 * over ~200 ms when SEQ REC is pressed and falls over ~150 ms on release,
 * so starting a show is no longer a hard one-frame jump (seen as a flash). */
static unsigned anim_mix;
static uint32_t round_rgb(uint32_t c)
{
    return rgb(led_level((c >> 16) & 255), led_level((c >> 8) & 255), led_level(c & 255));
}
uint32_t kk_led_pad(unsigned pad, uint32_t boot_ms)
{
    if (pad >= KK_PADS || kk_led_test == 3) return 0;
    if (boot_ms < KK_BOOT_MS) return output(aurora(pad, boot_ms), 1000);
    unsigned level;
    uint32_t c = logical(pad, &level);
    if (anim_mix) {
        uint32_t show_rgb = output(show(kk.anim_pattern, pad, kk.anim_phase >> 16), 1000);
        return anim_mix >= 255 ? show_rgb : round_rgb(mix(output(c, level), show_rgb, anim_mix));
    }
    if (boot_ms < KK_BOOT_MS + KK_BOOT_FADE_MS) {
        /* Blend the last aurora frame into the normal pad colours. */
        unsigned t = (boot_ms - KK_BOOT_MS) * 255U / KK_BOOT_FADE_MS;
        return output(mix(aurora(pad, boot_ms), scale(c, level * 255U / 1000U), t), 1000);
    }
    return output(c, level);
}
unsigned kk_led_button(unsigned id)
{
    unsigned pressed = (kk.buttons >> id) & 1U;
    switch (id) {
    case KK_BTN_PARA: return 0;                                       /* disabled */
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
/* Pad output. 0.36: the whole pad frame is resent continuously, ~40 times a
 * second, whether or not a colour changed. On hardware, frames sent only on
 * change stayed garbled after LCD traffic, while the same frame resent
 * continuously (Diagnostics "BARS RESENT") was steady with every timing:
 * any disturbance is now overwritten within ~25 ms. */
void kk_led_service(uint32_t boot_ms)
{
    static uint8_t levels[KK_BUTTONS + 1] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                             255, 255, 255, 255, 255, 255, 255, 255, 255};
    static uint32_t frame_ms;
    for (unsigned id = 0; id <= KK_BUTTONS; ++id) {
        unsigned level = id < KK_BUTTONS ? kk_led_button(id) : 0; /* 18: PLAY's second LED, unused */
        if (boot_ms < KK_BOOT_MS && id < KK_BUTTONS) level = level ? 1 : 0;
        if (level != levels[id]) { levels[id] = (uint8_t)level; smk37_led_level(id, level); }
    }
    uint32_t now = fm1_millis();
    static uint32_t mix_ms;
    {
        uint32_t dt = now - mix_ms > 100U ? 100U : now - mix_ms;
        mix_ms = now;
        if (kk.anim_active) anim_mix = anim_mix + dt * 255U / 200U >= 255U ? 255U : anim_mix + dt * 255U / 200U;
        else anim_mix = anim_mix > dt * 255U / 150U ? anim_mix - dt * 255U / 150U : 0;
    }
    {
        for (unsigned pad = 0; pad < KK_PADS; ++pad) {
            uint32_t colour = kk_led_test == 1 || kk_led_test == 2
                ? output(scale(hue16((pad % 8) * 8192U), pad < 8 ? 255 : 64), 1000)   /* fixed bars */
                : kk_led_test == 4
                ? (pad < 8 ? (1U << (pad % 8)) << 16 : (1U << (pad % 8)) << 8)       /* raw bit test (bypasses rounding) */
                : kk_led_pad(pad, boot_ms);
            smk37_rgb_set(pad, colour);    /* marks a pad dirty only if it changed */
        }
        /* Changed colours go out at once (smk37_rgb_set marks them dirty);
         * every 25 ms the whole frame is resent anyway, changed or not
         * (except STATIC BARS, which shows what an un-refreshed frame does). */
        if (kk_led_test != 1 && now - frame_ms >= 25U) { frame_ms = now; smk37_rgb_touch(); }
    }
    smk37_rgb_service();
}

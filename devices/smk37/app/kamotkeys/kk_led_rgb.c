/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys SMK-37 pad output: RGB pad colours after Fader 4, procedural
 * light shows, the Aurora boot, the diagnostics RGB tests and the frame
 * refresh. Colours come from the shared logic (core/kamotkeys/kk_led.c);
 * app/board/panel.c maps pads onto the LED chain. */
#include "kk_led_util.h"
#include "kk_boot.h"
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
/* Boot (0.57, understated): one dim teal-to-violet wave crosses the pads
 * from left to right, then the normal colours fade in. */
static uint32_t aurora(unsigned pad, unsigned ms)
{
    /* 0.67 Bubbles boot: each pad lights in its bubble's colour as it pops. */
    const struct kk_boot_bubble *b = &kk_boot_bubbles[pad];
    if (ms < b->t0 + 50U) return 0;
    unsigned k = ms - b->t0 - 50U; k = k > 200U ? 200U : k;
    return scale(kk_boot_palette[pad % 8], 90U + k * 110U / 200U);   /* quick pop to ~80 % */
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
/* ------------------------------------------------------------- ripples */
/* Ripple lights (0.57, toggled with three quick SEQ REC presses): each key
 * or pad press starts a ring that spreads across the 8 x 2 pads from where
 * it was pressed (keys map left to right onto the 8 columns) and fades out.
 * Drawn over the normal colours; the pads keep working. */
enum { RIPPLES = 8, RIPPLE_MS = 900, RIPPLE_SPEED = 11 /* pads per second */ };
static struct { uint32_t start; int16_t x, y; uint16_t hue; uint8_t strength; } ripples[RIPPLES];
static unsigned ripple_next;
void kk_led_ripple(unsigned kind, unsigned id, unsigned velocity)
{
    int x, y;
    if (kind) { x = (int)(id % 8) * 1000; y = (int)(id / 8) * 1000; }
    else { x = (int)(id * 7000U / (KK_KEYS - 1)); y = 500; }
    unsigned i = ripple_next++ % RIPPLES;
    ripples[i].start = fm1_millis(); ripples[i].x = (int16_t)x; ripples[i].y = (int16_t)y;
    ripples[i].hue = (uint16_t)(x * 65535 / 7000 + 20000);
    ripples[i].strength = (uint8_t)(110 + (velocity > 127 ? 127 : velocity) * 145 / 127);
}
static unsigned isqrt(unsigned v)
{
    unsigned r = 0, bit = 1U << 30;
    while (bit > v) bit >>= 2;
    while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r;
}
/* Strongest ripple colour on a pad now (0 = none), at full scale. */
static uint32_t ripple_colour(unsigned pad)
{
    uint32_t now = fm1_millis(), best = 0; unsigned best_b = 0;
    int px = (int)(pad % 8) * 1000, py = (int)(pad / 8) * 1000;
    for (unsigned i = 0; i < RIPPLES; ++i) {
        uint32_t age = now - ripples[i].start;
        if (!ripples[i].strength || age >= RIPPLE_MS) continue;
        int dx = px - ripples[i].x, dy = py - ripples[i].y;
        int d = (int)isqrt((unsigned)(dx * dx + dy * dy));          /* milli-pads */
        int radius = (int)(age * RIPPLE_SPEED), off = d - radius; if (off < 0) off = -off;
        if (off >= 900) continue;
        unsigned ring = (unsigned)(900 - off) * 255U / 900U;          /* ring profile */
        unsigned fade = (RIPPLE_MS - age) * 255U / RIPPLE_MS;
        unsigned b = ring * ring / 255U * fade / 255U * ripples[i].strength / 255U;
        if (b > best_b) { best_b = b; best = scale(hue16(ripples[i].hue + age * 20U), b); }
    }
    return best;
}
static uint32_t brighter(uint32_t a, uint32_t b)
{
    unsigned r = (a >> 16) > (b >> 16) ? (a >> 16) & 255 : (b >> 16) & 255;
    unsigned g = ((a >> 8) & 255) > ((b >> 8) & 255) ? (a >> 8) & 255 : (b >> 8) & 255;
    unsigned l = (a & 255) > (b & 255) ? a & 255 : b & 255;
    return rgb(r, g, l);
}
uint32_t kk_led_pad(unsigned pad, uint32_t boot_ms)
{
    if (pad >= KK_PADS || kk_led_test == 3) return 0;
    if (boot_ms < KK_BOOT_MS) return output(aurora(pad, boot_ms), 1000);
    unsigned level;
    uint32_t c = kk_led_logical(pad, &level);
    if (anim_mix) {
        uint32_t show_rgb = output(show(kk.anim_pattern, pad, kk.anim_phase >> 16), 1000);
        return anim_mix >= 255 ? show_rgb : round_rgb(mix(output(c, level), show_rgb, anim_mix));
    }
    if (boot_ms < KK_BOOT_MS + KK_BOOT_FADE_MS) {
        /* Blend the last aurora frame into the normal pad colours. */
        unsigned t = (boot_ms - KK_BOOT_MS) * 255U / KK_BOOT_FADE_MS;
        return output(mix(aurora(pad, boot_ms), scale(c, level * 255U / 1000U), t), 1000);
    }
    if (kk.ripple) {
        uint32_t r = ripple_colour(pad);
        if (r) return brighter(output(c, level), output(r, 1000));
    }
    return output(c, level);
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

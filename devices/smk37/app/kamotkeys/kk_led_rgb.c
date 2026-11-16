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
        unsigned b = dist < 1536U ? 255U - dist * 255U / 1536U
                   : dist > 4096U - 512U ? (dist - (4096U - 512U)) * 255U / 512U : 0;   /* 0.96: soft front */
        return scale(hue16(t + ring * 2048U), b * b / 255);
    }
    case 3: { /* checkerboard: lingers, then a smooth cross (0.96: no clipped edge) */
        unsigned v = sin16(t), k = v * v * (765U - 2U * v) / 65025U, parity = (row + col) & 1;
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
        unsigned b = dist < 896U ? 255U - dist * 255U / 896U
                   : dist > 2048U - 320U ? (dist - (2048U - 320U)) * 255U / 320U : 0;   /* 0.96: soft front */
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
    unsigned k = ms - b->t0 - 50U; k = k > 250U ? 250U : k;
    k = k * 255U / 250U; k = k * k * (765U - 2U * k) / 65025U;      /* 0.96: eased in from dark */
    return scale(kk_boot_palette[pad % 8], k * 200U / 255U);
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
static uint32_t brighter_rgb(uint32_t a, uint32_t b)
{
    unsigned r = (a >> 16) > (b >> 16) ? (a >> 16) & 255 : (b >> 16) & 255;
    unsigned g = ((a >> 8) & 255) > ((b >> 8) & 255) ? (a >> 8) & 255 : (b >> 8) & 255;
    unsigned l = (a & 255) > (b & 255) ? a & 255 : b & 255;
    return rgb(r, g, l);
}

/* Ripple lights (0.57, toggled with three quick SEQ REC presses): each key
 * or pad press starts a ring that spreads across the 8 x 2 pads from where
 * it was pressed (keys map left to right onto the 8 columns) and fades out.
 * Drawn over the normal colours; the pads keep working. */
/* 0.80: more dramatic: longer, wider and brighter rings, a white flash at the
 * press point and a second echo ring. kind 2 = the giant centre ripple shown
 * when ripples are switched on (a slow double rainbow ring from the middle). */
enum { RIPPLES = 12, RIPPLE_MS = 1300, RIPPLE_SPEED = 9 /* pads per second */, RIPPLE_WIDTH = 1300,
       GIANT_MS = 2400, GIANT_SPEED = 5, GIANT_WIDTH = 2200, FLASH_MS = 220, RIPPLE_ATTACK_MS = 60 };
static struct { uint32_t start; int16_t x, y; uint16_t hue; uint8_t strength, giant; } ripples[RIPPLES];
static unsigned ripple_next;
void kk_led_ripple(unsigned kind, unsigned id, unsigned velocity)
{
    int x, y;
    if (kind == 2 || kind == 3) { x = 3500; y = 500; }   /* 3 (0.94): giant ring closing in on the middle */
    else if (kind) { x = (int)(id % 8) * 1000; y = (int)(id / 8) * 1000; }
    else { x = (int)(id * 7000U / (KK_KEYS - 1)); y = 500; }
    unsigned i = ripple_next++ % RIPPLES;
    ripples[i].start = fm1_millis(); ripples[i].x = (int16_t)x; ripples[i].y = (int16_t)y;
    ripples[i].hue = (uint16_t)(x * 65535 / 7000 + 20000);
    ripples[i].strength = kind >= 2 ? 255 : (uint8_t)(170 + (velocity > 127 ? 127 : velocity) * 85 / 127);
    ripples[i].giant = kind == 2 ? 1 : kind == 3 ? 2 : 0;
}
static unsigned isqrt(unsigned v)
{
    unsigned r = 0, bit = 1U << 30;
    while (bit > v) bit >>= 2;
    while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r;
}
/* Brightness 0..255 of a ring of this width at distance off from its edge. */
static unsigned ring_level(int off, int width)
{
    if (off < 0) off = -off;
    if (off >= width) return 0;
    unsigned ring = (unsigned)(width - off) * 255U / (unsigned)width;
    return ring * ring / 255U;
}
/* Strongest ripple colour on a pad now (0 = none), at full scale. */
static uint32_t ripple_colour(unsigned pad)
{
    uint32_t now = fm1_millis(), best = 0; unsigned best_b = 0;
    int px = (int)(pad % 8) * 1000, py = (int)(pad / 8) * 1000;
    for (unsigned i = 0; i < RIPPLES; ++i) {
        uint32_t age = now - ripples[i].start, life = ripples[i].giant ? GIANT_MS : RIPPLE_MS;
        if (!ripples[i].strength || age >= life) continue;
        int dx = px - ripples[i].x, dy = py - ripples[i].y;
        int d = (int)isqrt((unsigned)(dx * dx + dy * dy));          /* milli-pads */
        int speed = ripples[i].giant ? GIANT_SPEED : RIPPLE_SPEED, width = ripples[i].giant ? GIANT_WIDTH : RIPPLE_WIDTH;
        int radius = (int)(age * (unsigned)speed);
        if (ripples[i].giant == 2) { radius = 4600 - radius; if (radius < 0) radius = 0; }   /* inward */
        unsigned ring = ring_level(d - radius, width);
        unsigned echo = ring_level(d - radius * 11 / 20, width * 2 / 3) / 2;   /* a second, inner ring */
        if (echo > ring) ring = echo;
        unsigned fade = (life - age) * 255U / life;
        fade = 255U - (255U - fade) * (255U - fade) / 255U;           /* stays bright longer */
        unsigned b = ring * fade / 255U * ripples[i].strength / 255U;
        if (age < RIPPLE_ATTACK_MS) b = b * (age + 8U) / (RIPPLE_ATTACK_MS + 8U);   /* 0.96: eased in, never a hard on */
        uint32_t colour;
        if (ripples[i].giant) colour = scale(hue16((uint16_t)(age * 30U + (unsigned)d * 9U)), b);   /* rainbow */
        else colour = scale(hue16((uint16_t)(ripples[i].hue + age * 20U)), b);
        if (!ripples[i].giant && age < FLASH_MS && d < 600) {         /* white flash where it started */
            unsigned f = (FLASH_MS - age) * 255U / FLASH_MS * ripples[i].strength / 255U;
            if (age < RIPPLE_ATTACK_MS) f = f * (age + 8U) / (RIPPLE_ATTACK_MS + 8U);
            colour = brighter_rgb(colour, rgb(f, f, f)); if (f > b) b = f;
        }
        if (b > best_b) { best_b = b; best = colour; }
    }
    return best;
}
/* 0.96: smooth at any speed. A new pattern crossfades from the old one
 * (both keep moving) instead of cutting, and each pad's light-show colour
 * follows its target through a short low-pass filter, so a fast pattern
 * can never switch a pad hard on or off from one frame to the next. */
enum { PATTERN_FADE_MS = 900, SHOW_FILTER_MS = 45 };
static uint8_t shown_pattern = 255, old_pattern;
static unsigned pattern_fade = 255, frame_dt;            /* 0..255 toward shown_pattern; ms since last frame */
static uint16_t show_state[KK_PADS][3];                  /* filtered colour, 8.8 fixed point */
static uint8_t show_state_ok;
static void show_frame(unsigned dt)
{
    frame_dt = dt;
    if (shown_pattern == 255 || !anim_mix) { shown_pattern = kk.anim_pattern; pattern_fade = 255; }
    if (kk.anim_pattern != shown_pattern) {
        old_pattern = pattern_fade >= 128 ? shown_pattern : old_pattern;   /* mid-fade: keep the stronger one */
        shown_pattern = kk.anim_pattern; pattern_fade = 0;
    } else if (pattern_fade < 255) {
        unsigned step = dt * 255U / PATTERN_FADE_MS; pattern_fade = pattern_fade + step >= 255U ? 255U : pattern_fade + (step ? step : 1U);
    }
    if (!anim_mix) show_state_ok = 0;
}
static uint32_t show_smooth(unsigned pad)
{
    unsigned t = kk.anim_phase >> 16;
    uint32_t target = show(shown_pattern, pad, t);
    if (pattern_fade < 255) {
        unsigned k = pattern_fade; k = k * k * (765U - 2U * k) / 65025U;   /* eased */
        target = mix(show(old_pattern, pad, t), target, k);
    }
    uint16_t *st = show_state[pad];
    unsigned c[3] = {(target >> 16) & 255U, (target >> 8) & 255U, target & 255U};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned want = c[i] << 8;
        if (!(show_state_ok & 1)) st[i] = (uint16_t)want;
        else st[i] = (uint16_t)((int)st[i] + ((int)want - (int)st[i]) * (int)frame_dt / (int)(SHOW_FILTER_MS + frame_dt));
    }
    if (pad == KK_PADS - 1) show_state_ok = 1;
    return rgb(st[0] >> 8, st[1] >> 8, st[2] >> 8);
}
uint32_t kk_led_pad(unsigned pad, uint32_t boot_ms)
{
    if (pad >= KK_PADS || kk_led_test == 3) return 0;
    if (boot_ms < KK_BOOT_MS) return output(aurora(pad, boot_ms), 1000);
    unsigned level;
    uint32_t c = kk_led_logical(pad, &level);
    if (anim_mix) {
        uint32_t show_rgb = output(show_smooth(pad), 1000);
        uint32_t r = ripple_colour(pad);                 /* 0.94: rings also over the light show */
        if (r) show_rgb = brighter_rgb(show_rgb, output(r, 1000));
        return anim_mix >= 255 ? show_rgb : round_rgb(mix(output(c, level), show_rgb, anim_mix));
    }
    if (boot_ms < KK_BOOT_MS + KK_BOOT_FADE_MS) {
        /* Blend the last aurora frame into the normal pad colours. */
        unsigned t = (boot_ms - KK_BOOT_MS) * 255U / KK_BOOT_FADE_MS;
        return output(mix(aurora(pad, boot_ms), scale(c, level * 255U / 1000U), t), 1000);
    }
    {   /* ripples: only exist with ripple lights on, or the light-show latch ring (0.94) */
        uint32_t r = ripple_colour(pad);
        if (r) return brighter_rgb(output(c, level), output(r, 1000));
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
    static uint32_t mix_ms; static unsigned mix_lin;
    {
        uint32_t dt = now - mix_ms > 100U ? 100U : now - mix_ms;
        mix_ms = now;
        /* 0.96: the light show fades in over 450 ms and out over 400 ms (eased below) */
        if (kk.anim_active) mix_lin = mix_lin + dt * 255U / 450U >= 255U ? 255U : mix_lin + dt * 255U / 450U;
        else mix_lin = mix_lin > dt * 255U / 400U ? mix_lin - dt * 255U / 400U : 0;
        anim_mix = mix_lin ? (mix_lin * mix_lin * (765U - 2U * mix_lin) / 65025U) | 1U : 0;
        show_frame(dt);
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

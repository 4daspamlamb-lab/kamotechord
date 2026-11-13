/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys SMK-37 pad output: RGB pad colours after Fader 4, procedural
 * light shows, the Aurora boot, the diagnostics RGB tests and the frame
 * refresh. Colours come from the shared logic (core/kamotkeys/kk_led.c);
 * app/board/panel.c maps pads onto the LED chain. */
#include "kk_led_util.h"
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

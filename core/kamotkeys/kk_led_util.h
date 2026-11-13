/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys colour helpers shared by the LED logic (core) and the device
 * LED outputs (SMK-37 RGB pads). */
#ifndef KK_LED_UTIL_H
#define KK_LED_UTIL_H
#include <stdint.h>
#include "kk_led.h"
static inline uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }
static inline uint32_t scale(uint32_t c, unsigned level /* 0..255 */)
{
    return rgb(((c >> 16) & 255) * level / 255, ((c >> 8) & 255) * level / 255, (c & 255) * level / 255);
}
static inline uint32_t mix(uint32_t a, uint32_t b, unsigned t /* 0..255 toward b */)
{
    unsigned r = (((a >> 16) & 255) * (255 - t) + ((b >> 16) & 255) * t) / 255;
    unsigned g = (((a >> 8) & 255) * (255 - t) + ((b >> 8) & 255) * t) / 255;
    unsigned l = ((a & 255) * (255 - t) + (b & 255) * t) / 255;
    return rgb(r, g, l);
}
/* Interpolated sine on a 16-bit phase: smooth even at very slow speeds. */
static inline unsigned sin16(unsigned phase)
{
    unsigned i = (phase >> 8) & 255, f = phase & 255;
    int a = (int)kk_sin8(i), b = (int)kk_sin8(i + 1);
    return (unsigned)(a + (b - a) * (int)f / 256);
}
/* Fully saturated hue, 0..65535 around the wheel, interpolated. */
static inline uint32_t hue16(unsigned h)
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


/* Logical colour of a pad at 100 % programmed brightness and its level in
 * permille (idle / pressed / clip state), before any device output. */
uint32_t kk_led_logical(unsigned pad, unsigned *level);
#endif

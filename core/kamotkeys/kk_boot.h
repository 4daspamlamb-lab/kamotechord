/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Bubbles boot (0.67), shared by the LCD (kk_scr_pages.inc) and the pads
 * (devices/smk37/app/kamotkeys/kk_led_rgb.c): white piano keys drop in and
 * bounce, flat bubbles in an earthy pastel palette pop up behind them, the
 * black keys pop, one colour sweep runs over the keys and the name pops in
 * letter by letter. Integer maths only. */
#ifndef KK_BOOT_H
#define KK_BOOT_H
#include <stdint.h>

/* coral apricot mustard lime mint teal sky violet */
static const uint32_t kk_boot_palette[8] = {
    0xd67060, 0xdc8c50, 0xccae40, 0x96ba50, 0x4eb07c, 0x40a0b4, 0x548cce, 0x806cc4};

/* Bubble i (also pad i): pop start in ms, position on the 240-wide screen
 * (y from the top of the boot band) and radius. */
struct kk_boot_bubble { uint16_t t0; uint8_t x, y, r; };
static const struct kk_boot_bubble kk_boot_bubbles[16] = {
    {0, 22, 12, 15}, {50, 47, 2, 13}, {99, 73, 24, 17}, {149, 104, 8, 11}, {198, 128, 18, 19}, {248, 162, 4, 12},
    {298, 188, 16, 16}, {347, 216, 6, 13}, {397, 18, 92, 17}, {446, 50, 104, 12}, {496, 78, 88, 15}, {545, 106, 108, 11},
    {595, 136, 96, 19}, {645, 160, 110, 13}, {694, 190, 90, 17}, {744, 220, 100, 14}};

/* Ease-outs in permille (in and out). */
static inline int kk_boot_bounce(int p)          /* drop with two bounces */
{
    if (p <= 0) return 0;
    if (p >= 1000) return 1000;
    if (p < 364) return 7562 * p / 1000 * p / 1000;
    if (p < 727) { p -= 545; return 7562 * p / 1000 * p / 1000 + 750; }
    if (p < 909) { p -= 818; return 7562 * p / 1000 * p / 1000 + 937; }
    p -= 955; return 7562 * p / 1000 * p / 1000 + 984;
}
static inline int kk_boot_pop(int p)             /* 0 -> ~1100 -> 1000 (overshoot) */
{
    if (p <= 0) return 0;
    if (p >= 1000) return 1000;
    int q = p - 1000;                            /* -1000..0 */
    return 1000 + 2900 * q / 1000 * q / 1000 * q / 1000 + 1900 * q / 1000 * q / 1000;
}
enum { KK_BOOT_KEY_STEP = 96, KK_BOOT_KEY_MS = 560, KK_BOOT_BLACK_AT = 800, KK_BOOT_SWEEP_AT = 960,
       KK_BOOT_NAME_AT = 1040, KK_BOOT_LETTER_STEP = 25, KK_BOOT_LETTER_MS = 125 };
#endif

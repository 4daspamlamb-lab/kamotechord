/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys centralized LED renderer. Each pad's logical colour and level is
 * decided once (core/kamotkeys/kk_led.c: mode colour, pressed/idle level),
 * then each device outputs it (SMK-37: Fader 4 multiplier and RGB pads,
 * kk_led_rgb.c; FM-1: on/off key LEDs, kk_fm1_input.c), so modes never fight
 * over LED values. */
#ifndef KK_LED_H
#define KK_LED_H
#include <stdint.h>

enum { KK_BOOT_MS = 1100, KK_BOOT_FADE_MS = 350 };

/* SMK-37 only: final 0xRRGGBB for pad 0..15 (after Fader 4). boot_ms = time since the
 * Aurora boot started, or >= KK_BOOT_MS + KK_BOOT_FADE_MS when finished. */
uint32_t kk_led_pad(unsigned pad, uint32_t boot_ms);
/* Logical (pre-Fader 4) colour at 100 % programmed brightness, for the LCD. */
uint32_t kk_led_pad_base(unsigned pad);
/* Button LED level: 0 off, 1 dim, 2 bright. */
unsigned kk_led_button(unsigned id);
/* Push pad/key and button LEDs to the device (control task; device code). */
void kk_led_service(uint32_t boot_ms);
/* SMK-37 diagnostics RGB test: 0 off, 1 static colour bars, 2 the same bars resent
 * ~30 times a second, 3 all pads off, 4 bit test (top row red 1,2,4..128,
 * bottom row green 1..128: should ramp evenly left to right). Not saved. */
enum { KK_LED_TESTS = 5 };
extern uint8_t kk_led_test;
const char *kk_led_test_name(unsigned test);
/* Sine 0..255 over phase 0..255 (128 = zero), shared with the LCD aurora. */
unsigned kk_sin8(unsigned phase);
#endif

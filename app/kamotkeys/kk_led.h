/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys LED logic (FM-1). Pad colours are used by the LCD pad map; the
 * FM-1 key and button LEDs are on/off. */
#ifndef KK_LED_H
#define KK_LED_H
#include <stdint.h>

enum { KK_BOOT_MS = 1100, KK_BOOT_FADE_MS = 350 };

/* Logical (pre-Fader 4) colour at 100 % programmed brightness, for the LCD. */
uint32_t kk_led_pad_base(unsigned pad);
/* Button LED level: 0 off, 1 dim, 2 bright. */
unsigned kk_led_button(unsigned id);
/* Push key and button LEDs to the FM-1 panel (control task). boot_ms = time
 * since the boot animation started. */
void kk_led_service(uint32_t boot_ms);
/* Sine 0..255 over phase 0..255 (128 = zero), shared with the LCD aurora. */
unsigned kk_sin8(unsigned phase);
#endif

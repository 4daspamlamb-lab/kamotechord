/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys physical event interface (FM-1). app/kamotkeys/kk_fm1_input.c
 * turns the FM-1 key/button/encoder state into these events. */
#ifndef KK_EVENTS_H
#define KK_EVENTS_H
#include "fm1.h"
enum { KK_EV_BUTTON, KK_EV_KEY, KK_EV_PRESSURE, KK_EV_PAD,
       KK_EV_ENCODER, KK_EV_COMBINATION, KK_EV_PEDAL, KK_EV_FADER };
/* Implemented by the KamoteKeys performance core (app/kk_controls.c). */
void kk_control_event(unsigned kind, unsigned id, int value);
void kk_controls_panic(void);
/* Deferred device-preference save (app/project.c). */
void groove_settings_panel_changed(void);
/* FM-1 input adapter (app/kamotkeys/kk_fm1_input.c): diff one controls
 * snapshot against the last and emit events; encoders go to kk.encoder. */
struct fm1_controls;
void kk_fm1_input(const struct fm1_controls *controls);
void kk_fm1_input_reset(void);
/* Key LED for a physical key 0..26 / button LED for button 0..13. */
unsigned kk_fm1_key_led(unsigned key, uint32_t boot_ms);
unsigned kk_fm1_button_led(unsigned button);
#endif

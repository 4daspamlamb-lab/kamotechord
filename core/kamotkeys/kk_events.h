/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys physical event interface, shared by all devices. Each device's
 * input code (SMK-37: app/board/scan.c; FM-1: app/kamotkeys/kk_fm1_input.c)
 * turns its hardware into these events. */
#ifndef KK_EVENTS_H
#define KK_EVENTS_H
#include "fm1.h"
enum { KK_EV_BUTTON, KK_EV_KEY, KK_EV_PRESSURE, KK_EV_PAD,
       KK_EV_ENCODER, KK_EV_COMBINATION, KK_EV_PEDAL, KK_EV_FADER };
/* Implemented by the KamoteKeys performance core (kk_controls.c). */
void kk_control_event(unsigned kind, unsigned id, int value);
void kk_controls_panic(void);
/* Deferred device-preference save (app/project.c). */
void groove_settings_panel_changed(void);
#endif

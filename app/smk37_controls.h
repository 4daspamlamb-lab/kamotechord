/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef SMK37_CONTROLS_H
#define SMK37_CONTROLS_H
#include "fm1.h"
/* Event kinds produced by app/board/scan.c. */
enum { SMK37_BUTTON, SMK37_KEY, SMK37_PRESSURE, SMK37_PAD,
       SMK37_ENCODER, SMK37_COMBINATION, SMK37_PEDAL, SMK37_FADER };
/* Implemented by the KamoteKeys performance core (app/smk37_controls.c). */
void smk37_control_event(unsigned kind, unsigned id, int value);
void smk37_controls_panic(void);
/* Deferred device-preference save (app/project.c). */
void groove_settings_panel_changed(void);
/* Pad LED bit timing preset (app/board/panel.c), 0 = SMK37 OPEN original. */
enum { SMK37_RGB_TIMINGS = 8 };
void smk37_rgb_timing(unsigned timing);
/* Button LED brightness: 0 off, 1 dim, 2 bright (app/board/scan.c). */
void smk37_led_level(unsigned id, unsigned level);
/* SMK37 OPEN panel hooks still used by the browser simulator
 * (web/engine/Simulator.c); KamoteKeys provides them only in FM1_OPEN_WEB
 * builds, see app/kamotkeys/kk_web_compat.c. */
void smk37_controls_snapshot(struct fm1_controls *out);
void smk37_controls_release(void);
unsigned smk37_fader_bank(void);
unsigned smk37_pad_steps(void);
unsigned smk37_shift(void);
unsigned smk37_pickup_mask(void);
unsigned smk37_master_level(void);
void smk37_master_set(unsigned level);
unsigned smk37_performance_track(unsigned pad);
int smk37_performance_octave(unsigned pad);
unsigned smk37_pad_focus(void);
unsigned smk37_pad_aftertouch(void);
unsigned smk37_screen_pad_key(unsigned pad);
#endif

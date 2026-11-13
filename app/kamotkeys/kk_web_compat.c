/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Browser-workbench compatibility (FM1_OPEN_WEB builds only).
 *
 * `build.bat <model> --package` embeds the firmware into the SMK37 OPEN
 * workbench page, which also compiles the browser simulator
 * (web/engine/Simulator.c) from these app sources. The simulator still calls
 * SMK37 OPEN's panel/editor hooks; KamotKeys has no such editor, so these
 * keep the workbench building and its update/backup pages working. The
 * browser preview of the panel is approximate, not a KamotKeys emulator. */
#ifdef FM1_OPEN_WEB
#include "kk_perform.h"
#include "kk_midi.h"
#include "instrument.h"
#include <string.h>

static unsigned master_level = 1023;
volatile uint32_t smk37_queue_overflows, smk37_rgb_frames;   /* board-only counters */
static uint32_t web_rgb[16];
static uint8_t web_leds[19];

void smk37_controls_snapshot(struct fm1_controls *out) { memset(out, 0, sizeof *out); out->knob = (uint16_t)master_level; }
void smk37_controls_release(void) { smk37_controls_panic(); }
unsigned smk37_fader_bank(void) { return 0; }
unsigned smk37_pad_steps(void) { return 0; }
unsigned smk37_shift(void) { return 0; }
unsigned smk37_pickup_mask(void) { return 0; }
unsigned smk37_master_level(void) { return master_level; }
void smk37_master_set(unsigned level) { master_level = level > 1023 ? 1023 : level; }
unsigned smk37_performance_track(unsigned pad) { return pad && kk.mode == KK_MODE_DRUM ? 7 : 0; }
int smk37_performance_octave(unsigned pad) { (void)pad; return kk.octave < -2 ? -2 : kk.octave > 2 ? 2 : kk.octave; }
unsigned smk37_pad_focus(void) { return 0; }
unsigned smk37_pad_aftertouch(void) { return 0; }
unsigned smk37_screen_pad_key(unsigned pad) { (void)pad; return 255; }

void smk37_rgb_set(unsigned id, unsigned rgb) { if (id < 16) web_rgb[id] = rgb; }
void smk37_rgb_service(void) {}
void smk37_rgb_touch(void) {}
void smk37_rgb_timing(unsigned timing) { (void)timing; }
void smk37_led_level(unsigned id, unsigned level) { if (id < 19) web_leds[id] = (uint8_t)level; }

void groove_screen_snapshot(uint8_t state[8])
{
    memset(state, 0, 8);
    state[0] = (uint8_t)(kk.mode == KK_MODE_DRUM ? 7 : 0);
    state[7] = (uint8_t)(kk.octave + 24);
}
int groove_screen_open(unsigned target) { (void)target; return 0; }
int groove_screen_preset(unsigned slot) { (void)slot; return GROOVE_PROJECT_INVALID; }
unsigned groove_screen_preset_position(void) { return 0; }
static char web_knobs[4][2][16];
static uint8_t web_targets[32 * 32];
const char *groove_screen_web_knobs(void) { return &web_knobs[0][0][0]; }
unsigned groove_screen_web_first(void) { return 0; }
int groove_screen_web_step(unsigned step, unsigned toggle) { (void)step; (void)toggle; return -1; }
const void *groove_screen_web_targets(void) { return web_targets; }
unsigned groove_screen_web_target_count(void) { return 0; }
unsigned groove_screen_web_context(void) { return 0; }
unsigned groove_screen_web_mask(void) { return 0; }
int groove_screen_web_adjust(unsigned context, unsigned action, unsigned index, unsigned owner, int delta)
{ (void)context; (void)action; (void)index; (void)owner; (void)delta; return -1; }
void groove_screen_web_fine(unsigned fine) { (void)fine; }
#else
/* Firmware builds: nothing here (keeps the translation unit non-empty). */
typedef int kk_web_compat_unused;
#endif

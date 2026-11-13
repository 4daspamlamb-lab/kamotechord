/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Browser-workbench compatibility (FM1_OPEN_WEB builds only).
 *
 * `build.bat --package` embeds the firmware into the FM1 OPEN workbench page,
 * which also compiles the browser simulator (web/engine/Simulator.c) from
 * these app sources. The simulator calls FM1 OPEN's editor hooks; KamoteKeys
 * has no such editor, so these keep the workbench building. The simulator
 * panel drives KamoteKeys through the same fm1_controls path as hardware. */
#ifdef FM1_OPEN_WEB
#include "kk_perform.h"
#include "kk_midi.h"
#include "instrument.h"
#include <string.h>

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

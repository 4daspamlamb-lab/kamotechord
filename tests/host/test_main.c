/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys host tests: drive the real performance/display/router code with
 * synthetic physical events and check the MIDI it produces. Screenshots of
 * the LCD are written as PPM files (converted to PNG by run.sh). */
#include "helpers.h"

unsigned failures, checks;

int main(void)
{
    test_theory();
    test_buttons();
    test_key_scale();
    test_velocity_faders();
    test_pad_modes();
    test_spare_and_refcount();
    test_arp_keys_and_persistence();
    test_fuzz();
    test_display();
    test_031();
    test_live_tempo();
    test_ripple();
    test_step_mode();
    printf("%u checks, %u failures\n", checks, failures);
    return failures != 0;
}

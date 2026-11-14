/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef KK_HOST_STUBS_H
#define KK_HOST_STUBS_H
#include <stddef.h>
#include <stdint.h>
enum { HOST_LOG = 1 << 20 };
struct host_msg { uint8_t port, n, b[3]; };
extern uint32_t host_ms;
extern struct host_msg host_out[HOST_LOG], host_local[HOST_LOG];
extern unsigned host_ble_busy;
extern unsigned host_out_n, host_local_n, host_ready_ports, host_settings_changes;
extern uint16_t host_fb[240 * 240];
extern uint32_t host_rgb[16];
extern uint8_t host_led[19];
extern unsigned host_rgb_changes, host_rgb_timing;
void host_clear(void);
unsigned host_engine_program(void);
unsigned host_engine_tempo(void);
#endif

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys central MIDI note engine. Every note from keys, chords, pads,
 * ARP and Note Repeat goes through here. Each (channel, note) carries an
 * owner count: Note On is transmitted on 0->1, Note Off on 1->0, so one
 * source can never cut a note still held by another and nothing hangs. */
#ifndef KK_MIDI_H
#define KK_MIDI_H
#include <stddef.h>
#include <stdint.h>

/* External output port policy (device preference, saved). */
enum { KK_PORTS_AUTO, KK_PORTS_ALL, KK_PORTS_USB, KK_PORTS_BLE, KK_PORTS_MODES };

void kk_midi_init(void);
void kk_midi_note_on(unsigned channel, unsigned note, unsigned velocity);
void kk_midi_note_off(unsigned channel, unsigned note);
void kk_midi_cc(unsigned channel, unsigned controller, unsigned value);
/* Channel voice message to the local synth and the external ports. */
void kk_midi_message(const uint8_t *message, size_t bytes);
/* Release everything this engine owns (mode changes, panic, disconnects). */
void kk_midi_all_off(void);
unsigned kk_midi_count(unsigned channel, unsigned note);
/* Sounding notes on a channel (ascending). Returns count, at most max. */
unsigned kk_midi_sounding(unsigned channel, uint8_t *notes, unsigned max);
uint32_t kk_midi_revision(void);   /* changes whenever the sounding set changes */

void kk_midi_set_ports(unsigned mode);
unsigned kk_midi_ports_mode(void);
void kk_midi_set_local(unsigned on);   /* internal synth on/off */
unsigned kk_midi_local(void);
/* Current external destination mask (bit 0 DIN, 1 USB, 2 BLE). */
unsigned kk_midi_port_mask(void);

/* Incoming MIDI (routing task, from fm1_app_midi_received): records Clip
 * Mode feedback, the Remote Script hello and host note activity. Returns
 * nonzero when the message must not be posted to the internal synth. */
int kk_midi_input(unsigned source, const uint8_t *message, size_t bytes);
unsigned kk_midi_remote(unsigned channel, unsigned note);  /* host note held */
void kk_midi_remote_clear(void);
unsigned kk_midi_clip_state(unsigned pad);                 /* KK_CLIP_* */
unsigned kk_midi_host_protocol(void);                      /* 0 = no script */
/* Live's tempo in tenths of BPM from the Remote Script, 0 = none (no script,
 * or disconnected). Ableton has priority over the KamoteKeys tempo. */
unsigned kk_midi_host_tempo_x10(void);
uint32_t kk_midi_input_revision(void);
void kk_midi_clip_sync_request(void);

/* Implemented in app/midi_router.c: send through the router's per-port
 * ownership tracking and disconnect recovery. */
void groove_midi_kk_send(const uint8_t *message, size_t bytes, unsigned ports);
#endif

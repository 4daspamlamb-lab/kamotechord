/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamoteKeys shared music-theory engine: scales, keyboard/pad/drum mapping,
 * chord construction and chord naming. Pure functions, no hardware access,
 * shared by keys, pads, display and LEDs (one engine for everything). */
#ifndef KK_THEORY_H
#define KK_THEORY_H
#include <stdint.h>

enum { KK_SCALES = 23, KK_MAX_CHORD = 6 };

/* Chord-pad modifier bits. Normal/Key Scale Mode chord pads:
 * Pad 1 MAJ, 2 MIN, 3 DIM, 4 SUS4, 9 "7", 10 MAJ7, 11 "6", 12 "9"; pad 13 is
 * the chord LOCK pad. KK_MOD_SCALE (diatonic triad) remains in the engine. */
enum {
    KK_MOD_MAJ = 1, KK_MOD_MIN = 2, KK_MOD_DIM = 4, KK_MOD_SUS4 = 8,
    KK_MOD_7 = 16, KK_MOD_MAJ7 = 32, KK_MOD_6 = 64, KK_MOD_9 = 128,
    KK_MOD_SCALE = 256
};

struct kk_scale_ctx { uint8_t scale, root; };   /* scale 0..22, root pc 0..11 */

const char *kk_scale_name(unsigned scale);       /* full name, e.g. "Minor Pentatonic" */
unsigned kk_scale_mask(unsigned scale);          /* 12-bit pitch-class set from C */
unsigned kk_scale_size(unsigned scale);
/* Semitone offset of (possibly negative) scale degree from the tonic. */
int kk_scale_step(unsigned scale, int degree);
int kk_in_scale(const struct kk_scale_ctx *s, int note);

const char *kk_note_name(unsigned pc);           /* "C", "C#", ... */
/* "A4" style; buffer >= 5. MIDI 60 = C4. */
void kk_note_label(char *out, int note);

/* Keyboard mapping. Returns -1 for a disabled key (black key in Key Scale
 * Mode) or an out-of-range result. key = physical scan id 0..36. */
int kk_key_note(unsigned key, int octave);
int kk_key_scale_note(unsigned key, int octave, const struct kk_scale_ctx *s);
int kk_pad_scale_note(unsigned pad, int octave, const struct kk_scale_ctx *s);
int kk_drum_note(unsigned pad, int octave);      /* pad 0..15 -> note or -1 */
int kk_spare_pad_note(unsigned pad);             /* -1 if not a spare pad   */
unsigned kk_chord_pad_modifier(unsigned pad);    /* 0 if not a chord pad    */
unsigned kk_is_lock_pad(unsigned pad);           /* pad 13: chord LOCK      */

/* Drum families for colours/labels. */
enum { KK_DRUM_KICK, KK_DRUM_SNARE, KK_DRUM_HIHAT, KK_DRUM_TOM, KK_DRUM_RIM,
       KK_DRUM_CLAP, KK_DRUM_CYMBAL, KK_DRUM_PERC, KK_DRUM_OTHER };
unsigned kk_drum_family(int note);
const char *kk_drum_label(int note);             /* short label, <= 4 chars */

/* Build the chord for a root note with the given modifier mask. Output is
 * ascending, unique, in MIDI range; returns the note count (>= 1 when the
 * root is valid). *chord_root receives the root (always notes[0]). */
unsigned kk_chord_build(int root, unsigned mods, const struct kk_scale_ctx *s,
                        uint8_t notes[KK_MAX_CHORD]);

/* Name a set of sounding notes. known_root >= 0 names relative to that root
 * (generated chords); -1 detects the most plausible root. Writes e.g. "Am7",
 * "C", "F#dim7", or a single note label "A4" for one note. Returns the
 * detected root note (lowest matching), or -1 for an empty set. out >= 16. */
int kk_chord_name(const uint8_t *notes, unsigned count, int known_root, char *out);

#endif

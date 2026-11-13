/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * KamotKeys shared music-theory engine. */
#include "kk_theory.h"
#include "kk_config.h"
#include <string.h>

/* Pitch-class sets, bit n = n semitones above the tonic. */
static const uint16_t scale_masks[KK_SCALES] = {
    0xAB5, /* Major                 0 2 4 5 7 9 11   */
    0x5AD, /* Natural Minor         0 2 3 5 7 8 10   */
    0x6AD, /* Dorian                0 2 3 5 7 9 10   */
    0x5AB, /* Phrygian              0 1 3 5 7 8 10   */
    0xAD5, /* Lydian                0 2 4 6 7 9 11   */
    0x6B5, /* Mixolydian            0 2 4 5 7 9 10   */
    0x56B, /* Locrian               0 1 3 5 6 8 10   */
    0x9AD, /* Harmonic Minor        0 2 3 5 7 8 11   */
    0xAAD, /* Melodic Minor         0 2 3 5 7 9 11   */
    0x295, /* Major Pentatonic      0 2 4 7 9        */
    0x4A9, /* Minor Pentatonic      0 3 5 7 10       */
    0x4E9, /* Blues                 0 3 5 6 7 10     */
    0x5B3, /* Phrygian Dominant     0 1 4 5 7 8 10   */
    0x9B3, /* Double Harmonic       0 1 4 5 7 8 11   */
    0x18D, /* Hirajoshi             0 2 3 7 8        */
    0x555, /* Whole Tone            0 2 4 6 8 10     */
    0xB6D, /* Diminished Whole-Half 0 2 3 5 6 8 9 11 */
    0x6DB, /* Diminished Half-Whole 0 1 3 4 6 7 9 10 */
    0xEB5, /* Bebop Dominant        0 2 4 5 7 9 10 11*/
    0xBB5, /* Bebop Major           0 2 4 5 7 8 9 11 */
    0x6BD, /* Bebop Minor           0 2 3 4 5 7 9 10 */
    0xBB5, /* 6-Diminished          0 2 4 5 7 8 9 11 */
    0xBAD  /* Minor 6-Diminished    0 2 3 5 7 8 9 11 */
};
static const char *const scale_names[KK_SCALES] = {
    "Major", "Natural Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian",
    "Locrian", "Harmonic Minor", "Melodic Minor", "Major Pentatonic",
    "Minor Pentatonic", "Blues", "Phrygian Dominant", "Double Harmonic",
    "Hirajoshi", "Whole Tone", "Diminished Whole-Half", "Diminished Half-Whole",
    "Bebop Dominant", "Bebop Major", "Bebop Minor", "6-Diminished",
    "Minor 6-Diminished"};
static const char *const note_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

const char *kk_scale_name(unsigned scale) { return scale_names[scale % KK_SCALES]; }
unsigned kk_scale_mask(unsigned scale) { return scale_masks[scale % KK_SCALES]; }
unsigned kk_scale_size(unsigned scale) { return (unsigned)__builtin_popcount(kk_scale_mask(scale)); }
const char *kk_note_name(unsigned pc) { return note_names[pc % 12U]; }

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int kk_scale_step(unsigned scale, int degree)
{
    unsigned mask = kk_scale_mask(scale), n = (unsigned)__builtin_popcount(mask);
    int octave = floor_div(degree, (int)n);
    unsigned index = (unsigned)(degree - octave * (int)n);
    for (unsigned pc = 0; pc < 12; ++pc)
        if (mask & (1U << pc)) {
            if (!index) return pc + 12 * octave;
            --index;
        }
    return 12 * octave; /* unreachable: every mask contains the tonic */
}
int kk_in_scale(const struct kk_scale_ctx *s, int note)
{
    unsigned pc = (unsigned)((note % 12 + 12 - s->root % 12) % 12);
    return (kk_scale_mask(s->scale) >> pc) & 1U;
}
void kk_note_label(char *out, int note)
{
    const char *name = note_names[((note % 12) + 12) % 12];
    int octave = floor_div(note, 12) - 1;
    unsigned n = 0;
    while (*name) out[n++] = *name++;
    if (octave < 0) { out[n++] = '-'; octave = -octave; }
    out[n++] = (char)('0' + octave % 10);
    out[n] = 0;
}

static int valid(int note) { return note >= 0 && note <= 127 ? note : -1; }

int kk_key_note(unsigned key, int octave)
{
    if (key >= KK_KEYS) return -1;
    return valid(KK_KEY_BASE_NOTE + (int)key + 12 * octave);
}
/* White-key index inside an octave, -1 for black keys. */
static const int8_t white_index[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
int kk_key_scale_note(unsigned key, int octave, const struct kk_scale_ctx *s)
{
    if (key >= KK_KEYS || white_index[key % 12] < 0) return -1;
    int white = (int)(key / 12) * 7 + white_index[key % 12];
    int anchor = (KK_ANCHOR_KEY / 12) * 7;     /* second C = white key 7 */
    int degree = white - anchor;               /* negative below the anchor */
    return valid(KK_ANCHOR_NOTE + 12 * octave + (s->root % 12) + kk_scale_step(s->scale, degree));
}
int kk_pad_scale_note(unsigned pad, int octave, const struct kk_scale_ctx *s)
{
    if (pad >= KK_PADS) return -1;
    return valid(48 + 12 * octave + (s->root % 12) + kk_scale_step(s->scale, (int)pad));
}
static const uint8_t drum_base[KK_PADS] = {36, 38, 40, 42, 44, 46, 48, 50,
                                           37, 39, 41, 43, 45, 47, 49, 51};
int kk_drum_note(unsigned pad, int octave)
{
    if (pad >= KK_PADS) return -1;
    return valid(drum_base[pad] + 12 * octave);
}
int kk_spare_pad_note(unsigned pad)
{
    /* Pads 5-8 -> 84..87, pads 14-16 -> 88..90 (one-based pad numbers). */
    if (pad >= 4 && pad <= 7) return 84 + (int)(pad - 4);
    if (pad >= 13 && pad <= 15) return 88 + (int)(pad - 13);
    return -1;
}
unsigned kk_chord_pad_modifier(unsigned pad)
{
    static const uint16_t mods[KK_PADS] = {
        KK_MOD_MAJ, KK_MOD_MIN, KK_MOD_DIM, KK_MOD_SUS4, 0, 0, 0, 0,
        KK_MOD_7, KK_MOD_MAJ7, KK_MOD_6, KK_MOD_9, 0, 0, 0, 0};
    return pad < KK_PADS ? mods[pad] : 0;
}
unsigned kk_is_lock_pad(unsigned pad) { return pad == 12; }

unsigned kk_drum_family(int note)
{
    switch (note) {
    case 35: case 36: return KK_DRUM_KICK;
    case 37: return KK_DRUM_RIM;
    case 38: case 40: return KK_DRUM_SNARE;
    case 39: return KK_DRUM_CLAP;
    case 42: case 44: case 46: return KK_DRUM_HIHAT;
    case 41: case 43: case 45: case 47: case 48: case 50: return KK_DRUM_TOM;
    case 49: case 51: case 52: case 53: case 55: case 57: case 59: return KK_DRUM_CYMBAL;
    default: return note >= 54 && note <= 81 ? KK_DRUM_PERC : KK_DRUM_OTHER;
    }
}
const char *kk_drum_label(int note)
{
    static const char *const gm[47] = { /* 35..81 */
        "KIK2", "KICK", "RIM", "SNR", "CLAP", "SNR2", "LFT", "CHH", "HFT", "PHH",
        "LTOM", "OHH", "LMT", "HMT", "CRS1", "HTOM", "RIDE", "CHN", "BELL", "TAMB",
        "SPL", "COWB", "CRS2", "VIBR", "RID2", "HBNG", "LBNG", "MCNG", "OCNG", "LCNG",
        "HTIM", "LTIM", "HAGO", "LAGO", "CABS", "MRCS", "WHS", "WHL", "GUIS", "GUIL",
        "CLAV", "HWB", "LWB", "MCUI", "OCUI", "MTRI", "OTRI"};
    return note >= 35 && note <= 81 ? gm[note - 35] : "--";
}

/* Interval of scale degree (k + steps) above degree k, for diatonic stacking. */
static int diatonic(unsigned scale, int k, int steps)
{
    return kk_scale_step(scale, k + steps) - kk_scale_step(scale, k);
}
static void add(uint8_t *set, unsigned *n, int note)
{
    if (note < 0 || note > 127 || *n >= KK_MAX_CHORD) return;
    for (unsigned i = 0; i < *n; ++i) if (set[i] == note) return;
    unsigned i = *n;
    while (i && set[i - 1] > note) { set[i] = set[i - 1]; --i; }
    set[i] = (uint8_t)note; ++*n;
}
unsigned kk_chord_build(int root, unsigned mods, const struct kk_scale_ctx *s, uint8_t notes[KK_MAX_CHORD])
{
    unsigned n = 0;
    if (root < 0 || root > 127) return 0;
    add(notes, &n, root);
    if (!mods) return n;
    unsigned quality = mods & (KK_MOD_MAJ | KK_MOD_MIN | KK_MOD_DIM | KK_MOD_SUS4);
    /* Diatonic when SCALE is held, or when only extensions are held and the
     * root belongs to the current scale. */
    int degree = -1;
    if ((mods & KK_MOD_SCALE) || !quality) {
        quality = 0;
        if (kk_in_scale(s, root)) {
            unsigned pc = (unsigned)((root - (int)s->root) % 12 + 12) % 12;
            unsigned mask = kk_scale_mask(s->scale);
            degree = __builtin_popcount(mask & ((1U << pc) - 1U));
        } else quality = KK_MOD_MAJ; /* chromatic root outside the scale */
    }
    if (degree >= 0) {
        add(notes, &n, root + diatonic(s->scale, degree, 2));
        add(notes, &n, root + diatonic(s->scale, degree, 4));
        if (mods & (KK_MOD_7 | KK_MOD_MAJ7)) add(notes, &n, root + diatonic(s->scale, degree, 6));
        if (mods & KK_MOD_6) add(notes, &n, root + diatonic(s->scale, degree, 5));
        if (mods & KK_MOD_9) add(notes, &n, root + 12 + diatonic(s->scale, degree, 1));
        return n;
    }
    static const int8_t triads[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 5, 7}};
    unsigned q = quality & KK_MOD_MAJ ? 0 : quality & KK_MOD_MIN ? 1 : quality & KK_MOD_DIM ? 2 : 3;
    add(notes, &n, root + triads[q][1]);
    add(notes, &n, root + triads[q][2]);
    if (mods & KK_MOD_6) add(notes, &n, root + 9);
    if (mods & KK_MOD_7) add(notes, &n, root + (q == 2 ? 9 : 10));
    if (mods & KK_MOD_MAJ7) add(notes, &n, root + 11);
    if (mods & KK_MOD_9) add(notes, &n, root + 14);
    return n;
}

/* Name a pitch-class set relative to a root. Returns a plausibility score;
 * writes the suffix. */
static int name_suffix(unsigned set, char *out)
{
    unsigned has[12];
    for (unsigned i = 0; i < 12; ++i) has[i] = (set >> i) & 1U;
    int score = 0;
    unsigned third = has[4] ? 4 : has[3] ? 3 : 0;
    unsigned fifth = has[7] ? 7 : 0;
    unsigned used = 1U | (1U << third) | (1U << 7);
    unsigned dim = 0, aug = 0, sus = 0;
    out[0] = 0;
    if (third == 4) {
        score += 4;
        if (!fifth && has[8]) { aug = 1; used |= 1U << 8; score += 1; }
    } else if (third == 3) {
        score += 4;
        if (!fifth && has[6]) { dim = 1; used |= 1U << 6; score += 1; }
    } else if (has[5]) { sus = 4; used |= 1U << 5; score += 2; }
    else if (has[2]) { sus = 2; used |= 1U << 2; score += 2; }
    if (fifth) score += 3;
    unsigned b7 = has[10], m7 = has[11], six = has[9] && !dim, d7 = dim && has[9] && !b7;
    unsigned nine = has[2] && sus != 2;
    if (b7) used |= 1U << 10;
    if (m7) used |= 1U << 11;
    if (has[9]) used |= 1U << 9;
    if (nine) used |= 1U << 2;
    char *p = out;
#define PUT(s) do { const char *t_ = (s); while (*t_) *p++ = *t_++; } while (0)
    if (dim && b7) { PUT("m7b5"); dim = 0; b7 = 0; }
    else if (d7) { PUT("dim7"); dim = 0; }
    else {
        if (third == 3 && !dim) PUT("m");
        if (dim) PUT("dim");
        if (aug) PUT("aug");
        if (m7) { PUT(third == 3 ? "Maj" : "maj"); PUT(nine ? "9" : "7"); nine = 0; }
        else if (b7) { PUT(nine ? "9" : "7"); nine = 0; }
        if (six && !(b7 || m7)) { PUT(nine ? "6/9" : "6"); nine = 0; }
        else if (six) PUT("(13)");
        if (nine) PUT("add9");
    }
    if (sus == 4) PUT("sus4");
    else if (sus == 2) PUT("sus2");
    if (!third && !sus && fifth && set == ((1U << 7) | 1U)) PUT("5");
#undef PUT
    *p = 0;
    score -= 2 * __builtin_popcount(set & ~used);
    return score;
}
int kk_chord_name(const uint8_t *notes, unsigned count, int known_root, char *out)
{
    out[0] = 0;
    if (!count) return -1;
    unsigned pcs = 0;
    int low = 127;
    for (unsigned i = 0; i < count; ++i) {
        pcs |= 1U << (notes[i] % 12);
        if (notes[i] < low) low = notes[i];
    }
    if (count == 1 || pcs == (1U << (low % 12))) { kk_note_label(out, low); return low; }
    int best_root = known_root, best = -1000;
    char suffix[16], best_suffix[16];
    if (known_root >= 0) {
        unsigned r = (unsigned)known_root % 12;
        unsigned rel = ((pcs >> r) | (pcs << (12 - r))) & 0xfff;
        name_suffix(rel, best_suffix);
    } else {
        /* Try every sounding pitch class; ties prefer the bass note. */
        for (unsigned i = 0; i < count; ++i) {
            unsigned r = notes[i] % 12;
            unsigned rel = ((pcs >> r) | (pcs << (12 - r))) & 0xfff;
            int score = name_suffix(rel, suffix) + (notes[i] == low ? 1 : 0);
            if (score > best) { best = score; best_root = notes[i]; memcpy(best_suffix, suffix, sizeof suffix); }
        }
        if (best < 3) {
            /* No convincing chord: list up to three note names. */
            unsigned n = 0, shown = 0;
            for (unsigned pc = 0; pc < 12 && shown < 3; ++pc) if (pcs & (1U << pc)) {
                const char *s = note_names[pc];
                if (n) out[n++] = ' ';
                while (*s) out[n++] = *s++;
                ++shown;
            }
            out[n] = 0;
            return low;
        }
    }
    const char *root_name = note_names[(unsigned)best_root % 12];
    unsigned n = 0;
    while (*root_name) out[n++] = *root_name++;
    for (unsigned i = 0; best_suffix[i] && n < 15; ++i) out[n++] = best_suffix[i];
    out[n] = 0;
    return best_root;
}

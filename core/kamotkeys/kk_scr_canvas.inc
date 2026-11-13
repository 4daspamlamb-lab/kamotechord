/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Font and band canvas drawing primitives.
 * Part of core/screen.c: #included there, never compiled on its own (the
 * hash-locked SDK build gives -Oz only to that file). */
/* ----------------------------------------------------------------- font */
/* 5x7 ASCII 32..126, column major, bit 0 at the top. */
static const uint8_t font[95][5] = {
    {0,0,0,0,0},{0,0,0x5f,0,0},{0,7,0,7,0},{0x14,0x7f,0x14,0x7f,0x14},{0x24,0x2a,0x7f,0x2a,0x12},
    {0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1c,0x22,0x41,0},{0,0x41,0x22,0x1c,0},
    {8,0x2a,0x1c,0x2a,8},{8,8,0x3e,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},
    {0x20,0x10,8,4,2},{0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},
    {6,0x49,0x49,0x29,0x1e},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,8,0},{2,1,0x51,9,6},{0x32,0x49,0x79,0x41,0x3e},{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,1,1},{0x3e,0x41,0x41,0x51,0x32},
    {0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
    {0x7f,2,4,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},
    {0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
    {0x7f,0x20,0x18,0x20,0x7f},{0x63,0x14,8,0x14,0x63},{3,4,0x78,4,3},{0x61,0x51,0x49,0x45,0x43},{0,0x7f,0x41,0x41,0},
    {2,4,8,0x10,0x20},{0,0x41,0x41,0x7f,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40},{0,1,2,4,0},
    {0x20,0x54,0x54,0x54,0x78},{0x7f,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7f},{0x38,0x54,0x54,0x54,0x18},
    {8,0x7e,9,1,2},{8,0x14,0x54,0x54,0x3c},{0x7f,8,4,4,0x78},{0,0x44,0x7d,0x40,0},{0x20,0x40,0x44,0x3d,0},
    {0,0x7f,0x10,0x28,0x44},{0,0x41,0x7f,0x40,0},{0x7c,4,0x18,4,0x78},{0x7c,8,4,4,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7c,0x14,0x14,0x14,8},{8,0x14,0x14,0x18,0x7c},{0x7c,8,4,4,8},{0x48,0x54,0x54,0x54,0x20},{4,0x3f,0x44,0x40,0x20},
    {0x3c,0x40,0x40,0x20,0x7c},{0x1c,0x20,0x40,0x20,0x1c},{0x3c,0x40,0x30,0x40,0x3c},{0x44,0x28,0x10,0x28,0x44},{0xc,0x50,0x50,0x50,0x3c},
    {0x44,0x64,0x54,0x4c,0x44},{0,8,0x36,0x41,0},{0,0,0x7f,0,0},{0,0x41,0x36,8,0},{2,1,2,4,2}};

/* ---------------------------------------------------------- band canvas */
enum { BAND_ROWS = 16 };
static uint16_t band[FM1_SCREEN_WIDTH * BAND_ROWS];
static int clip_x0, clip_y0, clip_x1, clip_y1;   /* current band window, [x0,x1) */
static unsigned stride;

static void fill(int x, int y, int w, int h, uint16_t c)
{
    int x0 = x > clip_x0 ? x : clip_x0, x1 = x + w < clip_x1 ? x + w : clip_x1;
    int y0 = y > clip_y0 ? y : clip_y0, y1 = y + h < clip_y1 ? y + h : clip_y1;
    for (int row = y0; row < y1; ++row) {
        uint16_t *p = band + (row - clip_y0) * (int)stride + (x0 - clip_x0);
        for (int col = x0; col < x1; ++col) *p++ = c;
    }
}
static inline void dot(int x, int y, uint16_t c)
{
    if (x >= clip_x0 && x < clip_x1 && y >= clip_y0 && y < clip_y1)
        band[(y - clip_y0) * (int)stride + (x - clip_x0)] = c;
}
static unsigned glyph_on(const uint8_t *g, int gx, int gy)
{
    return gx >= 0 && gx < 5 && gy >= 0 && gy < 7 && ((g[gx] >> gy) & 1U);
}
/* Text with transparent background. Scales >= 3 round diagonal steps so
 * large note and chord names read smoothly instead of as blocky pixels. */
static int text(int x, int y, const char *s, unsigned scale, uint16_t color)
{
    int w = 6 * (int)scale, h = 7 * (int)scale;
    if (y >= clip_y1 || y + h <= clip_y0) { while (*s) { x += w; ++s; } return x; }
    for (; *s; ++s, x += w) {
        if (x >= clip_x1) { while (*s) { x += w; ++s; } break; }
        if (x + w <= clip_x0) continue;
        unsigned c = (unsigned char)*s;
        const uint8_t *g = font[c >= 32 && c < 127 ? c - 32 : 0];
        int s_ = (int)scale;
        if (scale < 3) {
            for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx)
                if (glyph_on(g, gx, gy)) fill(x + gx * s_, y + gy * s_, s_, s_, color);
            continue;
        }
        /* Large text uses a light stroke: each glyph pixel is a (scale-1)
         * square joined to its right/lower neighbours, and diagonal steps
         * get a bridge, so names read as clean lines instead of blocks. */
        int k = s_ - 1;
        for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx) {
            if (!glyph_on(g, gx, gy)) continue;
            int px = x + gx * s_, py = y + gy * s_;
            fill(px, py, k, k, color);
            if (glyph_on(g, gx + 1, gy)) fill(px + k, py, s_ - k, k, color);
            if (glyph_on(g, gx, gy + 1)) fill(px, py + k, k, s_ - k, color);
            if (glyph_on(g, gx + 1, gy + 1) && !glyph_on(g, gx + 1, gy) && !glyph_on(g, gx, gy + 1))
                fill(px + s_ / 2, py + s_ / 2, k, k, color);
            if (glyph_on(g, gx - 1, gy + 1) && !glyph_on(g, gx - 1, gy) && !glyph_on(g, gx, gy + 1))
                fill(px - s_ / 2, py + s_ / 2, k, k, color);
        }
    }
    return x;
}
static int text_width(const char *s, unsigned scale) { return (int)strlen(s) * 6 * (int)scale - (int)scale; }
static void text_center(int cx, int y, const char *s, unsigned scale, uint16_t color)
{
    text(cx - text_width(s, scale) / 2, y, s, scale, color);
}
static void text_right(int right, int y, const char *s, unsigned scale, uint16_t color)
{
    text(right - text_width(s, scale), y, s, scale, color);
}
/* Rounded "pill" rectangle (1 px corner cut). */
static void pill(int x, int y, int w, int h, uint16_t c)
{
    fill(x + 1, y, w - 2, h, c); fill(x, y + 1, 1, h - 2, c); fill(x + w - 1, y + 1, 1, h - 2, c);
}
static char *decimal(char *out, int value)
{
    char tmp[12]; unsigned n = 0, neg = value < 0;
    unsigned v = neg ? (unsigned)-value : (unsigned)value;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    char *p = out;
    if (neg) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}
static char *append(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

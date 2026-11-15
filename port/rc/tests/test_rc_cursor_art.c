/* R&C patch 7065: the wrench pointer (libs/input/pad_cursor_art.h, built with PAD_CURSOR_WRENCH). Checks at 32 and 48 px:
 * the hotspot lies inside the bitmap, in the jaw's open mouth (transparent), with metal on both jaw tips beside it; the
 * handle reaches the lower right; every pixel is premultiplied (colour <= alpha). Writes a PPM of the 48 px pointer on
 * grey when given a path (for a look). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "pad_cursor_art.h"

static int fails;
#define CHECK(c, m) do { if (c) printf("[PASS] %s\n", m); else { printf("[FAIL] %s\n", m); fails++; } } while (0)
static unsigned A(const uint32_t* px, int size, int x, int y) { return px[y * size + x] >> 24; }

int main(int argc, char** argv)
{
    for (int size = 32; size <= 48; size += 16) {
        uint32_t* px = calloc((size_t)size * size, 4);
        int hx = -1, hy = -1;
        pad_cursor_render_wrench(px, size, &hx, &hy);
        char m[160];
        snprintf(m, sizeof m, "%d px: hotspot (%d,%d) inside the bitmap", size, hx, hy);
        CHECK(hx >= 0 && hy >= 0 && hx < size / 3 && hy < size / 3, m);
        snprintf(m, sizeof m, "%d px: hotspot pixel is the open mouth (alpha %u)", size, A(px, size, hx, hy));
        CHECK(A(px, size, hx, hy) < 64, m);
        int tips = 0;   /* metal above-right and below-left of the mouth: the two jaw tips */
        for (int d = 1; d <= size / 6; d++) {
            if (hx + d < size && hy - d / 2 >= 0 && A(px, size, hx + d, hy - d / 2) > 200) { tips |= 1; }
            if (hy + d < size && hx - d / 2 >= 0 && A(px, size, hx - d / 2, hy + d) > 200) { tips |= 2; }
        }
        snprintf(m, sizeof m, "%d px: both jaw tips are solid beside the hotspot", size);
        CHECK(tips == 3, m);
        int far = 0;
        for (int y = size * 3 / 4; y < size; y++)
            for (int x = size * 3 / 4; x < size; x++) if (A(px, size, x, y) > 200) far = 1;
        snprintf(m, sizeof m, "%d px: the handle reaches the lower-right quarter", size);
        CHECK(far, m);
        int pm = 1;
        for (int i = 0; i < size * size; i++) {
            const uint32_t a = px[i] >> 24;
            if (((px[i] >> 16) & 0xFF) > a || ((px[i] >> 8) & 0xFF) > a || (px[i] & 0xFF) > a) pm = 0;
        }
        snprintf(m, sizeof m, "%d px: every pixel premultiplied", size);
        CHECK(pm, m);
        if (size == 48 && argc > 1) {
            FILE* f = fopen(argv[1], "wb");
            if (f) {
                fprintf(f, "P6 %d %d 255\n", size * 4, size * 4);
                for (int y = 0; y < size * 4; y++)
                    for (int x = 0; x < size * 4; x++) {
                        const uint32_t c = px[(y / 4) * size + x / 4], a = c >> 24;
                        const int hs = (x / 4 == hx && y / 4 == hy);
                        unsigned char o[3] = { (unsigned char)(((c >> 16) & 0xFF) + (255 - a) * 128 / 255),
                                               (unsigned char)(((c >> 8) & 0xFF) + (255 - a) * 128 / 255),
                                               (unsigned char)((c & 0xFF) + (255 - a) * 128 / 255) };
                        if (hs) { o[0] = 255; o[1] = 0; o[2] = 255; }
                        fwrite(o, 1, 3, f);
                    }
                fclose(f);
            }
        }
        free(px);
    }
    return fails ? 1 : 0;
}

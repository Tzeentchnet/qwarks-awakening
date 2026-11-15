/* Host test for R&C patch 7045 (rsx_tex_1d.h): a 1D texture decoded as a width x 1 2D texture samples the same
 * texels as a 1D fetch of the same data, for every u, v, U address mode and guest V address mode, point and linear.
 *
 * A small software sampler follows the D3D12 addressing and filtering rules (texel centres at i + 0.5, bilinear
 * weights, WRAP / MIRROR / CLAMP / BORDER / MIRROR_ONCE). The GCM wrap codes map to those modes as live draw's
 * gcm_wrap does (1 wrap, 2 mirror, 3 and 5 clamp, 4 border, 6-8 mirror once). The test also checks that it can see
 * the hazard the V clamp removes: with the guest's BORDER on V, the Wx1 texture blends in the border colour. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "rsx_tex_1d.h"

enum { M_WRAP, M_MIRROR, M_CLAMP, M_BORDER, M_MIRROR_ONCE };
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, "FAIL: " __VA_ARGS__); fputc('\n', stderr); fails++; } } while (0)

static int mode_of_gcm(uint32_t w)
{
    switch (w & 0xF) {
    case 1: return M_WRAP;
    case 2: return M_MIRROR;
    case 3: case 5: return M_CLAMP;
    case 4: return M_BORDER;
    case 6: case 7: case 8: return M_MIRROR_ONCE;
    default: return M_CLAMP;
    }
}

/* texel index for integer coordinate i in a size-n axis, or -1 for the border colour */
static int address(int i, int n, int mode)
{
    switch (mode) {
    case M_WRAP: { int r = i % n; return r < 0 ? r + n : r; }
    case M_MIRROR: { int p = i % (2 * n); if (p < 0) p += 2 * n; return p < n ? p : 2 * n - 1 - p; }
    case M_CLAMP: return i < 0 ? 0 : i >= n ? n - 1 : i;
    case M_BORDER: return (i < 0 || i >= n) ? -1 : i;
    default: { int a = i < 0 ? -i - 1 : i; return a >= n ? n - 1 : a; }   /* mirror once */
    }
}

#define W 16
static float row[W];
static const float border = 7.0f;

static float texel(int x, int y)   /* y: row index of a height-1 texture, or -1 */
{
    return (x < 0 || y < 0) ? border : row[x];
}

/* 1D fetch: u only */
static float fetch_1d(float u, int mu, int linear)
{
    if (!linear) return texel(address((int)floorf(u * W), W, mu), 0);
    const float x = u * W - 0.5f;
    const int i0 = (int)floorf(x);
    const float f = x - (float)i0;
    return (1 - f) * texel(address(i0, W, mu), 0) + f * texel(address(i0 + 1, W, mu), 0);
}

/* 2D fetch from a W x 1 texture */
static float fetch_2d(float u, float v, int mu, int mv, int linear)
{
    if (!linear) return texel(address((int)floorf(u * W), W, mu), address((int)floorf(v), 1, mv));
    const float x = u * W - 0.5f, y = v - 0.5f;
    const int i0 = (int)floorf(x), j0 = (int)floorf(y);
    const float fx = x - (float)i0, fy = y - (float)j0;
    const int a0 = address(i0, W, mu), a1 = address(i0 + 1, W, mu);
    const int b0 = address(j0, 1, mv), b1 = address(j0 + 1, 1, mv);
    const float r0 = (1 - fx) * texel(a0, b0) + fx * texel(a1, b0);
    const float r1 = (1 - fx) * texel(a0, b1) + fx * texel(a1, b1);
    return (1 - fy) * r0 + fy * r1;
}

int main(void)
{
    for (int i = 0; i < W; i++) row[i] = (float)((i * 37 + 11) % 23) / 4.0f;   /* distinct, not 7.0 */
    CHECK(ld_tex_2d_path(1) && ld_tex_2d_path(2), "dimensions 1 and 2 take the 2D path");
    CHECK(!ld_tex_2d_path(0) && !ld_tex_2d_path(3), "dimensions 0 and 3 do not");
    for (uint32_t g = 1; g <= 8; g++) {
        CHECK(ld_tex_wrap_v(1, g) == 3, "1D V wrap %u -> clamp", g);
        CHECK(ld_tex_wrap_v(2, g) == g, "2D V wrap %u unchanged", g);
        CHECK(ld_tex_wrap_v(2, g | 0x100) == g, "2D V wrap keeps the low nibble only");
    }
    static const float vs[] = { 0.5f, 0.0f, 1.0f, 0.25f, 0.99f, -0.3f, 1.7f, -12.25f, 40.5f, 0.123f };
    long compared = 0, border_hits = 0;
    for (uint32_t gu = 1; gu <= 8; gu++)
        for (uint32_t gv = 1; gv <= 8; gv++)
            for (int linear = 0; linear < 2; linear++)
                for (int k = -64; k <= 96; k++) {
                    const float u = (float)k / 37.0f;
                    const float want = fetch_1d(u, mode_of_gcm(gu), linear);
                    for (unsigned j = 0; j < sizeof vs / sizeof vs[0]; j++) {
                        const float got = fetch_2d(u, vs[j], mode_of_gcm(gu), mode_of_gcm(ld_tex_wrap_v(1, gv)), linear);
                        compared++;
                        if (fabsf(got - want) > 1e-5f) {
                            CHECK(0, "1D vs Wx1: u=%g v=%g wrap u=%u v=%u linear=%d: %g != %g", u, vs[j], gu, gv,
                                  linear, got, want);
                            if (fails > 20) goto out;
                        }
                        /* the raw guest V mode (no clamp): only BORDER may differ, and it must for some v */
                        const float raw = fetch_2d(u, vs[j], mode_of_gcm(gu), mode_of_gcm(gv), linear);
                        if (fabsf(raw - want) > 1e-5f) {
                            CHECK(mode_of_gcm(gv) == M_BORDER, "raw V mode %u differs without BORDER", gv);
                            border_hits++;
                        }
                    }
                }
out:
    CHECK(border_hits > 0, "the BORDER hazard is visible to the test");
    if (fails) { fprintf(stderr, "test_tex_1d: %d failed\n", fails); return 1; }
    printf("test_tex_1d: all passed (%ld fetch pairs; raw BORDER V differs in %ld)\n", compared, border_hits);
    return 0;
}

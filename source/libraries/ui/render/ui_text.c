// source/libraries/ui/render/ui_text.c
// TrueType glyph blitter and utf-8 helpers for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// simd dispatch for the glyph blit
#if defined(__AVX2__) || (defined(_M_X64) && defined(__AVX2__))
#  include <immintrin.h>
#  define UI_TEXT_HAVE_AVX2 1
#elif defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  include <emmintrin.h>
#  define UI_TEXT_HAVE_SSE2 1
#endif

// set true by ui_text.c when the destination surface carries per-pixel alpha
extern bool g_text_dst_alpha_used;

// opaque handle to the cache entry internals; layout matches ui_font.c
typedef struct CacheEntry CacheEntry;

// accessors exposed by ui_font.c
extern CacheEntry* ui_font_glyph_get(UiFont* f, uint16_t gid, uint16_t size_q);
extern uint16_t    ui_font_glyph_index_cached(UiFont* f, uint32_t cp);
extern uint32_t    ui_font_decode_cp(const char* s, int n, int* adv);
extern int         ui_font_cache_w(CacheEntry* e);
extern int         ui_font_cache_h(CacheEntry* e);
extern int         ui_font_cache_off_x(CacheEntry* e);
extern int         ui_font_cache_off_y(CacheEntry* e);
extern float       ui_font_cache_advance(CacheEntry* e);
extern uint8_t*    ui_font_cache_cov(CacheEntry* e);
extern uint16_t*   ui_font_cache_row_x0(CacheEntry* e);
extern uint16_t*   ui_font_cache_row_x1(CacheEntry* e);

// blends one row of a glyph coverage bitmap into the framebuffer.
static void blend_glyph_row(uint32_t* dst, const uint8_t* cov, int n,
                            uint32_t sr, uint32_t sg, uint32_t sb, uint32_t ca) {
    if (ca == 0 || n <= 0) return;                       // fully transparent or empty row
    const bool opaque_ca = (ca == 255);                  // common case: no alpha multiply

    if (ca < 255 || g_text_dst_alpha_used) {
        for (int i = 0; i < n; i++) {
            uint32_t cv = cov[i];
            if (!cv) continue;
            uint32_t sa = opaque_ca ? cv : ((ca * cv + 128) >> 8);
            if (!sa) continue;
            uint32_t d  = dst[i];
            uint32_t da = (d >> 24) & 0xFF;
            if (da == 0) {
                dst[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { dst[i] = 0; continue; }
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            dst[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
        }
        return;
    }

#if defined(UI_TEXT_HAVE_AVX2)
    const __m256i v_sr   = _mm256_set1_epi32((int)sr);
    const __m256i v_sg   = _mm256_set1_epi32((int)sg);
    const __m256i v_sb   = _mm256_set1_epi32((int)sb);
    const __m256i v_ca   = _mm256_set1_epi32((int)ca);
    const __m256i v_255  = _mm256_set1_epi32(255);
    const __m256i v_128  = _mm256_set1_epi32(128);
    const __m256i v_ff   = _mm256_set1_epi32(0xFF);
    const __m256i v_aa   = _mm256_set1_epi32((int)0xFF000000u);
    const __m256i v_zero = _mm256_setzero_si256();

    int i = 0;
    for (; i + 8 <= n; i += 8) {
        // widen 8 coverage bytes to 8 32-bit lanes
        __m128i cov8 = _mm_loadl_epi64((const __m128i*)(cov + i));
        __m256i cv   = _mm256_cvtepu8_epi32(cov8);

        // sa = (ca * cv + 128) >> 8, or cv when ca == 255
        __m256i sa;
        if (opaque_ca) {
            sa = cv;
        } else {
            sa = _mm256_srli_epi32(
                     _mm256_add_epi32(_mm256_mullo_epi16(cv, v_ca), v_128), 8);
        }
        __m256i inv = _mm256_sub_epi32(v_255, sa);

        // load 8 destination pixels and deinterleave the channels
        __m256i d  = _mm256_loadu_si256((const __m256i*)(dst + i));
        __m256i dr = _mm256_and_si256(_mm256_srli_epi32(d, 16), v_ff);
        __m256i dg = _mm256_and_si256(_mm256_srli_epi32(d,  8), v_ff);
        __m256i db = _mm256_and_si256(d,                         v_ff);

        // r = (sr*sa + dr*inv + 128) >> 8
        __m256i r = _mm256_srli_epi32(
            _mm256_add_epi32(
                _mm256_add_epi32(_mm256_mullo_epi16(v_sr, sa),
                                 _mm256_mullo_epi16(dr, inv)),
                v_128), 8);
        __m256i g = _mm256_srli_epi32(
            _mm256_add_epi32(
                _mm256_add_epi32(_mm256_mullo_epi16(v_sg, sa),
                                 _mm256_mullo_epi16(dg, inv)),
                v_128), 8);
        __m256i b = _mm256_srli_epi32(
            _mm256_add_epi32(
                _mm256_add_epi32(_mm256_mullo_epi16(v_sb, sa),
                                 _mm256_mullo_epi16(db, inv)),
                v_128), 8);

        // pack: 0xFF000000 | (r << 16) | (g << 8) | b
        __m256i out = _mm256_or_si256(v_aa,
            _mm256_or_si256(_mm256_slli_epi32(r, 16),
                            _mm256_or_si256(_mm256_slli_epi32(g, 8), b)));

        // where sa == 0, keep the original destination so repeated blits
        // of the same glyph do not drift by the >> 8 rounding error
        __m256i mask_skip = _mm256_cmpeq_epi32(sa, v_zero);
        out = _mm256_blendv_epi8(out, d, mask_skip);

        _mm256_storeu_si256((__m256i*)(dst + i), out);
    }
    for (; i < n; i++) {                                 // scalar tail
        uint32_t c = cov[i];
        if (!c) continue;
        uint32_t sa = opaque_ca ? c : ((ca * c + 128) >> 8);
        if (!sa) continue;
        uint32_t inv = 255 - sa;
        uint32_t d = dst[i];
        uint32_t dr = (d >> 16) & 0xFF;
        uint32_t dg = (d >>  8) & 0xFF;
        uint32_t db =  d        & 0xFF;
        uint32_t r  = (sr * sa + dr * inv + 128) >> 8;
        uint32_t g  = (sg * sa + dg * inv + 128) >> 8;
        uint32_t b  = (sb * sa + db * inv + 128) >> 8;
        dst[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }

#elif defined(UI_TEXT_HAVE_SSE2)
    const __m128i v_sr   = _mm_set1_epi32((int)sr);
    const __m128i v_sg   = _mm_set1_epi32((int)sg);
    const __m128i v_sb   = _mm_set1_epi32((int)sb);
    const __m128i v_ca   = _mm_set1_epi32((int)ca);
    const __m128i v_255  = _mm_set1_epi32(255);
    const __m128i v_128  = _mm_set1_epi32(128);
    const __m128i v_ff   = _mm_set1_epi32(0xFF);
    const __m128i v_aa   = _mm_set1_epi32((int)0xFF000000u);
    const __m128i v_zero = _mm_setzero_si128();

    int i = 0;
    for (; i + 4 <= n; i += 4) {
        // widen 4 coverage bytes to 4 32-bit lanes via two unpack passes
        int c32;
        memcpy(&c32, cov + i, sizeof(c32));              // safe unaligned 32-bit load
        __m128i bytes = _mm_cvtsi32_si128(c32);
        __m128i cv = _mm_unpacklo_epi8(bytes, v_zero);   // 4x u16 in low 64 bits
        cv         = _mm_unpacklo_epi16(cv, v_zero);     // 4x u32

        __m128i sa;
        if (opaque_ca) {
            sa = cv;
        } else {
            sa = _mm_srli_epi32(
                     _mm_add_epi32(_mm_mullo_epi16(cv, v_ca), v_128), 8);
        }
        __m128i inv = _mm_sub_epi32(v_255, sa);

        __m128i d  = _mm_loadu_si128((const __m128i*)(dst + i));
        __m128i dr = _mm_and_si128(_mm_srli_epi32(d, 16), v_ff);
        __m128i dg = _mm_and_si128(_mm_srli_epi32(d,  8), v_ff);
        __m128i db = _mm_and_si128(d,                       v_ff);

        __m128i r = _mm_srli_epi32(
            _mm_add_epi32(
                _mm_add_epi32(_mm_mullo_epi16(v_sr, sa),
                              _mm_mullo_epi16(dr, inv)),
                v_128), 8);
        __m128i g = _mm_srli_epi32(
            _mm_add_epi32(
                _mm_add_epi32(_mm_mullo_epi16(v_sg, sa),
                              _mm_mullo_epi16(dg, inv)),
                v_128), 8);
        __m128i b = _mm_srli_epi32(
            _mm_add_epi32(
                _mm_add_epi32(_mm_mullo_epi16(v_sb, sa),
                              _mm_mullo_epi16(db, inv)),
                v_128), 8);

        __m128i out = _mm_or_si128(v_aa,
            _mm_or_si128(_mm_slli_epi32(r, 16),
                         _mm_or_si128(_mm_slli_epi32(g, 8), b)));

        __m128i mask_skip = _mm_cmpeq_epi32(sa, v_zero);
        out = _mm_or_si128(_mm_and_si128(mask_skip, d),
                           _mm_andnot_si128(mask_skip, out));

        _mm_storeu_si128((__m128i*)(dst + i), out);
    }
    for (; i < n; i++) {                                 // scalar tail
        uint32_t c = cov[i];
        if (!c) continue;
        uint32_t sa = opaque_ca ? c : ((ca * c + 128) >> 8);
        if (!sa) continue;
        uint32_t inv = 255 - sa;
        uint32_t d = dst[i];
        uint32_t dr = (d >> 16) & 0xFF;
        uint32_t dg = (d >>  8) & 0xFF;
        uint32_t db =  d        & 0xFF;
        uint32_t r  = (sr * sa + dr * inv + 128) >> 8;
        uint32_t g  = (sg * sa + dg * inv + 128) >> 8;
        uint32_t b  = (sb * sa + db * inv + 128) >> 8;
        dst[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }

#else
    for (int i = 0; i < n; i++) {                        // pure scalar fallback
        uint32_t c = cov[i];
        if (!c) continue;
        uint32_t sa = opaque_ca ? c : ((ca * c + 128) >> 8);
        if (!sa) continue;
        uint32_t inv = 255 - sa;
        uint32_t d = dst[i];
        uint32_t dr = (d >> 16) & 0xFF;
        uint32_t dg = (d >>  8) & 0xFF;
        uint32_t db =  d        & 0xFF;
        uint32_t r  = (sr * sa + dr * inv + 128) >> 8;
        uint32_t g  = (sg * sa + dg * inv + 128) >> 8;
        uint32_t b  = (sb * sa + db * inv + 128) >> 8;
        dst[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
#endif
}

// scissor rect applied to glyph blits. an empty rect (x1 <= x0 or y1 <= y0) rejects every pixel
static int  g_clip_x0 = 0, g_clip_y0 = 0, g_clip_x1 = 0, g_clip_y1 = 0;
static bool g_clip_on = false;

// row pitch in pixels. defaults to fbw and is overridden by ui_painter_init_stride
// when the backing buffer is larger than the visible window.
static int  g_text_stride = 0;

// sets the scissor rect used by ui_text_draw
void ui_text_set_clip(int x0, int y0, int x1, int y1) {
    g_clip_x0 = x0; g_clip_y0 = y0;
    g_clip_x1 = x1; g_clip_y1 = y1;
    g_clip_on = true;
}

// sets the row pitch used by ui_text_draw
void ui_text_set_stride(int stride) {
    g_text_stride = stride > 0 ? stride : 0;             // 0 means "fall back to fbw"
}

// draws a utf-8 string into the framebuffer and returns the advance width in pixels
float ui_text_draw(UiFont* f, uint32_t* fb, int fbw, int fbh,
                   float x, float baseline, const char* s, int n,
                   float size, uint32_t color) {
    if (!f || !s) return 0;                              // nothing to draw

    // hoist the color channels out of the per-glyph loop: they are constant
    // across the whole call and the row blender takes them by value
    uint32_t ca = (color >> 24) & 0xFF;
    if (ca == 0) return 0;                               // fully transparent: nothing to draw
    uint32_t sr = (color >> 16) & 0xFF;
    uint32_t sg = (color >>  8) & 0xFF;
    uint32_t sb =  color        & 0xFF;

    uint16_t size_q = (uint16_t)(size * 2 + 0.5f);       // quantized size to share cache entries
    float pen = x;                                       // running pen position
    int i = 0;
    int base_y = (int)floorf(baseline + 0.5f);           // rounded baseline row
    int stride = g_text_stride > 0 ? g_text_stride : fbw; // row pitch in pixels
    while (i < n) {
        int adv; uint32_t cp = ui_font_decode_cp(s + i, n - i, &adv);  // decode one codepoint
        i += adv;

        // control characters (LF, CR, TAB, ...) have no glyph
        if (cp < 0x20 || cp == 0x7F) {
            uint16_t sp_gid = ui_font_glyph_index_cached(f, ' ');
            CacheEntry* sp = ui_font_glyph_get(f, sp_gid, size_q);
            pen += ui_font_cache_advance(sp);
            continue;
        }

        uint16_t gid = ui_font_glyph_index_cached(f, cp);  // map to a glyph (O(1) for BMP after first touch)
        CacheEntry* e = ui_font_glyph_get(f, gid, size_q); // fetch or rasterize the bitmap
        int ew = ui_font_cache_w(e);
        uint16_t* rx0 = ui_font_cache_row_x0(e);
        if (ew > 0 && rx0) {                             // bitmap present
            uint16_t* rx1 = ui_font_cache_row_x1(e);
            uint8_t*  cov = ui_font_cache_cov(e);
            int eh = ui_font_cache_h(e);
            int eox = ui_font_cache_off_x(e);
            int eoy = ui_font_cache_off_y(e);
            int px = (int)floorf(pen + 0.5f) + eox;      // left edge in framebuffer space
            int py = base_y - eoy;                       // baseline row in framebuffer space
            for (int yy = 0; yy < eh; yy++) {            // coverage bitmap is stored top-down
                int cx0 = rx0[yy];                       // tight nonzero span of this row
                int cx1 = rx1[yy];
                if (cx0 >= cx1) continue;                // fully empty row: skip whole

                int ty = py - yy;                        // font y grows up, framebuffer y grows down
                if (ty < 0 || ty >= fbh) continue;       // row outside the framebuffer
                if (g_clip_on && (ty < g_clip_y0 || ty >= g_clip_y1))
                    continue;                            // row outside the scissor

                // clip the tight span to the framebuffer and the scissor
                int x0 = cx0, x1 = cx1;
                if (px + x0 < 0)   x0 = -px;             // clip to the left framebuffer edge
                if (px + x1 > fbw) x1 = fbw - px;        // clip to the right framebuffer edge
                if (g_clip_on) {
                    if (px + x0 < g_clip_x0) x0 = g_clip_x0 - px;
                    if (px + x1 > g_clip_x1) x1 = g_clip_x1 - px;
                }
                if (x0 >= x1) continue;                  // row fully clipped

                const uint8_t* cov_row = cov + (size_t)yy * ew + x0;
                uint32_t* dst = fb + (size_t)ty * stride + px + x0;
                blend_glyph_row(dst, cov_row, x1 - x0, sr, sg, sb, ca);
            }
        }
        pen += ui_font_cache_advance(e);                 // advance the pen
    }
    return pen - x;                                      // total advance width used
}
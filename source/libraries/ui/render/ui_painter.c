// source/libraries/ui/render/ui_painter.c
// SIMD-accelerated pixel painter for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_render.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// simd dispatch
#if defined(__AVX2__) || (defined(_M_X64) && defined(__AVX2__))
#  include <immintrin.h>
#  define UI_HAVE_AVX2 1
#elif defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#  include <emmintrin.h>
#  define UI_HAVE_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#  include <arm_neon.h>
#  define UI_HAVE_NEON 1
#endif

// active framebuffer and its dimensions. g_stride is the row pitch in
// pixels: it can exceed g_w when the backing buffer is larger than the
// window, which lets us avoid reallocating on every resize.
static uint32_t* g_pixels; static int g_w, g_h, g_stride;

// true when the target surface carries per-pixel alpha (notification popups)
static bool g_alpha_canvas;

// mirrored from g_alpha_canvas so ui_text.c can pick its glyph-blit path
bool g_text_dst_alpha_used = false;

// nested scissor rectangles
static int clip_stack[16][4]; static int clip_depth;

// top-of-stack clip cached as (x0, y0, x1, y1)
static int g_clip_x0, g_clip_y0, g_clip_x1, g_clip_y1;

// refresh the cached clip from the current top of the stack
static inline void clip_refresh(void) {
    if (clip_depth > 0) {
        g_clip_x0 = clip_stack[clip_depth-1][0];
        g_clip_y0 = clip_stack[clip_depth-1][1];
        g_clip_x1 = g_clip_x0 + clip_stack[clip_depth-1][2];
        g_clip_y1 = g_clip_y0 + clip_stack[clip_depth-1][3];
    } else {
        g_clip_x0 = 0; g_clip_y0 = 0;
        g_clip_x1 = g_w; g_clip_y1 = g_h;
    }
}

// binds the painter to a framebuffer and clears the clip stack
void ui_painter_init_stride(uint32_t* p, int w, int h, int stride) {
    g_pixels = p; g_w = w; g_h = h; g_stride = stride; clip_depth = 0;
    g_alpha_canvas = false;
    g_text_dst_alpha_used = false;
    clip_refresh();
    ui_text_set_clip(0, 0, w, h);                        // whole surface: nothing clipped
    ui_text_set_stride(stride);
}

// binds the painter to a framebuffer and clears the clip stack
void ui_painter_init(uint32_t* p, int w, int h) {
    ui_painter_init_stride(p, w, h, w);                  // stride matches width: tight buffer
}

// binds the painter to an alpha framebuffer with an explicit row stride
void ui_painter_init_alpha_stride(uint32_t* p, int w, int h, int stride) {
    g_pixels = p; g_w = w; g_h = h; g_stride = stride; clip_depth = 0;
    g_alpha_canvas = true;
    g_text_dst_alpha_used = true;
    clip_refresh();
    ui_text_set_clip(0, 0, w, h);
    ui_text_set_stride(stride);
}

// binds the painter to a framebuffer that must preserve per-pixel alpha
void ui_painter_init_alpha(uint32_t* p, int w, int h) {
    ui_painter_init_alpha_stride(p, w, h, w);            // stride matches width: tight buffer
}

// pushes a scissor rect, intersected with the current top of the stack
void ui_painter_push_clip(int x, int y, int w, int h) {
    if (clip_depth < 16) {                              // hard cap to bound recursion
        int cx0 = 0, cy0 = 0, cx1 = g_w, cy1 = g_h;     // start from the whole surface
        if (clip_depth > 0) {                           // intersect with current clip
            cx0 = clip_stack[clip_depth-1][0];
            cy0 = clip_stack[clip_depth-1][1];
            cx1 = cx0 + clip_stack[clip_depth-1][2];
            cy1 = cy0 + clip_stack[clip_depth-1][3];
        }
        int nx0 = x > cx0 ? x : cx0, ny0 = y > cy0 ? y : cy0;  // max of the left/top edges
        int nx1 = x+w < cx1 ? x+w : cx1, ny1 = y+h < cy1 ? y+h : cy1;  // min of the right/bottom edges
        if (nx1 < nx0) nx1 = nx0;                       // guard against inverted rects
        if (ny1 < ny0) ny1 = ny0;
        clip_stack[clip_depth][0] = nx0; clip_stack[clip_depth][1] = ny0;
        clip_stack[clip_depth][2] = nx1 - nx0; clip_stack[clip_depth][3] = ny1 - ny0;
        clip_depth++;
        clip_refresh();
        ui_text_set_clip(nx0, ny0, nx1, ny1);
    }
}

// pops the most recent scissor rect
void ui_painter_pop_clip(void) {
    if (clip_depth > 0) clip_depth--;
    clip_refresh();
    ui_text_set_clip(g_clip_x0, g_clip_y0, g_clip_x1, g_clip_y1);
}

// clears the whole clip stack
void ui_painter_reset_clip(void) {
    clip_depth = 0;
    clip_refresh();
    ui_text_set_clip(0, 0, g_w, g_h);
}

// blends one run of coverage bytes onto the framebuffer with a solid color
static void blend_cov_row(uint32_t* dst, const uint8_t* cov, int n,
                          uint32_t ca, uint32_t sr, uint32_t sg, uint32_t sb) {
    if (ca == 0 || n <= 0) return;
    const bool opaque_ca = (ca == 255);

    int i = 0;

    if (!g_alpha_canvas) {
#if defined(UI_HAVE_AVX2)
    const __m256i v_sr   = _mm256_set1_epi32((int)sr);
    const __m256i v_sg   = _mm256_set1_epi32((int)sg);
    const __m256i v_sb   = _mm256_set1_epi32((int)sb);
    const __m256i v_ca   = _mm256_set1_epi32((int)ca);
    const __m256i v_255  = _mm256_set1_epi32(255);
    const __m256i v_128  = _mm256_set1_epi32(128);
    const __m256i v_ff   = _mm256_set1_epi32(0xFF);
    const __m256i v_aa   = _mm256_set1_epi32((int)0xFF000000u);
    const __m256i v_zero = _mm256_setzero_si256();

    for (; i + 8 <= n; i += 8) {
        __m128i cov8 = _mm_loadl_epi64((const __m128i*)(cov + i));
        __m256i cv   = _mm256_cvtepu8_epi32(cov8);

        __m256i sa;
        if (opaque_ca) {
            sa = cv;
        } else {
            sa = _mm256_srli_epi32(
                     _mm256_add_epi32(_mm256_mullo_epi16(cv, v_ca), v_128), 8);
        }
        __m256i inv = _mm256_sub_epi32(v_255, sa);

        __m256i d  = _mm256_loadu_si256((const __m256i*)(dst + i));
        __m256i dr = _mm256_and_si256(_mm256_srli_epi32(d, 16), v_ff);
        __m256i dg = _mm256_and_si256(_mm256_srli_epi32(d,  8), v_ff);
        __m256i db = _mm256_and_si256(d,                         v_ff);

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

        __m256i out = _mm256_or_si256(v_aa,
            _mm256_or_si256(_mm256_slli_epi32(r, 16),
                            _mm256_or_si256(_mm256_slli_epi32(g, 8), b)));

        __m256i skip = _mm256_cmpeq_epi32(sa, v_zero);
        out = _mm256_blendv_epi8(out, d, skip);

        _mm256_storeu_si256((__m256i*)(dst + i), out);
    }
#endif

#if defined(UI_HAVE_AVX2) || defined(UI_HAVE_SSE2)
    const __m128i v_sr4  = _mm_set1_epi32((int)sr);
    const __m128i v_sg4  = _mm_set1_epi32((int)sg);
    const __m128i v_sb4  = _mm_set1_epi32((int)sb);
    const __m128i v_ca4  = _mm_set1_epi32((int)ca);
    const __m128i v_2554 = _mm_set1_epi32(255);
    const __m128i v_1284 = _mm_set1_epi32(128);
    const __m128i v_ff4  = _mm_set1_epi32(0xFF);
    const __m128i v_aa4  = _mm_set1_epi32((int)0xFF000000u);
    const __m128i v_z4   = _mm_setzero_si128();

    for (; i + 4 <= n; i += 4) {
        int c32;
        memcpy(&c32, cov + i, 4);                        // unaligned 4-byte load
        __m128i bytes = _mm_cvtsi32_si128(c32);
        __m128i cv = _mm_unpacklo_epi8(bytes, v_z4);     // 4x u16 in the low 64 bits
        cv         = _mm_unpacklo_epi16(cv, v_z4);       // 4x u32

        __m128i sa;
        if (opaque_ca) {
            sa = cv;
        } else {
            sa = _mm_srli_epi32(
                     _mm_add_epi32(_mm_mullo_epi16(cv, v_ca4), v_1284), 8);
        }
        __m128i inv = _mm_sub_epi32(v_2554, sa);

        __m128i d  = _mm_loadu_si128((const __m128i*)(dst + i));
        __m128i dr = _mm_and_si128(_mm_srli_epi32(d, 16), v_ff4);
        __m128i dg = _mm_and_si128(_mm_srli_epi32(d,  8), v_ff4);
        __m128i db = _mm_and_si128(d,                      v_ff4);

        __m128i r = _mm_srli_epi32(
            _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(v_sr4, sa),
                                        _mm_mullo_epi16(dr, inv)), v_1284), 8);
        __m128i g = _mm_srli_epi32(
            _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(v_sg4, sa),
                                        _mm_mullo_epi16(dg, inv)), v_1284), 8);
        __m128i b = _mm_srli_epi32(
            _mm_add_epi32(_mm_add_epi32(_mm_mullo_epi16(v_sb4, sa),
                                        _mm_mullo_epi16(db, inv)), v_1284), 8);

        __m128i out = _mm_or_si128(v_aa4,
            _mm_or_si128(_mm_slli_epi32(r, 16),
                         _mm_or_si128(_mm_slli_epi32(g, 8), b)));

        __m128i skip = _mm_cmpeq_epi32(sa, v_z4);
        out = _mm_or_si128(_mm_and_si128(skip, d),
                           _mm_andnot_si128(skip, out));

        _mm_storeu_si128((__m128i*)(dst + i), out);
    }
#endif
    }

    for (; i < n; i++) {                                 // scalar tail
        uint32_t cv = cov[i];
        if (!cv) continue;
        uint32_t sa = opaque_ca ? cv : ((ca * cv + 128) >> 8);
        if (!sa) continue;
        uint32_t d  = dst[i];

        if (g_alpha_canvas) {
            uint32_t da = (d >> 24) & 0xFF;
            uint32_t dr = (d >> 16) & 0xFF;
            uint32_t dg = (d >>  8) & 0xFF;
            uint32_t db =  d        & 0xFF;
            if (da == 0) {
                dst[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { dst[i] = 0; continue; }
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            dst[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
            continue;
        }

        uint32_t inv = 255 - sa;
        uint32_t dr = (d >> 16) & 0xFF;
        uint32_t dg = (d >>  8) & 0xFF;
        uint32_t db =  d        & 0xFF;
        uint32_t r  = (sr * sa + dr * inv + 128) >> 8;
        uint32_t g  = (sg * sa + dg * inv + 128) >> 8;
        uint32_t b  = (sb * sa + db * inv + 128) >> 8;
        dst[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}

// simd row blenders
#ifdef UI_HAVE_AVX2
// processes 8 pixels at a time
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    if (!sa) return;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    if (g_alpha_canvas) {
        for (int i = 0; i < n; i++) {
            uint32_t d  = row[i];
            uint32_t da = (d >> 24) & 0xFF;
            if (da == 0) {
                row[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { row[i] = 0; continue; }
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            row[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
        }
        return;
    }

    // destination is weighted by inv, source is weighted by sa
    __m256i k_inv = _mm256_set1_epi32((int)inv);
    __m256i k_sr  = _mm256_set1_epi32((int)(sr * sa + 128));
    __m256i k_sg  = _mm256_set1_epi32((int)(sg * sa + 128));
    __m256i k_sb  = _mm256_set1_epi32((int)(sb * sa + 128));
    __m256i kFF   = _mm256_set1_epi32(0xFF);
    __m256i kAA   = _mm256_set1_epi32((int)0xFF000000u);

    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256i d  = _mm256_loadu_si256((const __m256i*)(row + i));
        __m256i dr = _mm256_and_si256(_mm256_srli_epi32(d, 16), kFF);
        __m256i dg = _mm256_and_si256(_mm256_srli_epi32(d, 8),  kFF);
        __m256i db = _mm256_and_si256(d,                        kFF);

        // dst * inv  +  (src * sa + 128)  >>  8
        __m256i r = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi16(dr, k_inv), k_sr), 8);
        __m256i g = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi16(dg, k_inv), k_sg), 8);
        __m256i b = _mm256_srli_epi32(_mm256_add_epi32(_mm256_mullo_epi16(db, k_inv), k_sb), 8);

        __m256i out = _mm256_or_si256(
            kAA,
            _mm256_or_si256(
                _mm256_slli_epi32(r, 16),
                _mm256_or_si256(_mm256_slli_epi32(g, 8), b)));
        _mm256_storeu_si256((__m256i*)(row + i), out);
    }
    for (; i < n; i++) {                                    // scalar tail
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (sr*sa + dr*inv + 128) >> 8;
        uint32_t g = (sg*sa + dg*inv + 128) >> 8;
        uint32_t b = (sb*sa + db*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#elif defined(UI_HAVE_SSE2)
// processes 4 pixels at a time. uses epi16 multiplies on epi32 lanes
// holding one channel each, with a +128>>8 rounding approximation.
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    if (!sa) return;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    if (g_alpha_canvas) {
        for (int i = 0; i < n; i++) {
            uint32_t d  = row[i];
            uint32_t da = (d >> 24) & 0xFF;
            if (da == 0) {
                row[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { row[i] = 0; continue; }
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            row[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
        }
        return;
    }

    __m128i k_inv = _mm_set1_epi32((int)inv);
    __m128i k_sr  = _mm_set1_epi32((int)(sr * sa + 128));
    __m128i k_sg  = _mm_set1_epi32((int)(sg * sa + 128));
    __m128i k_sb  = _mm_set1_epi32((int)(sb * sa + 128));
    __m128i kFF   = _mm_set1_epi32(0xFF);
    __m128i kAA   = _mm_set1_epi32((int)0xFF000000u);

    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i d  = _mm_loadu_si128((const __m128i*)(row + i));
        __m128i dr = _mm_and_si128(_mm_srli_epi32(d, 16), kFF);
        __m128i dg = _mm_and_si128(_mm_srli_epi32(d, 8),  kFF);
        __m128i db = _mm_and_si128(d,                     kFF);

        // dst * inv + (src * sa + 128) >> 8
        __m128i r = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(dr, k_inv), k_sr), 8);
        __m128i g = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(dg, k_inv), k_sg), 8);
        __m128i b = _mm_srli_epi32(_mm_add_epi32(_mm_mullo_epi16(db, k_inv), k_sb), 8);

        __m128i out = _mm_or_si128(
            kAA,
            _mm_or_si128(
                _mm_slli_epi32(r, 16),
                _mm_or_si128(_mm_slli_epi32(g, 8), b)));
        _mm_storeu_si128((__m128i*)(row + i), out);
    }
    for (; i < n; i++) {                                    // scalar tail
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (sr*sa + dr*inv + 128) >> 8;
        uint32_t g = (sg*sa + dg*inv + 128) >> 8;
        uint32_t b = (sb*sa + db*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#elif defined(UI_HAVE_NEON)
// processes 8 pixels at a time via vld4/vst4 deinterleave
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    if (!sa) return;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    if (g_alpha_canvas) {
        for (int i = 0; i < n; i++) {
            uint32_t d  = row[i];
            uint32_t da = (d >> 24) & 0xFF;
            if (da == 0) {
                row[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { row[i] = 0; continue; }
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            row[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
        }
        return;
    }

    // vmlaq_u16 computes dst * k + acc
    uint16x8_t k_inv = vdupq_n_u16((uint16_t)inv);
    uint16x8_t k_sr  = vdupq_n_u16((uint16_t)(sr * sa + 128));
    uint16x8_t k_sg  = vdupq_n_u16((uint16_t)(sg * sa + 128));
    uint16x8_t k_sb  = vdupq_n_u16((uint16_t)(sb * sa + 128));
    uint8x8_t  kA8   = vdup_n_u8(0xFF);

    int i = 0;
    for (; i + 8 <= n; i += 8) {
        uint8x8x4_t d = vld4_u8((const uint8_t*)(row + i));   // [B][G][R][A]
        // dst * inv + src*sa+128  (was: dst * sa + src*inv+128)
        uint16x8_t b16 = vmlaq_u16(k_sb, vmovl_u8(d.val[0]), k_inv);
        uint16x8_t g16 = vmlaq_u16(k_sg, vmovl_u8(d.val[1]), k_inv);
        uint16x8_t r16 = vmlaq_u16(k_sr, vmovl_u8(d.val[2]), k_inv);
        uint8x8x4_t out;
        out.val[0] = vshrn_n_u16(b16, 8);
        out.val[1] = vshrn_n_u16(g16, 8);
        out.val[2] = vshrn_n_u16(r16, 8);
        out.val[3] = kA8;
        vst4_u8((uint8_t*)(row + i), out);
    }
    for (; i < n; i++) {                                    // scalar tail
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (sr*sa + dr*inv + 128) >> 8;
        uint32_t g = (sg*sa + dg*inv + 128) >> 8;
        uint32_t b = (sb*sa + db*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#else
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    if (!sa) return;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

    if (g_alpha_canvas) {
        for (int i = 0; i < n; i++) {
            uint32_t d  = row[i];
            uint32_t da = (d >> 24) & 0xFF;
            if (da == 0) {
                row[i] = (sa << 24) | (sr << 16) | (sg << 8) | sb;
                continue;
            }
            uint32_t w_da  = (da * (255 - sa) + 127) / 255;
            uint32_t out_a = sa + w_da;
            if (out_a > 255) out_a = 255;
            if (!out_a) { row[i] = 0; continue; }
            uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
            uint32_t r = (sr * sa + dr * w_da + out_a / 2) / out_a;
            uint32_t g = (sg * sa + dg * w_da + out_a / 2) / out_a;
            uint32_t b = (sb * sa + db * w_da + out_a / 2) / out_a;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            row[i] = (out_a << 24) | (r << 16) | (g << 8) | b;
        }
        return;
    }

    for (int i = 0; i < n; i++) {
        uint32_t d  = row[i];
        uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
        uint32_t r = (sr*sa + dr*inv + 128) >> 8;
        uint32_t g = (sg*sa + dg*inv + 128) >> 8;
        uint32_t b = (sb*sa + db*inv + 128) >> 8;
        row[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
    }
}
#endif

// simd row fillers for the opaque path: one store per lane, no blending
#ifdef UI_HAVE_AVX2
static inline void fill_row_simd(uint32_t* row, int n, uint32_t color) {
    __m256i v = _mm256_set1_epi32((int)color);
    int i = 0;
    for (; i + 8 <= n; i += 8) _mm256_storeu_si256((__m256i*)(row + i), v);
    for (; i < n; i++) row[i] = color;
}
#elif defined(UI_HAVE_SSE2)
static inline void fill_row_simd(uint32_t* row, int n, uint32_t color) {
    __m128i v = _mm_set1_epi32((int)color);
    int i = 0;
    for (; i + 4 <= n; i += 4) _mm_storeu_si128((__m128i*)(row + i), v);
    for (; i < n; i++) row[i] = color;
}
#elif defined(UI_HAVE_NEON)
static inline void fill_row_simd(uint32_t* row, int n, uint32_t color) {
    uint32x4_t v = vdupq_n_u32(color);
    int i = 0;
    for (; i + 4 <= n; i += 4) vst1q_u32(row + i, v);
    for (; i < n; i++) row[i] = color;
}
#else
static inline void fill_row_simd(uint32_t* row, int n, uint32_t color) {
    for (int i = 0; i < n; i++) row[i] = color;
}
#endif

// non-temporal variant, used only for full-window clears
#ifdef UI_HAVE_AVX2
static inline void fill_row_stream_simd(uint32_t* row, int n, uint32_t color) {
    __m256i v = _mm256_set1_epi32((int)color);
    int i = 0;
    while (i < n && ((uintptr_t)(row + i) & 31)) row[i++] = color;   // align to 32B
    for (; i + 8 <= n; i += 8) _mm256_stream_si256((__m256i*)(row + i), v);
    for (; i < n; i++) row[i] = color;                              // scalar tail
    // no sfence here: the caller emits one after the whole surface is written
}
#elif defined(UI_HAVE_SSE2)
static inline void fill_row_stream_simd(uint32_t* row, int n, uint32_t color) {
    __m128i v = _mm_set1_epi32((int)color);
    int i = 0;
    while (i < n && ((uintptr_t)(row + i) & 15)) row[i++] = color;   // align to 16B
    for (; i + 4 <= n; i += 4) _mm_stream_si128((__m128i*)(row + i), v);
    for (; i < n; i++) row[i] = color;
    // no sfence here: the caller emits one after the whole surface is written
}
#elif defined(UI_HAVE_NEON)
static inline void fill_row_stream_simd(uint32_t* row, int n, uint32_t color) {
    // neon has no non-temporal stores
    uint32x4_t v = vdupq_n_u32(color);
    int i = 0;
    for (; i + 4 <= n; i += 4) vst1q_u32(row + i, v);
    for (; i < n; i++) row[i] = color;
}
#else
static inline void fill_row_stream_simd(uint32_t* row, int n, uint32_t color) {
    for (int i = 0; i < n; i++) row[i] = color;
}
#endif

// clears the whole bound framebuffer to an opaque color using streaming stores
void ui_painter_clear(uint32_t c) {
    if (g_alpha_canvas) {
        for (int y = 0; y < g_h; y++) {
            uint32_t* row = g_pixels + (size_t)y * g_stride;
            for (int x = 0; x < g_w; x++) row[x] = c;
        }
        return;
    }
    uint32_t sa = (c >> 24) & 0xFF;
    if (sa != 255) return;                                          // opaque only
    uint32_t color = 0xFF000000u | (c & 0x00FFFFFFu);
    for (int y = 0; y < g_h; y++) {
        uint32_t* row = g_pixels + (size_t)y * g_stride;
        fill_row_stream_simd(row, g_w, color);
    }
#if defined(UI_HAVE_AVX2) || defined(UI_HAVE_SSE2)
    _mm_sfence();   // one barrier for the whole surface, not one per row
#endif
}

// fills an axis-aligned rectangle with the given color
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle

    // clamp to the cached clip; the clip already lives inside the surface
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < g_clip_x0) x0 = g_clip_x0;
    if (y0 < g_clip_y0) y0 = g_clip_y0;
    if (x1 > g_clip_x1) x1 = g_clip_x1;
    if (y1 > g_clip_y1) y1 = g_clip_y1;
    if (x1 <= x0 || y1 <= y0) return;                   // fully clipped

    uint32_t sa = (c >> 24) & 0xFF;
    if (!sa) return;                                    // fully transparent

    int rw = x1 - x0;

    if (sa == 255) {                                    // opaque fast path: row-wise store
        uint32_t color = g_alpha_canvas
                       ? c
                       : (0xFF000000u | (c & 0x00FFFFFFu));
        for (int yy = y0; yy < y1; yy++) {
            uint32_t* row = g_pixels + (size_t)yy * g_stride + x0;
            fill_row_simd(row, rw, color);
        }
        return;
    }

    // alpha path: one SIMD (or scalar) sweep per row
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = g_pixels + (size_t)yy * g_stride + x0;
        blend_row_simd(row, rw, c);
    }
}

// corner coverage atlas
typedef struct {
    int      r;              // radius this entry was built for, 0 = empty
    uint8_t* fill_cov;       // r*r coverage, top-left corner, filled arc
    uint8_t* stroke_cov;     // r*r coverage, top-left corner, 1px stroke band
} CornerEntry;

#define CORNER_CACHE_MAX 16
static CornerEntry g_corners[CORNER_CACHE_MAX];
static int         g_corners_count;

// looks up the cache entry for r, building it on first use
static const CornerEntry* corner_get(int r) {
    for (int i = 0; i < g_corners_count; i++)
        if (g_corners[i].r == r) return &g_corners[i];
    if (g_corners_count >= CORNER_CACHE_MAX) return NULL;

    CornerEntry* e = &g_corners[g_corners_count++];
    e->r = r;
    e->fill_cov   = (uint8_t*)malloc((size_t)r * (size_t)r);
    e->stroke_cov = (uint8_t*)malloc((size_t)r * (size_t)r);

    const int SS = 4;
    float rr  = (float)r;
    float rr2 = rr * rr;
    float ri  = rr - 1.0f;
    float ri2 = ri > 0 ? ri * ri : 0.0f;

    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            int inside_fill = 0, inside_stroke = 0;
            for (int sy = 0; sy < SS; sy++) {
                for (int sx = 0; sx < SS; sx++) {
                    float fx = (float)xx + (sx + 0.5f) / SS;
                    float fy = (float)yy + (sy + 0.5f) / SS;
                    float ex = rr - fx;                 // distance from corner center (r, r)
                    float ey = rr - fy;
                    float d2 = ex*ex + ey*ey;
                    if (d2 <= rr2) inside_fill++;
                    if (d2 <= rr2 && d2 >= ri2) inside_stroke++;
                }
            }
            e->fill_cov  [(size_t)yy*r + xx] = (uint8_t)(inside_fill   * 255 / (SS * SS));
            e->stroke_cov[(size_t)yy*r + xx] = (uint8_t)(inside_stroke * 255 / (SS * SS));
        }
    }
    return e;
}

// releases the corner cache; called from ui_painter_shutdown
void ui_painter_shutdown(void) {
    for (int i = 0; i < g_corners_count; i++) {
        free(g_corners[i].fill_cov);
        free(g_corners[i].stroke_cov);
    }
    g_corners_count = 0;
}

// blends an r*r corner coverage bitmap into the framebuffer at (ox, oy).
static void blit_corner_cov(const uint8_t* cov, int r, int ox, int oy,
                            int flip_x, int flip_y, uint32_t c) {
    if (r <= 0) return;

    // clip the corner rect to the active scissor once
    int x0 = 0, y0 = 0, x1 = r, y1 = r;
    if (ox + x0 < g_clip_x0) x0 = g_clip_x0 - ox;
    if (oy + y0 < g_clip_y0) y0 = g_clip_y0 - oy;
    if (ox + x1 > g_clip_x1) x1 = g_clip_x1 - ox;
    if (oy + y1 > g_clip_y1) y1 = g_clip_y1 - oy;
    if (x0 >= x1 || y0 >= y1) return;                    // fully clipped

    uint32_t ca = (c >> 24) & 0xFF;
    if (!ca) return;                                     // fully transparent color
    uint32_t sr = (c >> 16) & 0xFF;
    uint32_t sg = (c >>  8) & 0xFF;
    uint32_t sb =  c        & 0xFF;

    uint8_t tmp[128];                                    // scratch for h-flipped rows
    const bool can_use_tmp = (r <= (int)sizeof(tmp));

    for (int yy = y0; yy < y1; yy++) {
        int sy = flip_y ? (r - 1 - yy) : yy;
        const uint8_t* src_row = cov + (size_t)sy * r;

        const uint8_t* row;
        if (!flip_x) {
            row = src_row;                               // contiguous already
        } else if (can_use_tmp) {
            for (int k = 0; k < r; k++) tmp[k] = src_row[r - 1 - k];
            row = tmp;
        } else {
            // oversized radius: scalar per-pixel fallback
            for (int xx = x0; xx < x1; xx++) {
                uint32_t cv = src_row[r - 1 - xx];
                if (!cv) continue;
                uint32_t sa = (ca * cv + 128) >> 8;
                if (!sa) continue;
                uint32_t inv = 255 - sa;
                uint32_t* p = g_pixels + (size_t)(oy + yy) * g_stride + (ox + xx);
                uint32_t d  = *p;
                uint32_t dr = (d >> 16) & 0xFF;
                uint32_t dg = (d >>  8) & 0xFF;
                uint32_t db =  d        & 0xFF;
                uint32_t rr = (sr * sa + dr * inv + 128) >> 8;
                uint32_t gg = (sg * sa + dg * inv + 128) >> 8;
                uint32_t bb = (sb * sa + db * inv + 128) >> 8;
                *p = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
            }
            continue;
        }

        const uint8_t* run = row + x0;
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_stride + (ox + x0);
        blend_cov_row(dst, run, x1 - x0, ca, sr, sg, sb);
    }
}

// draws the four rounded corners of a rectangle from the fill bitmap
static void fill_corners_atlas(int x, int y, int w, int h, int r, uint32_t c) {
    if (r <= 0) return;
    const CornerEntry* e = corner_get(r);
    if (!e) return;

    uint8_t tmp[64];
    if (r > (int)sizeof(tmp)) {
        // oversized radius (never hit with the current theme)
        blit_corner_cov(e->fill_cov, r, x,       y,       0, 0, c);
        blit_corner_cov(e->fill_cov, r, x + w-r, y,       1, 0, c);
        blit_corner_cov(e->fill_cov, r, x,       y + h-r, 0, 1, c);
        blit_corner_cov(e->fill_cov, r, x + w-r, y + h-r, 1, 1, c);
        return;
    }

    uint32_t ca = (c >> 24) & 0xFF;
    if (!ca) return;                                     // fully transparent color
    uint32_t sr = (c >> 16) & 0xFF;
    uint32_t sg = (c >>  8) & 0xFF;
    uint32_t sb =  c        & 0xFF;

    // early-out: neither the top block nor the bottom block intersects the scissor vertically
    bool top_visible = (y < g_clip_y1) && (y + r > g_clip_y0);
    bool bot_visible = (y + h - r < g_clip_y1) && (y + h > g_clip_y0);
    if (!top_visible && !bot_visible) return;

    // horizontal clip for the left column [x, x+r) and right column [x+w-r, x+w)
    int xl0 = 0, xl1 = r;
    if (x + xl0 < g_clip_x0) xl0 = g_clip_x0 - x;
    if (x + xl1 > g_clip_x1) xl1 = g_clip_x1 - x;
    int xr0 = 0, xr1 = r;
    if (x + w - r + xr0 < g_clip_x0) xr0 = g_clip_x0 - (x + w - r);
    if (x + w - r + xr1 > g_clip_x1) xr1 = g_clip_x1 - (x + w - r);

    int n_left  = xl1 > xl0 ? xl1 - xl0 : 0;
    int n_right = xr1 > xr0 ? xr1 - xr0 : 0;
    if (n_left == 0 && n_right == 0) return;             // fully clipped horizontally

    // one pass over the corner rows
    for (int i = 0; i < r; i++) {
        const uint8_t* top_cov = e->fill_cov + (size_t)i * r;
        const uint8_t* bot_cov = e->fill_cov + (size_t)(r - 1 - i) * r;

        int ty = y + i;
        int by = y + h - r + i;

        if (n_left > 0) {
            if (ty >= g_clip_y0 && ty < g_clip_y1)       // top-left
                blend_cov_row(g_pixels + (size_t)ty * g_stride + x + xl0,
                              top_cov + xl0, n_left, ca, sr, sg, sb);
            if (by >= g_clip_y0 && by < g_clip_y1)       // bottom-left
                blend_cov_row(g_pixels + (size_t)by * g_stride + x + xl0,
                              bot_cov + xl0, n_left, ca, sr, sg, sb);
        }
        if (n_right > 0) {
            for (int k = 0; k < r; k++) tmp[k] = top_cov[r - 1 - k];
            if (ty >= g_clip_y0 && ty < g_clip_y1)       // top-right
                blend_cov_row(g_pixels + (size_t)ty * g_stride + x + w - r + xr0,
                              tmp + xr0, n_right, ca, sr, sg, sb);
            for (int k = 0; k < r; k++) tmp[k] = bot_cov[r - 1 - k];
            if (by >= g_clip_y0 && by < g_clip_y1)       // bottom-right
                blend_cov_row(g_pixels + (size_t)by * g_stride + x + w - r + xr0,
                              tmp + xr0, n_right, ca, sr, sg, sb);
        }
    }
}

// draws the four rounded corners of a rectangle from the stroke bitmap
static void stroke_corners_atlas(int x, int y, int w, int h, int r, uint32_t c) {
    if (r <= 0) return;
    const CornerEntry* e = corner_get(r);
    if (!e) return;
    blit_corner_cov(e->stroke_cov, r, x,       y,       0, 0, c); // top-left
    blit_corner_cov(e->stroke_cov, r, x + w-r, y,       1, 0, c); // top-right
    blit_corner_cov(e->stroke_cov, r, x,       y + h-r, 0, 1, c); // bottom-left
    blit_corner_cov(e->stroke_cov, r, x + w-r, y + h-r, 1, 1, c); // bottom-right
}

// fills a rounded rectangle with antialiased corners
void ui_painter_fill_round(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle
    if (r > w/2) r = w/2;                               // clamp radius to half the width
    if (r > h/2) r = h/2;                               // clamp radius to half the height
    if (r <= 0) { ui_painter_fill_rect(x, y, w, h, c); return; }  // square corners: delegate
    ui_painter_fill_rect(x+r,   y,     w-2*r, h,     c);          // horizontal middle band
    ui_painter_fill_rect(x,     y+r,   r,     h-2*r, c);          // left middle
    ui_painter_fill_rect(x+w-r, y+r,   r,     h-2*r, c);          // right middle
    fill_corners_atlas(x, y, w, h, r, c);                         // all four rounded corners
}

// strokes a 1px axis-aligned rectangle outline
void ui_painter_stroke_rect(int x, int y, int w, int h, uint32_t c) {
    ui_painter_fill_rect(x, y, w, 1, c);                // top edge
    ui_painter_fill_rect(x, y+h-1, w, 1, c);            // bottom edge
    ui_painter_fill_rect(x, y, 1, h, c);                // left edge
    ui_painter_fill_rect(x+w-1, y, 1, h, c);            // right edge
}

// strokes a 1px rounded rectangle outline with antialiased corners
void ui_painter_stroke_round(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 0 || h <= 0) return;                       // degenerate rectangle
    if (r > w/2) r = w/2;                               // clamp radius to half the width
    if (r > h/2) r = h/2;                               // clamp radius to half the height
    if (r <= 0) { ui_painter_stroke_rect(x, y, w, h, c); return; }  // square corners: delegate
    ui_painter_fill_rect(x+r,   y,       w-2*r, 1,     c);          // top edge
    ui_painter_fill_rect(x+r,   y+h-1,   w-2*r, 1,     c);          // bottom edge
    ui_painter_fill_rect(x,     y+r,     1,     h-2*r, c);          // left edge
    ui_painter_fill_rect(x+w-1, y+r,     1,     h-2*r, c);          // right edge
    stroke_corners_atlas(x, y, w, h, r, c);                         // all four rounded corners
}

// 12x8 checkbox checkmark: 1px sharp core, 50% antialiased edges
static const uint8_t k_checkmark_12x8[8][12] = {
    {  0,   0,   0,   0,   0,   0,   0,   0,   0, 128, 255,   0},
    {  0,   0,   0,   0,   0,   0,   0,   0, 128, 255,   0,   0},
    {  0,   0,   0,   0,   0,   0,   0, 128, 255,   0,   0,   0},
    {  0, 255, 128,   0,   0,   0, 128, 255,   0,   0,   0,   0},
    {  0,   0, 255, 128,   0, 128, 255,   0,   0,   0,   0,   0},
    {  0,   0, 128, 255, 255, 255,   0,   0,   0,   0,   0,   0},
    {  0,   0,   0, 128, 255,   0,   0,   0,   0,   0,   0,   0},
    {  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0}
};

// draws the 12x8 checkmark at (ox, oy)
void blit_checkmark_shared(int ox, int oy, uint32_t c) {
    int x0 = 0, y0 = 0, x1 = 12, y1 = 8;
    if (ox + x0 < g_clip_x0) x0 = g_clip_x0 - ox;
    if (oy + y0 < g_clip_y0) y0 = g_clip_y0 - oy;
    if (ox + x1 > g_clip_x1) x1 = g_clip_x1 - ox;
    if (oy + y1 > g_clip_y1) y1 = g_clip_y1 - oy;
    if (x0 >= x1 || y0 >= y1) return;                    // fully clipped

    uint32_t ca = (c >> 24) & 0xFF;
    if (!ca) return;                                     // fully transparent color
    uint32_t sr = (c >> 16) & 0xFF;
    uint32_t sg = (c >>  8) & 0xFF;
    uint32_t sb =  c        & 0xFF;

    for (int yy = y0; yy < y1; yy++) {
        const uint8_t* run = k_checkmark_12x8[yy] + x0;
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_stride + (ox + x0);
        blend_cov_row(dst, run, x1 - x0, ca, sr, sg, sb);
    }
}

// 9x9 close icon: two 1px diagonals, no antialiasing
static const uint8_t k_close_x_9[9][9] = {
    {255,  0,  0,  0,  0,  0,  0,  0, 255},
    {  0,255,  0,  0,  0,  0,  0, 255,  0},
    {  0,  0,255,  0,  0,  0, 255,  0,  0},
    {  0,  0,  0,255,  0, 255,  0,  0,  0},
    {  0,  0,  0,  0,255,  0,  0,  0,  0},
    {  0,  0,  0,255,  0, 255,  0,  0,  0},
    {  0,  0,255,  0,  0,  0, 255,  0,  0},
    {  0,255,  0,  0,  0,  0,  0, 255,  0},
    {255,  0,  0,  0,  0,  0,  0,  0, 255},
};

// draws the 9x9 close glyph at (ox, oy)
void blit_close_x_shared(int ox, int oy, uint32_t c) {
    int x0 = 0, y0 = 0, x1 = 9, y1 = 9;
    if (ox + x0 < g_clip_x0) x0 = g_clip_x0 - ox;
    if (oy + y0 < g_clip_y0) y0 = g_clip_y0 - oy;
    if (ox + x1 > g_clip_x1) x1 = g_clip_x1 - ox;
    if (oy + y1 > g_clip_y1) y1 = g_clip_y1 - oy;
    if (x0 >= x1 || y0 >= y1) return;                 // fully clipped

    uint32_t ca = (c >> 24) & 0xFF;
    if (!ca) return;                                  // fully transparent color
    uint32_t sr = (c >> 16) & 0xFF;
    uint32_t sg = (c >>  8) & 0xFF;
    uint32_t sb =  c        & 0xFF;

    for (int yy = y0; yy < y1; yy++) {
        const uint8_t* run = k_close_x_9[yy] + x0;
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_stride + (ox + x0);
        blend_cov_row(dst, run, x1 - x0, ca, sr, sg, sb);
    }
}

// draws a single line of text at the given baseline position
void ui_draw_text_at(UiContext* ctx, float x, float baseline,
                     const char* s, int len, float size, uint32_t color) {
    if (!ctx->font || !s || len <= 0) return;           // nothing to draw
    ui_text_draw(ctx->font, g_pixels, g_w, g_h, x, baseline, s, len, size, color);
}
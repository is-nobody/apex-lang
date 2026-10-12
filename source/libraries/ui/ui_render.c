// source/libraries/ui/ui_render.c
// Implementation of UI painter and renderer for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_text.h"
#include "ui_window.h"
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

// active framebuffer and its dimensions
static uint32_t* g_pixels; static int g_w, g_h;

// nested scissor rectangles
static int clip_stack[16][4]; static int clip_depth;

// top-of-stack clip cached as (x0, y0, x1, y1). refreshed on every push/pop
// so the hot pixel loops read two globals instead of chasing the stack.
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
void ui_painter_init(uint32_t* p, int w, int h) {
    g_pixels = p; g_w = w; g_h = h; clip_depth = 0;
    clip_refresh();
    ui_text_set_clip(0, 0, w, h);                        // whole surface: nothing clipped
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

// alpha-blends a single pixel into the framebuffer, honoring the active clip
static inline void blend_px(int x, int y, uint32_t c) {
    if ((unsigned)x >= (unsigned)g_clip_x1 || x < g_clip_x0) return;
    if ((unsigned)y >= (unsigned)g_clip_y1 || y < g_clip_y0) return;
    uint32_t sa = (c >> 24) & 0xFF;                     // source alpha
    if (!sa) return;                                    // fully transparent: nothing to do
    uint32_t idx = (uint32_t)y * g_w + x;               // linear pixel index
    if (sa == 255) { g_pixels[idx] = 0xFF000000u | (c & 0x00FFFFFFu); return; }
    uint32_t inv = 255 - sa;                            // destination weight
    uint32_t d  = g_pixels[idx];
    uint32_t dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr*sa + dr*inv + 128) >> 8;
    uint32_t g = (sg*sa + dg*inv + 128) >> 8;
    uint32_t b = (sb*sa + db*inv + 128) >> 8;
    g_pixels[idx] = 0xFF000000u | (r<<16) | (g<<8) | b;
}

// blends one run of coverage bytes onto the framebuffer with a solid color
static void blend_cov_row(uint32_t* dst, const uint8_t* cov, int n,
                          uint32_t ca, uint32_t sr, uint32_t sg, uint32_t sb) {
    if (ca == 0 || n <= 0) return;
    const bool opaque_ca = (ca == 255);

    int i = 0;

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

    for (; i < n; i++) {                                 // scalar tail
        uint32_t cv = cov[i];
        if (!cv) continue;
        uint32_t sa = opaque_ca ? cv : ((ca * cv + 128) >> 8);
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
}

// simd row blenders
#ifdef UI_HAVE_AVX2
// processes 8 pixels at a time
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

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
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

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
// processes 8 pixels at a time via vld4/vst4 deinterleave.
static inline void blend_row_simd(uint32_t* row, int n, uint32_t c) {
    uint32_t sa  = (c >> 24) & 0xFF;
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;

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
    uint32_t inv = 255 - sa;
    uint32_t sr  = (c >> 16) & 0xFF;
    uint32_t sg  = (c >> 8)  & 0xFF;
    uint32_t sb  =  c        & 0xFF;
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
    uint32_t sa = (c >> 24) & 0xFF;
    if (sa != 255) return;                                          // opaque only
    uint32_t color = 0xFF000000u | (c & 0x00FFFFFFu);
    for (int y = 0; y < g_h; y++) {
        uint32_t* row = g_pixels + (size_t)y * g_w;
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
        uint32_t color = 0xFF000000u | (c & 0x00FFFFFFu);
        for (int yy = y0; yy < y1; yy++) {
            uint32_t* row = g_pixels + (size_t)yy * g_w + x0;
            fill_row_simd(row, rw, color);
        }
        return;
    }

    // alpha path: one SIMD (or scalar) sweep per row
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = g_pixels + (size_t)yy * g_w + x0;
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
                uint32_t* p = g_pixels + (size_t)(oy + yy) * g_w + (ox + xx);
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
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_w + (ox + x0);
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
                blend_cov_row(g_pixels + (size_t)ty * g_w + x + xl0,
                              top_cov + xl0, n_left, ca, sr, sg, sb);
            if (by >= g_clip_y0 && by < g_clip_y1)       // bottom-left
                blend_cov_row(g_pixels + (size_t)by * g_w + x + xl0,
                              bot_cov + xl0, n_left, ca, sr, sg, sb);
        }
        if (n_right > 0) {
            for (int k = 0; k < r; k++) tmp[k] = top_cov[r - 1 - k];
            if (ty >= g_clip_y0 && ty < g_clip_y1)       // top-right
                blend_cov_row(g_pixels + (size_t)ty * g_w + x + w - r + xr0,
                              tmp + xr0, n_right, ca, sr, sg, sb);
            for (int k = 0; k < r; k++) tmp[k] = bot_cov[r - 1 - k];
            if (by >= g_clip_y0 && by < g_clip_y1)       // bottom-right
                blend_cov_row(g_pixels + (size_t)by * g_w + x + w - r + xr0,
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

// 9x6 checkbox checkmark: symmetric V, 2px thick, no antialiasing
static const uint8_t k_checkmark_9x6[6][9] = {
    {255,   0,   0,   0,   0,   0,   0,   0, 255},
    {255, 255,   0,   0,   0,   0,   0, 255, 255},
    {  0, 255, 255,   0,   0,   0, 255, 255,   0},
    {  0,   0, 255, 255,   0, 255, 255,   0,   0},
    {  0,   0,   0, 255, 255, 255,   0,   0,   0},
    {  0,   0,   0,   0, 255,   0,   0,   0,   0},
};

// draws the 9x6 checkmark at (ox, oy)
static void blit_checkmark(int ox, int oy, uint32_t c) {
    int x0 = 0, y0 = 0, x1 = 9, y1 = 6;
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
        const uint8_t* run = k_checkmark_9x6[yy] + x0;
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_w + (ox + x0);
        blend_cov_row(dst, run, x1 - x0, ca, sr, sg, sb);
    }
}

// damage-aware
static void draw_widget(UiContext* ctx, Widget* w, const DamageRect* d);

// draws a single line of text at the given baseline position
static void draw_text_at(UiContext* ctx, float x, float baseline,
                         const char* s, int len, float size, uint32_t color) {
    if (!ctx->font || !s || len <= 0) return;           // nothing to draw
    ui_text_draw(ctx->font, g_pixels, g_w, g_h, x, baseline, s, len, size, color);
}

// returns the font size used for a text-bearing widget kind
static float text_fs(WidgetKind k) { return k == WK_HEADING ? UI_FS_H1 : UI_FS_BODY; }

// returns the line height used for a text-bearing widget kind
static float text_lh(WidgetKind k) { return k == WK_HEADING ? UI_LINE_H + 8 : UI_LINE_H; }

// draws wrapped multi-line text using the line breakdown cached by wrap_measure during layout
static void draw_wrapped(UiContext* ctx, Widget* w, int x, int y, int width, uint32_t color) {
    (void)width;                                        // lines were wrapped for this width at layout
    if (!ctx->font) return;                             // no font: nothing to draw
    const char* s = w->text.ptr;
    if (!s || w->line_count == 0) return;               // no cached breakdown: nothing to draw

    float size = text_fs(w->kind);                      // font size for this widget kind
    float lh   = text_lh(w->kind);                      // line height for this widget kind
    float ly   = (float)y + lh - 4;                     // first baseline
    for (int i = 0; i < w->line_count; i++) {
        int a = w->line_starts[i];
        int b = w->line_ends[i];
        if (b > a)                                      // empty ranges (leading indents) draw nothing
            draw_text_at(ctx, (float)x, ly, s + a, b - a, size, color);
        ly += lh;                                       // advance to the next baseline
    }
}

// recursively draws a widget and all of its visible children
static void draw_widget(UiContext* ctx, Widget* w, const DamageRect* d) {
    if (!w || !w->visible) return;      // nothing to draw

    // fast reject
    if (d) {
        if (w->bbox_x1 <= d->x || w->bbox_x0 >= d->x + d->w ||
            w->bbox_y1 <= d->y || w->bbox_y0 >= d->y + d->h)
            return;
    }

    int x  = (int)floorf(w->x + 0.5f);  // rounded pixel coordinates
    int y  = (int)floorf(w->y + 0.5f);
    int ww = (int)floorf(w->w + 0.5f);
    int hh = (int)floorf(w->h + 0.5f);

    bool draw_self = true;
    if (d) {
        int wx1 = x + ww;
        int wy1 = y + hh;
        if (wx1 <= d->x || x >= d->x + d->w ||
            wy1 <= d->y || y >= d->y + d->h) {
            draw_self = false;
        }
    }

    // a scroll always clips its children to its viewport
    bool clip_pushed = false;
    if (w->kind == WK_SCROLL && ww > 0 && hh > 0) {
        int has_sb = w->content_h > (float)hh;
        int content_w = has_sb ? ww - UI_SCROLL_W : ww;
        if (content_w < 1) content_w = 1;
        ui_painter_push_clip(x, y, content_w, hh);
        clip_pushed = true;
    }

    if (draw_self && ww > 0 && hh > 0) {
        if (w->focused && (w->kind == WK_INPUT || w->kind == WK_COMBOBOX)) {
            ui_painter_stroke_round(x-2, y-2, ww+4, hh+4, UI_RADIUS+2, UI_C_FOCUS);
        }

        switch (w->kind) {
            case WK_TEXT:
                draw_wrapped(ctx, w, x, y, ww, UI_C_FG);  // plain body text
                break;
            case WK_HEADING:
                draw_wrapped(ctx, w, x, y, ww, UI_C_FG);  // heading text uses a larger size
                break;
            case WK_BUTTON: {
                uint32_t bg = UI_C_ACCENT;                // default button background
                if (w->disabled) bg = UI_C_DIS_BG;        // greyed when disabled
                else if (w->pressed) bg = UI_C_ACCENT_A;  // brighter while pressed
                else if (w->hovered) bg = UI_C_ACCENT_H;  // slightly brighter on hover
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, bg);
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER_S);
                float fs = UI_FS_BODY;
                float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : w->text.len*8.0f;
                float tx = x + (ww - tw) * 0.5f;           // center horizontally
                float base = y + (hh + fs * 0.7f) * 0.5f;  // vertically center the cap height
                uint32_t tc = w->disabled ? UI_C_DIS_FG : UI_C_ACCENT_T;
                draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, tc);
                break;
            }
            case WK_BADGE: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS_SM, UI_C_ACCENT);  // pill background
                float fs = UI_FS_SMALL;
                float tw = ctx->font ? ui_text_measure_cached(ctx, w->text.ptr, w->text.len, fs) : 0;
                float tx = x + (ww - tw) * 0.5f;            // center horizontally
                float base = y + hh - 5;                    // baseline near the bottom
                draw_text_at(ctx, tx, base, w->text.ptr, w->text.len, fs, UI_C_ACCENT_T);
                break;
            }
            case WK_CHECKBOX: {
                int sz = UI_CB_SIZE;                        // box side length
                int bx = x, by = y + (hh - sz)/2;           // vertically centered box

                if (w->checked) {                           // checked state: accent fill plus checkmark
                    uint32_t fill = w->hovered ? UI_C_ACCENT_H : UI_C_ACCENT;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER_S);

                    // blit a precomputed 9x6 coverage blob
                    int cx = bx + sz/2 - 1, cy = by + sz/2 - 2;
                    blit_checkmark(cx - 4, cy, UI_C_ACCENT_T);
                } else {                                    // unchecked: empty box
                    uint32_t fill = w->hovered ? UI_C_PANEL_ALT : UI_C_INPUT_BG;
                    ui_painter_fill_round(bx, by, sz, sz, UI_RADIUS_SM, fill);
                    ui_painter_stroke_round(bx, by, sz, sz, UI_RADIUS_SM, UI_C_BORDER);
                }

                float fs = UI_FS_BODY;
                // center the label on the widget instead of pinning it to the bottom
                float base = y + (hh + fs * 0.7f) * 0.5f;
                draw_text_at(ctx, (float)(bx + sz + 8), base,
                             w->text.ptr, w->text.len, fs, UI_C_FG);  // label to the right of the box
                break;
            }
            case WK_RADIO: {
                int sz = UI_CB_SIZE;                        // radio circle side
                int bx = x, by = y + (hh - sz)/2;           // vertically centered circle

                uint32_t fill = w->hovered ? UI_C_PANEL_ALT : UI_C_INPUT_BG;
                ui_painter_fill_round(bx, by, sz, sz, sz/2, fill);              // outer circle
                ui_painter_stroke_round(bx, by, sz, sz, sz/2, UI_C_BORDER);     // outline
                if (w->checked)                                                 // filled dot when selected
                    ui_painter_fill_round(bx+4, by+4, sz-8, sz-8, (sz-8)/2, UI_C_ACCENT);

                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;  // center on the widget
                draw_text_at(ctx, (float)(bx + sz + 8), base, w->text.ptr, w->text.len, fs, UI_C_FG);
                break;
            }
            case WK_PROGRESS: {
                ui_painter_fill_round(x, y, ww, hh, hh/2, UI_C_DIS_BG);         // empty track
                double v = w->num_value;                    // current value, expected 0..1
                if (v < 0) v = 0;
                if (v > 1) v = 1;
                int fw = (int)(ww * v);                     // filled width in pixels
                if (fw > 0) ui_painter_fill_round(x, y, fw, hh, hh/2, UI_C_ACCENT);
                break;
            }
            case WK_SLIDER: {
                int track_y = y + hh/2 - 2;                 // center the track vertically
                ui_painter_fill_round(x, track_y, ww, 4, 2, UI_C_DIS_BG);       // track
                double range = w->num_max - w->num_min;
                double t = range > 0 ? (w->num_value - w->num_min) / range : 0;  // normalized value
                if (t < 0) t = 0;
                if (t > 1) t = 1;
                int thumb_x = x + (int)(t * (ww - 16));     // thumb position along the track
                uint32_t tc = w->pressed ? UI_C_ACCENT_A
                            : w->hovered ? UI_C_ACCENT_H
                            : UI_C_ACCENT;                  // thumb color state
                ui_painter_fill_round(thumb_x, y + hh/2 - 8, 16, 16, 8, tc);    // round thumb
                ui_painter_stroke_round(thumb_x, y + hh/2 - 8, 16, 16, 8, UI_C_BORDER_S);
                break;
            }
            case WK_INPUT: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_INPUT_BG);  // input background
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);  // outline
                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;   // vertically centered text baseline

                WidgetState* s = ui_state_get(ctx, w->id);

                // the persistent cursor and anchor can point past the descriptor's text for one frame
                int lim = w->input_value.len;
                int cur_off = s->cursor < 0 ? lim : s->cursor;
                if (cur_off > lim) cur_off = lim;
                int anc_off = s->sel_anchor < 0 ? lim : s->sel_anchor;
                if (anc_off > lim) anc_off = lim;

                // selection highlight goes under the text so glyphs sit on top
                if (w->focused && anc_off != cur_off) {
                    int a = anc_off < cur_off ? anc_off : cur_off;
                    int b = anc_off < cur_off ? cur_off : anc_off;
                    float x0 = x + 8.0f;
                    float x1 = x + 8.0f;
                    if (ctx->font) {
                        x0 += ui_text_measure_cached(ctx, w->input_value.ptr, a, fs);
                        x1 += ui_text_measure_cached(ctx, w->input_value.ptr, b, fs);
                    }
                    ui_painter_fill_rect((int)x0, y + 5, (int)(x1 - x0), hh - 10, UI_C_SEL_BG);
                }

                draw_text_at(ctx, (float)(x + 8), base, w->input_value.ptr,
                             w->input_value.len, fs, UI_C_FG);

                if (w->focused) {                           // draw the caret when focused
                    if (s->blink_phase < 0.5) {             // visible half of the blink cycle
                        float cx = x + 8;
                        if (ctx->font) cx += ui_text_measure_cached(ctx, w->input_value.ptr, cur_off, fs);
                        ui_painter_fill_rect((int)cx, y + 6, 1, hh - 12, UI_C_FG);
                    }
                }
                break;
            }
            case WK_COMBOBOX: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_INPUT_BG);
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);
                float fs = UI_FS_BODY;
                float base = y + (hh + fs * 0.7f) * 0.5f;
                draw_text_at(ctx, (float)(x + 8), base,
                             w->combo_value.ptr, w->combo_value.len, fs, UI_C_FG);
                int cx = x + ww - 14;
                int cy = y + hh / 2 - 2;
                for (int i = 0; i < 4; i++) {
                    ui_painter_fill_rect(cx + i,     cy + i, 1, 1, UI_C_FG_DIM);
                    ui_painter_fill_rect(cx + 6 - i, cy + i, 1, 1, UI_C_FG_DIM);
                }
                break;
            }
            case WK_SEPARATOR:
                ui_painter_fill_rect(x, y, ww, 1, UI_C_BORDER);  // 1px horizontal rule
                break;
            case WK_PANEL: {
                ui_painter_fill_round(x, y, ww, hh, UI_RADIUS, UI_C_PANEL);      // panel background
                ui_painter_stroke_round(x, y, ww, hh, UI_RADIUS, UI_C_BORDER);   // panel outline
                break;
            }
            case WK_SPACER:
                break;                                      // spacers draw nothing
            case WK_SCROLL:
                break;                                      // clip already pushed above
            case WK_HSCROLL:
            case WK_VBOX:
            case WK_HBOX:
                break;                                      // pure containers: no own chrome
        }
    }

    for (int i = 0; i < w->child_count; i++)                // first pass: normal children
        if (!w->children[i]->on_top)
            draw_widget(ctx, w->children[i], d);

    for (int i = 0; i < w->child_count; i++)                // second pass: on-top overlay children
        if (w->children[i]->on_top)
            draw_widget(ctx, w->children[i], d);

    if (clip_pushed) ui_painter_pop_clip();                 // restore the clip after scroll children

    if (draw_self && w->kind == WK_SCROLL) {
        if (w->content_h > (float)hh) {                     // scrollbar drawn last, above children
            int sb_x = x + ww - UI_SCROLL_W;
            ui_painter_fill_rect(sb_x, y, UI_SCROLL_W, hh, UI_C_SCROLL_T);   // track
            double maxs = w->content_h - hh;                // scrollable extent
            double t = maxs > 0 ? w->scroll_y / maxs : 0;
            int th = (int)(hh * (hh / w->content_h));       // thumb height proportional to viewport
            if (th < 24) th = 24;                           // enforce a minimum thumb size
            int ty = y + (int)(t * (hh - th));              // thumb position
            ui_painter_fill_round(sb_x+2, ty+2, UI_SCROLL_W-4, th-4,
                                  (UI_SCROLL_W-4)/2, UI_C_SCROLL_H);
        }
    }
}

// paints open combobox dropdowns on top of the tree, in pool order
static void draw_combobox_popups(UiContext* ctx, const DamageRect* d) {
    for (int i = 0; i < ctx->pool_used; i++) {
        Widget* w = &ctx->pool[i];
        if (w->kind != WK_COMBOBOX || !w->combo_open || !w->visible) continue;
        if (w->combo_option_count == 0) continue;

        int x  = (int)floorf(w->x + 0.5f);
        int y  = (int)floorf(w->y + 0.5f);
        int ww = (int)floorf(w->w + 0.5f);
        int hh = (int)floorf(w->h + 0.5f);
        int dy = y + hh;
        int dh = w->combo_option_count * UI_INPUT_H;

        if (d) {
            if (x + ww <= d->x || x >= d->x + d->w ||
                dy + dh <= d->y || dy >= d->y + d->h) continue;
        }

        ui_painter_fill_round(x, dy, ww, dh, UI_RADIUS_SM, UI_C_PANEL);
        ui_painter_stroke_round(x, dy, ww, dh, UI_RADIUS_SM, UI_C_BORDER);

        for (int j = 0; j < w->combo_option_count; j++) {
            int oy = dy + j * UI_INPUT_H;
            bool hot = (ctx->input.x >= x && ctx->input.x < x + ww &&
                        ctx->input.y >= oy && ctx->input.y < oy + UI_INPUT_H);
            if (j == w->combo_selected || hot)
                ui_painter_fill_rect(x + 1, oy + 1, ww - 2, UI_INPUT_H - 2, UI_C_PANEL_ALT);
            float fs = UI_FS_BODY;
            float base = oy + (UI_INPUT_H + fs * 0.7f) * 0.5f;
            draw_text_at(ctx, (float)(x + 8), base,
                         w->combo_options[j].ptr, w->combo_options[j].len,
                         fs, UI_C_FG);
        }
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
static void blit_close_x(int ox, int oy, uint32_t c) {
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
        uint32_t* dst = g_pixels + (size_t)(oy + yy) * g_w + (ox + x0);
        blend_cov_row(dst, run, x1 - x0, ca, sr, sg, sb);
    }
}

static uint32_t dlg_fade(uint32_t c, double t) {
    uint32_t a = (c >> 24) & 0xFF;
    a = (uint32_t)(a * t + 0.5);
    return (c & 0x00FFFFFFu) | (a << 24);
}

// draws the currently open modal dialog on top of everything else
static void draw_dialog(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;
    double t = d->anim_t;
    if (t <= 0.0) return;

    // backdrop
    ui_painter_fill_rect(0, 0, ctx->win_w, ctx->win_h, dlg_fade(0x8C000000u, t));

    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);

    uint32_t panel_c  = dlg_fade(UI_C_PANEL,     t);
    uint32_t border_c = dlg_fade(UI_C_BORDER_S,  t);
    uint32_t fg       = dlg_fade(UI_C_FG,        t);
    uint32_t fg_dim   = dlg_fade(UI_C_FG_DIM,    t);
    uint32_t fg_muted = dlg_fade(UI_C_FG_MUTED,  t);
    uint32_t input_bg = dlg_fade(UI_C_INPUT_BG,  t);
    uint32_t input_bd = dlg_fade(UI_C_BORDER,    t);

    ui_painter_fill_round(g.bx, g.by, g.bw, g.bh, DLG_RADIUS, panel_c);
    ui_painter_stroke_round(g.bx, g.by, g.bw, g.bh, DLG_RADIUS, border_c);

    if (d->title && *d->title)
        draw_text_at(ctx, (float)g.tx, (float)g.ty,
                     d->title, (int)strlen(d->title), DLG_TITLE_FS, fg);

    if (d->message && *d->message) {
        const char* s = d->message;
        int len = (int)strlen(s);
        float max_w = (float)(g.bw - 2 * DLG_PAD);
        float py = (float)g.my;
        int i = 0;
        while (i < len) {
            // a hard break (\n, \r, \r\n) closes the current segment
            int seg_end = i;
            while (seg_end < len && s[seg_end] != '\n' && s[seg_end] != '\r') seg_end++;

            // greedy word wrap inside [i, seg_end), flushing one line at a time
            float cur = 0;
            int line_start = i;
            int j = i;
            while (j < seg_end) {
                while (j < seg_end && (s[j] == ' ' || s[j] == '\t')) j++;
                if (j >= seg_end) break;
                int ws = j;
                while (j < seg_end && s[j] != ' ' && s[j] != '\t') j++;
                float ww = ui_text_measure_cached(ctx, s + ws, j - ws, DLG_BODY_FS);
                float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, DLG_BODY_FS) : 0;
                if (cur + sp + ww > max_w && cur > 0) {
                    int le = ws;
                    while (le > line_start && s[le - 1] == ' ') le--;
                    if (le > line_start)
                        draw_text_at(ctx, (float)g.mx, py,
                                     s + line_start, le - line_start,
                                     DLG_BODY_FS, fg);
                    py += DLG_LINE_H;
                    line_start = ws;
                    cur = ww;
                } else {
                    cur += sp + ww;
                }
            }
            // flush the final line of this segment
            int le = seg_end;
            while (le > line_start && s[le - 1] == ' ') le--;
            if (le > line_start)
                draw_text_at(ctx, (float)g.mx, py,
                             s + line_start, le - line_start,
                             DLG_BODY_FS, fg);
            py += DLG_LINE_H;

            i = seg_end;
            if (i < len && s[i] == '\r' && i + 1 < len && s[i + 1] == '\n') i += 2;
            else if (i < len) i++;
        }
    }

    // fields
    int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;
    for (int i = 0; i < d->field_count; i++) {
        int fy = g.field_y + i * (g.field_row_h + 6);
        UiDialogField* f = &d->fields[i];
        if (f->label && *f->label)
            draw_text_at(ctx, (float)(g.bx + DLG_PAD),
                         (float)(fy + (int)(DLG_LABEL_FS * 1.0f)),
                         f->label, (int)strlen(f->label),
                         DLG_LABEL_FS, fg_dim);
        int iy = fy + label_h;
        ui_painter_fill_round(g.bx + DLG_PAD, iy, g.input_w, DLG_INPUT_H, 6, input_bg);
        ui_painter_stroke_round(g.bx + DLG_PAD, iy, g.input_w, DLG_INPUT_H, 6, input_bd);

        float base = iy + (DLG_INPUT_H + DLG_BODY_FS * 0.7f) * 0.5f;
        if (f->input && f->input_len > 0) {
            draw_text_at(ctx, (float)(g.bx + DLG_PAD + 10), base,
                         f->input, f->input_len, DLG_BODY_FS, fg);
        } else if (f->placeholder && *f->placeholder) {
            draw_text_at(ctx, (float)(g.bx + DLG_PAD + 10), base,
                         f->placeholder, (int)strlen(f->placeholder),
                         DLG_BODY_FS, fg_muted);
        }
        if (d->focused_field == i && !d->closing && d->blink_phase < 0.5) {
            float cx = (float)(g.bx + DLG_PAD + 10);
            if (ctx->font && f->input && f->cursor > 0)
                cx += ui_text_measure_cached(ctx, f->input, f->cursor, DLG_BODY_FS);
            ui_painter_fill_rect((int)cx, iy + 6, 1, DLG_INPUT_H - 12, fg);
        }
    }

    // buttons
    for (int i = 0; i < g.btn_count; i++) {
        UiDialogButton* b = &d->buttons[i];
        uint32_t bg;
        if (b->primary)
            bg = (d->active_btn == i) ? UI_C_ACCENT_A
               : (d->hover_btn  == i) ? UI_C_ACCENT_H
                                      : UI_C_ACCENT;
        else
            bg = (d->active_btn == i) ? UI_C_ACCENT_A
               : (d->hover_btn  == i) ? UI_C_ACCENT_H
                                      : UI_C_PANEL;
        uint32_t bd = dlg_fade(b->primary ? UI_C_BORDER_S : UI_C_BORDER, t);
        uint32_t tx_c = dlg_fade(b->primary ? UI_C_ACCENT_T : UI_C_FG,   t);

        ui_painter_fill_round(g.btn_x[i], g.button_y, g.btn_w[i], g.button_h, 6,
                              dlg_fade(bg, t));
        ui_painter_stroke_round(g.btn_x[i], g.button_y, g.btn_w[i], g.button_h, 6, bd);

        const char* lbl = b->label ? b->label : "";
        float tw = ui_text_measure_cached(ctx, lbl, (int)strlen(lbl), DLG_BODY_FS);
        float x0 = g.btn_x[i] + (g.btn_w[i] - tw) * 0.5f;
        float base = g.button_y + (g.button_h + DLG_BODY_FS * 0.7f) * 0.5f;
        draw_text_at(ctx, x0, base, lbl, (int)strlen(lbl), DLG_BODY_FS, tx_c);
    }
}

// paints the custom titlebar across the top of the framebuffer
void ui_render_titlebar(UiContext* ctx) {
    if (!ctx->custom_chrome) return;                     // feature disabled

    int w = ctx->win_w;
    int h = UI_TITLEBAR_H;
    if (w <= 0 || h <= 0) return;                        // degenerate surface

    ui_painter_fill_rect(0, 0, w, h - 1, UI_C_TB_BG);    // titlebar background
    ui_painter_fill_rect(0, h - 1, w, 1, UI_C_BORDER);   // hairline underneath

    // three floating 28x28 buttons pinned to the right with a small gap,
    // matching the apex-engine chrome: minimize, maximize, close
    const int sz  = UI_TB_BTN_SZ;
    const int gap = UI_TB_BTN_GAP;
    const int pr  = UI_TB_BTN_PAD;
    int by  = (h - sz) / 2;
    if (by < UI_TB_RESIZE) by = UI_TB_RESIZE;            // keep buttons clear of the resize strip
    int bx3 = w - pr - sz;
    int bx2 = bx3 - gap - sz;
    int bx1 = bx2 - gap - sz;

    int xs[3] = { bx1, bx2, bx3 };
    for (int i = 0; i < 3; i++) {
        int id = i + 1;                                  // 1 min, 2 max, 3 close
        int bx = xs[i];

        uint32_t fg = UI_C_TB_BTN;                       // default icon color
        if (ctx->tb_active == id) {
            ui_painter_fill_round(bx, by, sz, sz, 6, UI_C_TB_BTN_ACT);
            fg = 0xFFFFFFFFu;
        } else if (ctx->tb_hot == id) {
            ui_painter_fill_round(bx, by, sz, sz, 6, UI_C_TB_BTN_HOV);
            fg = 0xFFFFFFFFu;
        }

        int cx = bx + sz / 2;
        int cy = by + sz / 2;

        // erase color for the restore notch; tracks the button background
        uint32_t bg = UI_C_TB_BG;
        if (ctx->tb_active == id)      bg = UI_C_TB_BTN_ACT;
        else if (ctx->tb_hot == id)    bg = UI_C_TB_BTN_HOV;

        if (id == 1) {                                   // minimize: 10x1 pixel line
            ui_painter_fill_rect(cx - 5, cy, 10, 1, fg);
        } else if (id == 2) {                            // maximize / restore
            bool maxed = ctx->window && ui_window_is_maximized(ctx->window);
            if (!maxed) {
                // 9x9 pixel outline, 1px stroke, no aa
                ui_painter_stroke_rect(cx - 4, cy - 4, 9, 9, fg);
            } else {
                // two overlapping 8x8 outlines; the rear one is punched out
                // where the front one overlaps so the stroke weight stays 1px
                ui_painter_stroke_rect(cx - 3, cy - 4, 8, 8, fg);   // rear
                ui_painter_fill_rect(cx - 5, cy - 2, 8, 8, bg);     // erase front footprint
                ui_painter_stroke_rect(cx - 5, cy - 2, 8, 8, fg);   // front
            }
        } else {                                         // close: 9-pixel diagonals, no aa
            // blit a precomputed 9x9 coverage blob in one pass per row
            blit_close_x(cx - 4, cy - 4, fg);
        }
    }

    // window title at the left edge, truncated with "..." if it would collide with the buttons
    int tlen = (int)strlen(ctx->title);
    if (tlen > 0 && ctx->font) {
        float fs   = UI_FS_BODY;
        float base = (h + fs * 0.7f) * 0.5f;

        const int left_pad  = 14;                     // gap from the window edge
        const int right_pad = 10;                     // gap before the minimize button
        int avail = bx1 - left_pad - right_pad;       // pixels the title may occupy
        if (avail < 0) avail = 0;

        float full_w = ui_text_measure_cached(ctx, ctx->title, tlen, fs);
        if (full_w <= (float)avail) {                 // whole title fits
            ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                         (float)left_pad, base, ctx->title, tlen, fs, UI_C_TB_FG);
        } else {
            static const char ell[] = "...";
            float ell_w = ui_text_measure_cached(ctx, ell, 3, fs);
            if ((float)avail > ell_w) {
                float budget = (float)avail - ell_w;  // width left for the prefix
                int cut = 0;                          // byte length of the kept prefix
                float cut_w = 0.0f;
                int i = 0;
                while (i < tlen) {                    // grow the prefix codepoint by codepoint
                    int next = ui_utf8_next(ctx->title, i, tlen);
                    float w  = ui_text_measure_cached(ctx, ctx->title, next, fs);
                    if (w > budget) break;            // this codepoint would overflow
                    i = next;
                    cut = i;
                    cut_w = w;
                }
                if (cut > 0) {                        // draw prefix then the ellipsis after it
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad, base, ctx->title, cut, fs, UI_C_TB_FG);
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad + cut_w, base, ell, 3, fs, UI_C_TB_FG);
                } else {                              // even a single glyph doesn't fit: just "..."
                    ui_text_draw(ctx->font, ctx->pixels, ctx->win_w, ctx->win_h,
                                 (float)left_pad, base, ell, 3, fs, UI_C_TB_FG);
                }
            }
        }
    }
}

// clears the surface and paints the titlebar plus the whole widget tree
void ui_render_frame(UiContext* ctx) {
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind the painter to the surface
    ui_painter_clear(UI_C_BG);                                  // streaming full-surface clear
    ui_render_titlebar(ctx);                                    // custom chrome, if enabled
    if (ctx->root) {
        if (ctx->custom_chrome) {                               // keep the tree out of the titlebar
            ui_painter_push_clip(0, UI_TITLEBAR_H,
                                 ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
            draw_widget(ctx, ctx->root, NULL);
            draw_combobox_popups(ctx, NULL);
            ui_painter_pop_clip();
        } else {
            draw_widget(ctx, ctx->root, NULL);                  // full repaint, no damage mask
            draw_combobox_popups(ctx, NULL);
        }
    }
    draw_dialog(ctx);                                           // modal dialog on top of everything
}

// repaints only the given damage rectangles, plus the titlebar when it is affected
void ui_render_frame_damage(UiContext* ctx, const DamageRect* rects, int n) {
    if (ctx->dialog.active) {                                   // modal dialog: repaint whole window
        ui_render_frame(ctx);
        return;
    }
    if (n <= 0) return;                                         // nothing damaged, no work
    ui_painter_init(ctx->pixels, ctx->win_w, ctx->win_h);       // bind painter to the framebuffer

    bool tb_damaged = false;                                    // did any rect touch the titlebar?

    for (int i = 0; i < n; i++) {                               // one pass per damage rect
        const DamageRect* d = &rects[i];                        // current damage rect
        if (ctx->custom_chrome && d->y < UI_TITLEBAR_H) tb_damaged = true;
        ui_painter_push_clip(d->x, d->y, d->w, d->h);           // restrict writes to this rect
        if (d->x == 0 && d->y == 0 &&
            d->w >= ctx->win_w && d->h >= ctx->win_h) {
            ui_painter_clear(UI_C_BG);                          // full surface: streaming path
        } else {
            ui_painter_fill_rect(d->x, d->y, d->w, d->h, UI_C_BG);  // partial: regular blend
        }
        if (ctx->root) {
            if (ctx->custom_chrome) {                           // keep the tree out of the titlebar
                ui_painter_push_clip(0, UI_TITLEBAR_H,
                                     ctx->win_w, ctx->win_h - UI_TITLEBAR_H);
                draw_widget(ctx, ctx->root, d);
                draw_combobox_popups(ctx, d);
                ui_painter_pop_clip();
            } else {
                draw_widget(ctx, ctx->root, d);                 // redraw widgets touching rect
                draw_combobox_popups(ctx, d);
            }
        }
        ui_painter_pop_clip();                                  // restore previous clip
    }

    if (tb_damaged) {                                           // repaint the titlebar on top
        ui_painter_push_clip(0, 0, ctx->win_w, UI_TITLEBAR_H);
        ui_painter_fill_rect(0, 0, ctx->win_w, UI_TITLEBAR_H, UI_C_BG);
        ui_render_titlebar(ctx);
        ui_painter_pop_clip();
    }
}
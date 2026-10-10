// source/libraries/ui/ui_text.c
// Implementation of TrueType font rasterizer for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// big-endian readers for the ttf byte stream

// unsigned 16-bit
static uint16_t r16(const uint8_t* p){ return (uint16_t)((p[0]<<8)|p[1]); }

// signed 16-bit
static int16_t  ri16(const uint8_t* p){ return (int16_t)r16(p); }

// unsigned 32-bit
static uint32_t r32(const uint8_t* p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

// counts the number of utf-8 codepoints in the first n bytes
int ui_utf8_len(const char* s, int n) {
    int c = 0; for (int i = 0; i < n;) {                            // walk byte-by-byte
        unsigned char b = (unsigned char)s[i];
        i += (b < 0x80) ? 1 : (b < 0xE0) ? 2 : (b < 0xF0) ? 3 : 4;  // skip a full codepoint
        c++;
    } return c;
}

// returns the byte offset of the codepoint preceding off
int ui_utf8_prev(const char* s, int off) {
    if (off <= 0) return 0;                                     // already at the start
    int i = off - 1;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;  // skip continuation bytes
    return i;
}

// returns the byte offset of the codepoint following off, capped at n
int ui_utf8_next(const char* s, int off, int n) {
    if (off >= n) return n;                                           // already at the end
    unsigned char b = (unsigned char)s[off];
    int step = (b < 0x80) ? 1 : (b < 0xE0) ? 2 : (b < 0xF0) ? 3 : 4;  // length from the leading byte
    int r = off + step; if (r > n) r = n;                             // clamp to the end
    return r;
}

// decodes one utf-8 codepoint, advancing *adv past it
static uint32_t decode_cp(const char* s, int n, int* adv) {
    unsigned char b = (unsigned char)s[0];
    if (b < 0x80) { *adv = 1; return b; }                // 1-byte ascii
    if ((b & 0xE0) == 0xC0 && n >= 2) { *adv = 2; return ((b&0x1F)<<6)|((unsigned char)s[1]&0x3F); }
    if ((b & 0xF0) == 0xE0 && n >= 3) { *adv = 3; return ((b&0x0F)<<12)|(((unsigned char)s[1]&0x3F)<<6)|((unsigned char)s[2]&0x3F); }
    if ((b & 0xF8) == 0xF0 && n >= 4) { *adv = 4; return ((b&0x07)<<18)|(((unsigned char)s[1]&0x3F)<<12)|(((unsigned char)s[2]&0x3F)<<6)|((unsigned char)s[3]&0x3F); }
    *adv = 1; return 0xFFFD;                             // malformed sequence: replacement char
}

// one cached rasterized glyph at a fixed pixel size
typedef struct {
    int      gid;                 // glyph id in the font
    uint16_t size_q;              // size in quarter-pixels (size * 4)
    int      w, h, off_x, off_y;  // bitmap size and offsets from pen
    float    advance;             // horizontal advance in pixels
    uint8_t* cov;                 // coverage bitmap, one byte per pixel
} CacheEntry;

// loaded TrueType font state and its glyph cache
struct UiFont {
    uint8_t* data;                                       // owned copy of the font file bytes
    size_t   size;                                       // size of the font file
    uint32_t cmap_off, glyf_off, loca_off, head_off, hhea_off, hmtx_off, maxp_off;  // table offsets
    uint16_t upem;                                       // units per em from the head table
    int16_t  loca_fmt;                                   // loca table format, 0 for short, 1 for long
    uint16_t num_glyphs, num_hmetrics;                   // glyph and hmetric counts
    int16_t  ascender, descender;                        // vertical metrics from hhea
    uint32_t cmap_sub;                                   // chosen cmap subtable offset
    int      cmap_fmt;                                   // chosen cmap format (4 or 12)
    CacheEntry* cache;                                   // glyph cache entries
    int cache_count, cache_cap;                          // live count and capacity of the cache
};

// parses the sfnt directory to locate every required table and pick a cmap
static bool find_tables(UiFont* f) {
    if (f->size < 12) return false;                      // too small to hold a header
    uint32_t sfnt = r32(f->data);                        // sfnt version
    if (sfnt != 0x00010000 && sfnt != 0x74727565) return false;  // true or "true"
    uint16_t nt = r16(f->data + 4);                      // number of table records
    for (uint16_t i = 0; i < nt; i++) {
        const uint8_t* rec = f->data + 12 + (size_t)i*16;  // record at 16 bytes each
        uint32_t tag = r32(rec), off = r32(rec+8), len = r32(rec+12);  // tag, offset, length
        if (off >= f->size) continue;                    // skip out of range
        if ((uint64_t)off + len > f->size) len = (uint32_t)(f->size - off);  // clamp length to file
        switch (tag) {                                   // dispatch on the four-character tag
            case 0x636D6170: f->cmap_off = off; break;   // 'cmap'
            case 0x676C7966: f->glyf_off = off; break;   // 'glyf'
            case 0x6C6F6361: f->loca_off = off; break;   // 'loca'
            case 0x68656164: f->head_off = off; break;   // 'head'
            case 0x68686561: f->hhea_off = off; break;   // 'hhea'
            case 0x686D7478: f->hmtx_off = off; break;   // 'hmtx'
            case 0x6D617870: f->maxp_off = off; break;   // 'maxp'
        }
    }
    if (!f->cmap_off || !f->glyf_off || !f->loca_off ||
        !f->head_off || !f->hhea_off || !f->hmtx_off || !f->maxp_off) return false;  // missing required table

    f->upem = r16(f->data + f->head_off + 18);           // units per em
    f->loca_fmt = ri16(f->data + f->head_off + 50);      // loca offset format
    f->num_glyphs = r16(f->data + f->maxp_off + 4);      // glyph count
    f->num_hmetrics = r16(f->data + f->hhea_off + 34);   // number of hmtx entries
    f->ascender = ri16(f->data + f->hhea_off + 4);       // ascender in font units
    f->descender = ri16(f->data + f->hhea_off + 6);      // descender in font units

    uint16_t n = r16(f->data + f->cmap_off + 2);         // number of cmap records
    int best = -1, best_fmt = 0;
    uint32_t best_off = 0;
    for (uint16_t i = 0; i < n; i++) {
        const uint8_t* rec = f->data + f->cmap_off + 4 + (size_t)i*8;  // record at 8 bytes each
        uint16_t plat = r16(rec), enc = r16(rec+2);      // platform and encoding ids
        uint32_t sub = (uint32_t)f->cmap_off + r32(rec+4);  // subtable offset
        if (sub + 2 > f->size) continue;                 // out of range
        uint16_t fmt = r16(f->data + sub);               // subtable format
        int score = -1;                                  // preference score
        if (plat == 3 && enc == 10 && fmt == 12) score = 100;  // windows ucs-4 full
        else if (plat == 0 && fmt == 12)        score = 95;    // unicode full
        else if (plat == 3 && enc == 1 && fmt == 4) score = 90;  // windows bmp
        else if (plat == 0 && fmt == 4)         score = 85;    // unicode bmp
        else if (fmt == 12)                     score = 60;    // any other full format
        else if (fmt == 4)                      score = 50;    // any other bmp format
        if (score > best) { best = score; best_off = sub; best_fmt = fmt; }  // keep best
    }
    if (best < 0) return false;                          // no usable subtable
    f->cmap_sub = best_off; f->cmap_fmt = best_fmt;
    return true;
}

// loads a ttf from a memory buffer, making a private copy
UiFont* ui_font_load_memory(const uint8_t* data, size_t len) {
    if (!data || len < 12) return NULL;                  // trivial sanity check
    UiFont* f = (UiFont*)calloc(1, sizeof(UiFont));      // zeroed font struct
    f->data = (uint8_t*)malloc(len);                     // take ownership of a copy
    memcpy(f->data, data, len);
    f->size = len;
    if (!find_tables(f)) { ui_font_free(f); return NULL; }  // parse failed: bail out
    return f;
}

// releases a font and its cached glyphs
void ui_font_free(UiFont* f) {
    if (!f) return;                                      // null guard
    for (int i = 0; i < f->cache_count; i++) free(f->cache[i].cov);  // free each coverage bitmap
    free(f->cache);                                      // free the cache array
    free(f->data);                                       // free the font bytes
    free(f);                                             // free the font struct
}

// returns true when the font pointer is non-null, used by callers as a validity check
int ui_font_has(UiFont* f) { return f != NULL; }

// maps a unicode codepoint to a glyph index using the chosen cmap subtable
static uint16_t glyph_index(UiFont* f, uint32_t cp) {
    const uint8_t* p = f->data;
    uint32_t s = f->cmap_sub;
    if (f->cmap_fmt == 4) {                              // format 4: segmented bmp mapping
        if (cp > 0xFFFF) return 0;                       // outside the bmp
        uint16_t seg = r16(p + s + 6) / 2;               // segment count
        const uint8_t* end   = p + s + 14;               // end code array
        const uint8_t* start = end + seg*2 + 2;          // start code array, after reserved pad
        const uint8_t* delta = start + seg*2;            // id delta array
        const uint8_t* rng   = delta + seg*2;            // id range offset array
        for (uint16_t i = 0; i < seg; i++) {
            uint16_t ec = r16(end + i*2);                // segment end code
            if (cp > ec) continue;                       // not this segment
            uint16_t sc = r16(start + i*2);              // segment start code
            if (cp < sc) return 0;                       // before the segment start
            int16_t d = ri16(delta + i*2);               // id delta
            uint16_t ro = r16(rng + i*2);                // id range offset
            if (ro == 0) return (uint16_t)(cp + d);      // direct delta mapping
            const uint8_t* ga = rng + i*2 + ro + (cp - sc)*2;  // glyph id array lookup
            uint16_t g = r16(ga);
            if (g == 0) return 0;                        // missing glyph
            return (uint16_t)(g + d);                    // offset glyph id
        }
        return 0;                                        // no match
    }
    if (f->cmap_fmt == 12) {                             // format 12: sequential groups
        uint32_t ng = r32(p + s + 12);                   // number of groups
        const uint8_t* grp = p + s + 16;                 // groups array at 12 bytes each
        for (uint32_t i = 0; i < ng; i++) {
            uint32_t a = r32(grp + i*12), b = r32(grp + i*12 + 4);  // group start and end
            if (cp < a) return 0;                        // before the first group
            if (cp > b) continue;                        // past this group
            return (uint16_t)(r32(grp + i*12 + 8) + (cp - a));  // offset within the group
        }
    }
    return 0;                                            // no mapping
}

// returns the raw advance width in font units for a glyph
static float advance_raw(UiFont* f, uint16_t gid) {
    if (!f->num_hmetrics) return 0;                      // no hmtx table
    uint16_t idx = gid < f->num_hmetrics ? gid : (uint16_t)(f->num_hmetrics - 1);  // clamp to last entry
    uint32_t off = f->hmtx_off + (uint32_t)idx * 4;      // hmtx record at 4 bytes each
    if (off + 2 > f->size) return 0;                     // out of range
    return (float)r16(f->data + off);                    // first field is the advance width
}

// measures the advance width of a utf-8 string at the given pixel size
float ui_text_measure(UiFont* f, const char* s, int n, float size) {
    if (!f || !s) return 0;                              // nothing to measure
    float scale = size / (float)f->upem;                 // convert font units to pixels
    float w = 0;
    int i = 0;
    while (i < n) {
        int adv; uint32_t cp = decode_cp(s + i, n - i, &adv);  // decode one codepoint
        i += adv;
        uint16_t gid = glyph_index(f, cp);               // map to a glyph id
        w += advance_raw(f, gid) * scale;                // add its scaled advance
    }
    return w;
}

typedef struct { float x, y; } P2;                       // 2d point
typedef struct { P2* pts; int n, cap; } Poly;            // growable polygon

// appends a point to a polygon, growing the buffer as needed
static void poly_push(Poly* p, float x, float y) {
    if (p->n >= p->cap) {
        p->cap = p->cap ? p->cap * 2 : 32;               // start at 32, then double
        p->pts = (P2*)realloc(p->pts, sizeof(P2) * p->cap);
    }
    p->pts[p->n].x = x; p->pts[p->n].y = y; p->n++;
}

// rasterizes a set of polygons into a coverage bitmap using 4x vertical supersampling
static void raster_polys(Poly* polys, int npolys,
                         int W, int H, int ox, int oy, uint8_t* out) {
    memset(out, 0, (size_t)W * H);                       // start with an empty coverage map
    const int SS = 4;                                    // 4 sub-scanlines per pixel row
    for (int y = 0; y < H; y++) {
        for (int sy = 0; sy < SS; sy++) {
            float fy = y + (sy + 0.5f) / SS + oy;        // y of this sub-scanline in glyph space
            static int cap = 0; static float* xs = NULL; // reused crossings buffer
            int nx = 0;                                  // number of crossings found
            for (int pi = 0; pi < npolys; pi++) {        // collect edges crossing this scanline
                Poly* p = &polys[pi];
                for (int i = 0; i < p->n; i++) {
                    P2 a = p->pts[i], b = p->pts[(i + 1) % p->n];  // edge from a to b
                    if ((a.y <= fy && b.y > fy) || (b.y <= fy && a.y > fy)) {  // straddles the line
                        float t = (fy - a.y) / (b.y - a.y);    // parametric position along the edge
                        float x = a.x + t * (b.x - a.x);       // x of the crossing
                        if (nx >= cap) { cap = cap ? cap * 2 : 64;
                            xs = (float*)realloc(xs, sizeof(float)*cap); }
                        xs[nx++] = x;
                    }
                }
            }
            for (int i = 1; i < nx; i++) {               // insertion sort the crossings
                float v = xs[i]; int j = i - 1;
                while (j >= 0 && xs[j] > v) { xs[j+1] = xs[j]; j--; }
                xs[j+1] = v;
            }
            for (int i = 0; i + 1 < nx; i += 2) {        // fill the even-odd spans
                float xa = xs[i] - ox, xb = xs[i+1] - ox;  // span in output space
                if (xb <= 0 || xa >= W) continue;        // fully outside the bitmap
                if (xa < 0) xa = 0;                      // clamp to the left edge
                if (xb > W) xb = W;                      // clamp to the right edge
                int ia = (int)xa, ib = (int)xb;
                for (int x = ia; x <= ib && x < W; x++) {
                    float lo = (float)x, hi = lo + 1.0f; // pixel bounds
                    float a = xa > lo ? xa : lo;         // covered span within the pixel
                    float b = xb < hi ? xb : hi;
                    if (b <= a) continue;                // no coverage
                    int cov = (int)((b - a) * 255.0f / SS + 0.5f);  // coverage scaled by sub-scanlines
                    if (cov > 0) {
                        int cur = out[(size_t)y*W + x];  // accumulate across scanlines
                        int nv = cur + cov; if (nv > 255) nv = 255;
                        out[(size_t)y*W + x] = (uint8_t)nv;
                    }
                }
            }
        }
    }
}

// decodes a simple glyph's contours into font-unit polygons
//
// this is the positive-contour-count case. all coordinates stay in font
// units here; the scale into pixels is applied by the caller so that
// composite transforms (which are also in font units) compose cleanly.
static bool decode_simple_glyph(UiFont* f, uint32_t goff, uint32_t glen, int16_t nc,
                                Poly** polys_out, int* npolys_out,
                                int* xmin_out, int* ymin_out, int* xmax_out, int* ymax_out) {
    const uint8_t* p = f->data;
    uint32_t end = goff + glen;
    if (end > f->size) end = f->size;
    uint32_t c = goff + 10;                              // cursor past the header
    if (c > end) return false;

    uint16_t endpts[256];                                // end point indices per contour
    for (int i = 0; i < nc; i++) {                       // nc is guaranteed <= 256 by the caller
        if (c + 2 > end) return false;
        endpts[i] = r16(p + c);
        c += 2;
    }
    if (c + 2 > end) return false;
    uint16_t ins = r16(p + c);                           // instruction length
    c += 2 + ins;                                        // skip the hinting instructions
    if (c > end) return false;

    int np = endpts[nc-1] + 1;                           // total number of points
    if (np <= 0 || np > 2000) return false;              // sanity bounds

    static uint8_t flags[2048];                          // per-point flag byte
    int fi = 0;
    while (fi < np) {
        if (c >= end) return false;
        uint8_t fl = p[c++];                             // read a flag byte
        flags[fi++] = fl;
        if (fl & 0x08) {                                 // repeat flag set
            if (c >= end) return false;
            uint8_t r = p[c++];                          // run length
            for (int k = 0; k < r && fi < np; k++) flags[fi++] = fl;
        }
    }
    int xs[2048], ys[2048];                              // decoded point coordinates in font units
    int32_t x = 0;
    for (int i = 0; i < np; i++) {                       // decode x deltas
        if (flags[i] & 0x02) {
            if (c >= end) return false;
            int32_t d = p[c++];                          // 1-byte delta
            x += (flags[i] & 0x10) ? d : -d;             // sign from the "same/opposite" bit
        } else if (!(flags[i] & 0x10)) {
            if (c + 2 > end) return false;
            x += ri16(p + c); c += 2;                    // 2-byte signed delta
        }
        xs[i] = x;
    }
    int32_t y = 0;
    for (int i = 0; i < np; i++) {                       // decode y deltas
        if (flags[i] & 0x04) {
            if (c >= end) return false;
            int32_t d = p[c++];                          // 1-byte delta
            y += (flags[i] & 0x20) ? d : -d;             // sign from the "same/opposite" bit
        } else if (!(flags[i] & 0x20)) {
            if (c + 2 > end) return false;
            y += ri16(p + c); c += 2;                    // 2-byte signed delta
        }
        ys[i] = y;
    }

    Poly* polys = (Poly*)calloc(nc, sizeof(Poly));       // one polygon per contour
    int pi2 = 0, si = 0;                                 // output polygon index, start point index
    int xmin = 0x7FFFFFFF, ymin = 0x7FFFFFFF, xmax = -1, ymax = -1;  // font-unit bounds
    for (int ci = 0; ci < nc; ci++) {
        int ei = endpts[ci];                             // end point index of this contour
        Poly* pl = &polys[pi2++];
        for (int i = si; i <= ei; i++) {
            float px = xs[i], py = ys[i];                // keep raw font units
            if (xs[i] < xmin) xmin = xs[i];              // track raw bounds
            if (ys[i] < ymin) ymin = ys[i];
            if (xs[i] > xmax) xmax = xs[i];
            if (ys[i] > ymax) ymax = ys[i];
            int prev = (i == si) ? ei : i - 1;           // neighbouring points, wrapping at contour ends
            int next = (i == ei) ? si : i + 1;
            bool on = (flags[i] & 0x01) != 0;            // on-curve flag
            if (on) { poly_push(pl, px, py); continue; } // on-curve points pass through
            float ax = xs[prev], ay = ys[prev];          // previous point in font units
            float cx = px, cy = py;                      // current point (off-curve control)
            float bx = xs[next], by = ys[next];          // next point in font units
            bool prev_on = (flags[prev] & 0x01) != 0;    // neighbour on-curve flags
            bool next_on = (flags[next] & 0x01) != 0;
            if (!prev_on && next_on) {                   // implied start at the midpoint between controls
                float sx = (ax + cx) * 0.5f, sy = (ay + cy) * 0.5f;
                poly_push(pl, sx, sy);
                for (int k = 1; k <= 8; k++) {           // flatten the quadratic into 8 segments
                    float t = k / 8.0f, mt = 1 - t;
                    poly_push(pl, mt*mt*sx + 2*mt*t*cx + t*t*bx,
                                  mt*mt*sy + 2*mt*t*cy + t*t*by);
                }
            } else if (prev_on && next_on) {             // plain quadratic
                for (int k = 1; k <= 8; k++) {
                    float t = k / 8.0f, mt = 1 - t;
                    poly_push(pl, mt*mt*ax + 2*mt*t*cx + t*t*bx,
                                  mt*mt*ay + 2*mt*t*cy + t*t*by);
                }
            } else {
                poly_push(pl, px, py);                   // consecutive off-curve: keep the control
            }
        }
        si = ei + 1;                                     // advance to the next contour
    }
    *polys_out = polys; *npolys_out = pi2;
    *xmin_out = xmin; *ymin_out = ymin; *xmax_out = xmax; *ymax_out = ymax;
    return true;
}

// forward declaration: composite decoder recurses through the dispatcher
static bool decode_glyph_units(UiFont* f, uint16_t gid,
                               Poly** polys_out, int* npolys_out,
                               int* xmin_out, int* ymin_out, int* xmax_out, int* ymax_out,
                               int depth);

// growable list of polygons used while assembling a composite glyph
typedef struct { Poly* polys; int count, cap; } PolyList;

// decodes a composite glyph by transforming and concatenating its components
//
// composite glyphs (negative contour count) reference other glyphs, each
// with its own affine transform. we recurse into every component, apply the
// transform in font units, and merge the resulting polygons. depth caps the
// recursion against pathological or self-referential fonts. components that
// use point-matching instead of explicit offsets are skipped, which is the
// same convention as the reference rasterizers for the rare fonts that use
// it at all.
static bool decode_composite_glyph(UiFont* f, uint32_t goff, uint32_t glen,
                                   Poly** polys_out, int* npolys_out,
                                   int* xmin_out, int* ymin_out, int* xmax_out, int* ymax_out,
                                   int depth) {
    if (depth > 8) return false;                         // guard against cycles
    const uint8_t* p = f->data;
    uint32_t end = goff + glen;
    if (end > f->size) end = f->size;
    if (goff + 10 > end) return false;

    uint32_t c = goff + 10;                              // cursor past the 10-byte header
    PolyList pl = {0, 0, 0};                             // growing list of polygons
    int xmin = 0x7FFFFFFF, ymin = 0x7FFFFFFF, xmax = -1, ymax = -1;

    while (c + 4 <= end) {
        uint16_t flags   = r16(p + c);                   // component flags
        uint16_t sub_gid = r16(p + c + 2);               // referenced glyph id
        c += 4;

        int16_t arg1 = 0, arg2 = 0;                      // component arguments
        if (flags & 0x0001) {                            // ARG_1_AND_2_ARE_WORDS
            if (c + 4 > end) break;
            arg1 = ri16(p + c);
            arg2 = ri16(p + c + 2);
            c += 4;
        } else {                                         // byte-sized arguments
            if (c + 2 > end) break;
            arg1 = (int8_t)p[c];
            arg2 = (int8_t)p[c + 1];
            c += 2;
        }

        // scale / matrix fields, F2Dot14 fixed point
        float a = 1.0f, b = 0.0f, cc = 0.0f, d = 1.0f;
        if (flags & 0x0008) {                            // WE_HAVE_A_SCALE
            if (c + 2 > end) break;
            a = d = ri16(p + c) / 16384.0f;
            c += 2;
        } else if (flags & 0x0040) {                     // WE_HAVE_AN_X_AND_Y_SCALE
            if (c + 4 > end) break;
            a = ri16(p + c)     / 16384.0f;
            d = ri16(p + c + 2) / 16384.0f;
            c += 4;
        } else if (flags & 0x0080) {                     // WE_HAVE_A_TWO_BY_TWO
            if (c + 8 > end) break;
            a  = ri16(p + c)     / 16384.0f;
            b  = ri16(p + c + 2) / 16384.0f;
            cc = ri16(p + c + 4) / 16384.0f;
            d  = ri16(p + c + 6) / 16384.0f;
            c += 8;
        }

        bool more = (flags & 0x0020) != 0;               // MORE_COMPONENTS

        // translation is only meaningful in the xy-offset form; the
        // point-matching form is rare and is skipped whole
        float dx = 0.0f, dy = 0.0f;
        if (flags & 0x0002) {                            // ARGS_ARE_XY_VALUES
            dx = (float)arg1;
            dy = (float)arg2;
        } else {
            if (!more) break;                            // last component, still skipping it
            continue;
        }

        Poly* sub = NULL; int sub_n = 0;                 // recursively decoded component
        int sx0, sy0, sx1, sy1;
        if (decode_glyph_units(f, sub_gid, &sub, &sub_n, &sx0, &sy0, &sx1, &sy1, depth + 1)) {
            for (int i = 0; i < sub_n; i++) {
                if (sub[i].n < 2) continue;              // skip degenerate contours
                if (pl.count >= pl.cap) {                // grow the output list
                    pl.cap = pl.cap ? pl.cap * 2 : 8;
                    pl.polys = (Poly*)realloc(pl.polys, sizeof(Poly) * pl.cap);
                }
                Poly* dst = &pl.polys[pl.count++];
                dst->n   = sub[i].n;
                dst->cap = sub[i].n;
                dst->pts = (P2*)malloc(sizeof(P2) * sub[i].n);
                for (int j = 0; j < sub[i].n; j++) {
                    float x = sub[i].pts[j].x;           // source point in font units
                    float y = sub[i].pts[j].y;
                    float px = a*x + cc*y + dx;          // apply 2x2 matrix then translate
                    float py = b*x + d*y  + dy;
                    dst->pts[j].x = px;
                    dst->pts[j].y = py;
                    int ix = (int)floorf(px);            // track composite bounds
                    int iy = (int)floorf(py);
                    if (ix < xmin) xmin = ix;
                    if (iy < ymin) ymin = iy;
                    if (ix > xmax) xmax = ix;
                    if (iy > ymax) ymax = iy;
                }
            }
            for (int i = 0; i < sub_n; i++) free(sub[i].pts);  // release the temp component
            free(sub);
        }

        if (!more) break;                                // no further components
    }

    if (pl.count == 0 || xmin > xmax || ymin > ymax) {   // nothing valid was assembled
        free(pl.polys);
        return false;
    }
    *polys_out = pl.polys;
    *npolys_out = pl.count;
    *xmin_out = xmin; *ymin_out = ymin; *xmax_out = xmax; *ymax_out = ymax;
    return true;
}

// dispatches a glyph id to the simple or composite decoder
//
// everything returns polygons in font units. the top-level decode_glyph
// wrapper applies the pixel scale once, after composition, so composite
// transforms stay in a single consistent coordinate system.
static bool decode_glyph_units(UiFont* f, uint16_t gid,
                               Poly** polys_out, int* npolys_out,
                               int* xmin_out, int* ymin_out, int* xmax_out, int* ymax_out,
                               int depth) {
    if (depth > 8) return false;                         // recursion depth guard
    if (gid >= f->num_glyphs) return false;              // glyph id out of range

    const uint8_t* p = f->data;
    uint32_t g1, g2;                                     // glyph data start and end
    if (f->loca_fmt == 0) {                              // short loca: 2-byte offsets, times two
        uint32_t b = f->loca_off + gid*2;
        if (b + 4 > f->size) return false;
        g1 = (uint32_t)r16(p + b) * 2;
        g2 = (uint32_t)r16(p + b + 2) * 2;
    } else {                                             // long loca: 4-byte offsets
        uint32_t b = f->loca_off + (uint32_t)gid*4;
        if (b + 8 > f->size) return false;
        g1 = r32(p + b); g2 = r32(p + b + 4);
    }
    if (g2 <= g1) return false;                          // empty glyph
    uint32_t goff = f->glyf_off + g1;
    uint32_t glen = g2 - g1;
    if (goff + glen > f->size) return false;
    if (glen < 10) return false;                         // header must fit

    int16_t nc = ri16(p + goff);                         // signed contour count
    if (nc == 0) return false;                           // empty glyph (space etc.)
    if (nc > 0) {                                        // simple glyph
        if (nc > 256) return false;                      // endpts[] array bound
        return decode_simple_glyph(f, goff, glen, nc,
                                   polys_out, npolys_out,
                                   xmin_out, ymin_out, xmax_out, ymax_out);
    }
    return decode_composite_glyph(f, goff, glen,         // negative count: composite
                                  polys_out, npolys_out,
                                  xmin_out, ymin_out, xmax_out, ymax_out,
                                  depth);
}

// decodes a glyph into pixel-space polygons with the given pixel scale
//
// this is the public-facing helper used by the glyph cache. it dispatches
// to the unit-space decoders and then scales every point, so the pixel
// scale is applied exactly once at the boundary.
static bool decode_glyph(UiFont* f, uint16_t gid, Poly** polys_out, int* npolys_out,
                         int* xmin_out, int* ymin_out, int* xmax_out, int* ymax_out,
                         float scale) {
    Poly* polys = NULL;
    int npolys = 0;
    int xmin, ymin, xmax, ymax;
    if (!decode_glyph_units(f, gid, &polys, &npolys, &xmin, &ymin, &xmax, &ymax, 0))
        return false;
    for (int i = 0; i < npolys; i++)                     // apply pixel scale to every point
        for (int j = 0; j < polys[i].n; j++) {
            polys[i].pts[j].x *= scale;
            polys[i].pts[j].y *= scale;
        }
    *polys_out = polys; *npolys_out = npolys;
    *xmin_out = xmin; *ymin_out = ymin; *xmax_out = xmax; *ymax_out = ymax;
    return true;
}

// fetches a cached glyph bitmap, rasterizing and inserting on first miss
static CacheEntry* glyph_get(UiFont* f, uint16_t gid, uint16_t size_q) {
    for (int i = 0; i < f->cache_count; i++)             // linear scan of the cache
        if (f->cache[i].gid == gid && f->cache[i].size_q == size_q)
            return &f->cache[i];
    if (f->cache_count >= f->cache_cap) {                // cache full: grow
        f->cache_cap = f->cache_cap ? f->cache_cap * 2 : 128;
        f->cache = (CacheEntry*)realloc(f->cache, sizeof(CacheEntry) * f->cache_cap);
    }
    CacheEntry* e = &f->cache[f->cache_count++];         // take the next slot
    memset(e, 0, sizeof(*e));
    e->gid = gid; e->size_q = size_q;
    float size = size_q / 4.0f;                          // undo the quarter-pixel quantization
    float scale = size / (float)f->upem;                 // font units to pixels
    e->advance = advance_raw(f, gid) * scale;            // scaled advance for this size

    Poly* polys = NULL; int npolys = 0;
    int xmin=0, ymin=0, xmax=0, ymax=0;
    if (!decode_glyph(f, gid, &polys, &npolys, &xmin, &ymin, &xmax, &ymax, scale))
        return e;                                        // blank glyph, no coverage bitmap

    float fxmin = xmin * scale, fymin = ymin * scale;    // scaled bounds
    float fxmax = xmax * scale, fymax = ymax * scale;
    int ox = (int)floorf(fxmin) - 1;                     // one pixel of padding on each side
    int oy = (int)floorf(fymin) - 1;
    int W = (int)ceilf(fxmax) - ox + 1;                  // bitmap width
    int H = (int)ceilf(fymax) - oy + 1;                  // bitmap height
    if (W <= 0 || H <= 0 || W > 512 || H > 512) {        // sanity bounds
        for (int i = 0; i < npolys; i++) free(polys[i].pts);
        free(polys);
        return e;
    }
    e->w = W; e->h = H; e->off_x = ox; e->off_y = oy;    // remember the placement
    e->cov = (uint8_t*)calloc((size_t)W*H, 1);           // allocate the coverage bitmap
    raster_polys(polys, npolys, W, H, ox, oy, e->cov);   // fill it

    for (int i = 0; i < npolys; i++) free(polys[i].pts); // release temporary polygons
    free(polys);
    return e;
}

// alpha-blends a single subpixel-weighted glyph sample into the framebuffer
static inline void blend(uint32_t* fb, int idx, uint32_t c, uint8_t cov) {
    uint32_t sa = ((c >> 24) & 0xFF) * cov / 255;        // combine source alpha with coverage
    if (!sa) return;                                     // fully transparent
    uint32_t inv = 255 - sa;                             // destination weight
    uint32_t dr = (fb[idx] >> 16) & 0xFF, dg = (fb[idx] >> 8) & 0xFF, db = fb[idx] & 0xFF;
    uint32_t sr = (c >> 16) & 0xFF, sg = (c >> 8) & 0xFF, sb = c & 0xFF;
    uint32_t r = (sr * sa + dr * inv) / 255;
    uint32_t g = (sg * sa + dg * inv) / 255;
    uint32_t b = (sb * sa + db * inv) / 255;
    fb[idx] = 0xFF000000u | (r << 16) | (g << 8) | b;
}

// draws a utf-8 string into the framebuffer and returns the advance width in pixels
float ui_text_draw(UiFont* f, uint32_t* fb, int fbw, int fbh,
                   float x, float baseline, const char* s, int n,
                   float size, uint32_t color) {
    if (!f || !s) return 0;                              // nothing to draw
    float scale = size / (float)f->upem;                 // not used directly, kept for clarity
    uint16_t size_q = (uint16_t)(size * 4 + 0.5f);       // quantized size to share cache entries
    float pen = x;                                       // running pen position
    int i = 0;
    int base_y = (int)floorf(baseline + 0.5f);           // rounded baseline row
    while (i < n) {
        int adv; uint32_t cp = decode_cp(s + i, n - i, &adv);  // decode one codepoint
        i += adv;
        uint16_t gid = glyph_index(f, cp);               // map to a glyph
        CacheEntry* e = glyph_get(f, gid, size_q);       // fetch or rasterize the bitmap
        if (e->w > 0) {                                  // bitmap present
            int px = (int)floorf(pen + 0.5f) + e->off_x; // left edge in framebuffer space
            int py = base_y - e->off_y;                  // baseline row in framebuffer space
            for (int yy = 0; yy < e->h; yy++) {          // coverage bitmap is stored top-down
                int ty = py - yy;                        // font y grows up, framebuffer y grows down
                if (ty < 0 || ty >= fbh) continue;       // row outside the framebuffer
                const uint8_t* row = e->cov + (size_t)yy * e->w;
                for (int xx = 0; xx < e->w; xx++) {
                    int tx = px + xx;                    // column in framebuffer space
                    if (tx < 0 || tx >= fbw) continue;   // column outside the framebuffer
                    uint8_t cov = row[xx];
                    if (cov) blend(fb, (size_t)ty*fbw + tx, color, cov);
                }
            }
        }
        pen += e->advance;                               // advance the pen
    }
    (void)scale;                                         // silence unused warning
    return pen - x;                                      // total advance width used
}
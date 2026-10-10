// source/libraries/ui/ui_text.h
// Public declarations for Apex text rendering
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_TEXT_H
#define UI_TEXT_H
#include <stdint.h>
#include <stddef.h>

// opaque loaded truetype font
typedef struct UiFont UiFont;

// decode a ttf from memory
UiFont* ui_font_load_memory(const uint8_t* data, size_t len);

// load the platform's default ui font
UiFont* ui_font_load_system(void);

// release a font and its glyph cache
void ui_font_free(UiFont* f);

// true when the font pointer is usable
int ui_font_has(UiFont* f);

// advance width in pixels
float ui_text_measure(UiFont* f, const char* s, int len, float size);

// count of utf-8 codepoints in a buffer
int ui_text_codepoints(const char* s, int len);

// rasterize into a framebuffer
float ui_text_draw(UiFont* f, uint32_t* fb, int fbw, int fbh,
                   float x, float baseline, const char* s, int len,
                   float size, uint32_t color);

// count codepoints in a byte range
int ui_utf8_len(const char* s, int byte_len);

// offset of the previous codepoint
int ui_utf8_prev(const char* s, int off);

// offset of the next codepoint
int ui_utf8_next(const char* s, int off, int byte_len);

#endif
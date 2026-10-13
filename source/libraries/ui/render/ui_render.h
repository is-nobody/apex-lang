// source/libraries/ui/render/ui_render.h
// Painter and renderer declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_RENDER_H
#define UI_RENDER_H

#include "ui_internal.h"

// binds the painter to a framebuffer and clears the clip stack
void ui_painter_init(uint32_t* pixels, int w, int h);

// fills an axis-aligned rectangle with the given color
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c);

// clears the whole bound framebuffer to an opaque color using streaming stores
void ui_painter_clear(uint32_t c);

// fills a rounded rectangle with antialiased corners
void ui_painter_fill_round(int x, int y, int w, int h, int r, uint32_t c);

// strokes a 1px axis-aligned rectangle outline
void ui_painter_stroke_rect(int x, int y, int w, int h, uint32_t c);

// strokes a 1px rounded rectangle outline with antialiased corners
void ui_painter_stroke_round(int x, int y, int w, int h, int r, uint32_t c);

// pushes a scissor rect, intersected with the current top of the stack
void ui_painter_push_clip(int x, int y, int w, int h);

// pops the most recent scissor rect
void ui_painter_pop_clip(void);

// clears the whole clip stack
void ui_painter_reset_clip(void);

// releases the corner cache; called from ui_painter_shutdown
void ui_painter_shutdown(void);

// draws a single line of text at the given baseline position
void ui_draw_text_at(UiContext* ctx, float x, float baseline,
                     const char* s, int len, float size, uint32_t color);

// draws the 9x6 checkmark at (ox, oy) -- shared with the renderer
void blit_checkmark_shared(int ox, int oy, uint32_t c);

// draws the 9x9 close glyph at (ox, oy) -- shared with renderers
void blit_close_x_shared(int ox, int oy, uint32_t c);

// draws the custom titlebar into the current framebuffer
void ui_render_titlebar(UiContext* ctx);

// draws the tree into the current framebuffer
void ui_render_frame(UiContext* ctx);

// repaints only the given damage rectangles
void ui_render_frame_damage(UiContext* ctx, const DamageRect* rects, int n);

// draws the currently open modal dialog on top of everything else
void ui_render_dialog(UiContext* ctx);

// paints one notification into its own framebuffer and blits
void ui_render_notification(UiNotification* n);

#endif
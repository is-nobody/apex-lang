// source/libraries/ui/ui_window.h
// Public window backend declarations for Apex ui
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_WINDOW_H
#define UI_WINDOW_H
#include <stdbool.h>
#include <stdint.h>

typedef struct UiWindow UiWindow;                        // opaque native window handle

typedef enum {
    UW_EV_NONE = 0,                                      // no event
    UW_EV_CLOSE,                                         // window close requested
    UW_EV_RESIZE,                                        // client area resized
    UW_EV_EXPOSE,                                        // client area needs repainting
    UW_EV_KEYDOWN,                                       // key pressed, possibly with text
    UW_EV_MOUSEMOVE,                                     // pointer moved
    UW_EV_MOUSEDOWN,                                     // mouse button pressed
    UW_EV_MOUSEUP,                                       // mouse button released
    UW_EV_SCROLL,                                        // wheel scrolled
} UiWinEventKind;                                        // native event kind discriminator

typedef struct {
    UiWinEventKind kind;                                 // which kind of event this is
    int x, y, w, h;                                      // pointer position or new size, depending on kind
    int key;                                             // apex keycode for key events
    int button;                                          // mouse button number for click events
    int mods;                                            // modifier bitmask for key events
    double scroll_dy;                                    // wheel delta for scroll events
    char utf8[8];                                        // utf-8 text produced by the key, if any
    int  utf8_len;                                       // byte length of utf8 payload
} UiWinEvent;                                            // a single native window event

// host hook invoked on repaint
typedef void (*UiPaintCallback)(void* user, int w, int h);

// create and show a native window; decorated=false strips system chrome
UiWindow* ui_window_create(int w, int h, const char* title, bool decorated);

// destroy a native window
void ui_window_destroy(UiWindow* w);

// update the window title
void ui_window_set_title(UiWindow* w, const char* title);

// install a paint hook
void ui_window_set_paint_callback(UiWindow* w, UiPaintCallback cb, void* user);

// framebuffer the painter should write into
uint32_t* ui_window_pixels(UiWindow* w);

// current client width in pixels
int ui_window_width(UiWindow* w);

// current client height in pixels
int ui_window_height(UiWindow* w);

// request a new client size
void ui_window_resize(UiWindow* w, int nw, int nh);

// blit the framebuffer to the screen
void ui_window_present(UiWindow* w);

// blit only the given rects
void ui_window_present_rects(UiWindow* w, const int* quads, int n);

// block until input, up to the timeout
bool ui_window_wait(UiWindow* w, int timeout_ms);

// pop the next pending event
bool ui_window_poll(UiWindow* w, UiWinEvent* out);

// true while the window is still open
bool ui_window_is_open(UiWindow* w);

// ask the window to close
void ui_window_close(UiWindow* w);

// true when the native window is currently maximized
bool ui_window_is_maximized(UiWindow* w);

// iconify / minimize the window
void ui_window_minimize(UiWindow* w);

// toggle between maximized and restored
void ui_window_toggle_maximize(UiWindow* w);

// clamp the window to a minimum client size
void ui_window_set_min_size(UiWindow* w, int min_w, int min_h);

// begin an interactive window-manager move (titlebar drag)
void ui_window_begin_drag(UiWindow* w);

// begin an interactive resize from the given edge
void ui_window_begin_resize(UiWindow* w, const char* edge);

// set a resize cursor hint while hovering a border, or null for default
void ui_window_set_resize_cursor(UiWindow* w, const char* edge);

// place a utf-8 string on the system clipboard; owns an internal copy
void ui_window_clipboard_set(UiWindow* w, const char* text, int len);

// fetch the current clipboard contents as utf-8
bool ui_window_clipboard_get(UiWindow* w, char** out_text, int* out_len);

// create a frameless, always-on-top popup for notifications
UiWindow* ui_window_create_popup(int x, int y, int w, int h, const char* title);

// move a window to the given screen position
void ui_window_move(UiWindow* w, int x, int y);

// fetch the work area of the monitor the window sits on
void ui_window_work_area(UiWindow* w, int* ox, int* oy, int* ow, int* oh);

// ask the host to repaint the window
void ui_window_invalidate(UiWindow* w);

#endif
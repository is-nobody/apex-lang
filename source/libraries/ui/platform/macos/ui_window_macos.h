// source/libraries/ui/platform/macos/ui_window_macos.h
// Internal macOS/Cocoa backend declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_WINDOW_MACOS_H
#define UI_WINDOW_MACOS_H

#ifdef __APPLE__

#include "ui_window.h"
#include "ui_theme.h"
#include <stdbool.h>
#include <stdint.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <objc/runtime.h>
#include <objc/message.h>

// foundation's NSInteger is a typedef for long on every 64-bit apple target
typedef long NSInteger;

// foundation's NSUInteger is a typedef for unsigned long on every 64-bit apple target
typedef unsigned long NSUInteger;

// NSPoint-shaped helper matching the ABI of the real NSPoint (2 doubles x, y)
typedef struct { double x, y; } ApexNSPoint;

// NSRect-shaped helper matching the ABI of the real NSRect (4 doubles in order x, y, w, h)
typedef struct { double x, y, w, h; } ApexNSRect;

// platform window state for the macos backend
struct UiWindow {
    id       ns_app;                 // nsapplication shared instance
    id       ns_window;              // nswindow, subclassed for borderless key windows
    id       ns_view;                // nsview, subclassed and layer-backed
    id       ns_layer;               // calayer backing the view
    id       ns_delegate;            // apexuiwindowdelegate (windowshouldclose etc.)

    uint32_t* pixels;                // owned pixel buffer (bgra, non-premultiplied)
    int       w, h;                  // visible window size in pixels
    int       buf_w, buf_h;          // pixel pitch of the buffer (>= w, h)
    int       point_w;               // requested point size (== pixel size for us)
    int       point_h;
    double    backing_scale;         // always 1.0 for now

    CGImageRef cg_image;             // image cached for the current frame

    bool     open;                   // false once closed
    bool     decorated;              // false = custom titlebar
    bool     is_popup;               // true for frameless notification popups
    bool     maximized;              // cached maximize state
    bool     fullscreen;             // cached fullscreen state

    int      min_w, min_h;           // minimum client size, 0 = unset

    bool       pending_set;          // whether pending is populated
    UiWinEvent pending;              // pending event queued by the delegate

    int      mods;                   // last modifier bitmask

    char*    clipboard_text;         // owned copy of what we last put on the pasteboard
    int      clipboard_len;          // length of that copy

    const char* cursor_edge;         // last edge passed to set_resize_cursor, null = arrow

    UiPaintCallback paint_cb;        // host paint callback, or null
    void*           paint_user;      // opaque pointer passed to the callback

    uint32_t pending_high_surrogate; // surrogate pair carry between key events
};

// encode a utf-32 codepoint into utf-8; returns bytes written
int macos_encode_utf8(uint32_t cp, char* out);

// translate a macos virtual keycode into an apex keysym (x11-style)
int macos_map_keycode(unsigned short keycode);

// lazily create the apexuiwindow runtime subclass
Class macos_apex_window_class(void);

// lazily create the apexuiview runtime subclass
Class macos_apex_view_class(void);

// lazily create the apexuiwindowdelegate runtime subclass
Class macos_apex_delegate_class(void);

// initialise the shared nsapplication on first use
int macos_init_app(void);

// convert a utf-8 c string into a fresh nsstring
id macos_nsstring_from_utf8(const char* s);

// convert an nsstring into a freshly allocated utf-8 c string
char* macos_utf8_from_nsstring(id nsstr);

#endif
#endif
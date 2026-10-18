// source/libraries/ui/platform/linux/ui_window_linux.h
// Internal X11 backend declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_WINDOW_LINUX_H
#define UI_WINDOW_LINUX_H

#ifndef _WIN32

#include "ui_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/sync.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdbool.h>
#include <stdint.h>

// platform window state for the x11 backend
struct UiWindow {
    Display* dpy;         // connection to the x server
    int      screen;      // default screen index
    Window   win;         // x11 window id
    GC       gc;          // graphics context for blits
    Atom     wm_delete;   // WM_DELETE_WINDOW protocol atom

    // EWMH resize synchronization (_NET_WM_SYNC_REQUEST)
    Atom         sync_request;       // _NET_WM_SYNC_REQUEST
    Atom         sync_counter_atom;  // _NET_WM_SYNC_REQUEST_COUNTER
    XSyncCounter sync_counter;       // server-side counter object
    XSyncValue   sync_value;         // value the WM is currently waiting for
    bool         sync_pending;       // true between request and ack

    XIM      im;          // input method, null when unavailable
    XIC      ic;          // input context, null when unavailable

    Atom     clipboard_atom;  // CLIPBOARD selection
    Atom     utf8_atom;       // UTF8_STRING target
    Atom     targets_atom;    // TARGETS target

    char*    clipboard_text;  // owned copy of the string we currently own
    int      clipboard_len;   // length of that copy

    XEvent*  saved;           // events displaced by blocking clipboard waits
    int      saved_count;     // number of saved events
    int      saved_cap;       // capacity of the saved-event array

    int      w, h;        // window size in pixels
    int      buf_w, buf_h; // pixel pitch of the shm/ximage backing (>= w, h)
    int      depth;       // visual depth: 24 for the main window, 32 for ARGB popups
    bool     open;        // false once the window is closed
    uint32_t* pixels;     // fallback cpu-side framebuffer
    XImage*  img;         // ximage presented to the server
    bool     shm_ok;      // true when shm backing is active
    XShmSegmentInfo shm;  // shm segment info when shm is active
    int      mods;        // last observed modifier bitmask

    bool     maximized;              // cached maximize state
    bool     fullscreen;             // cached fullscreen state
    bool     decorated;              // false when we draw our own titlebar
    int      min_w;                  // minimum client width from wm hints
    int      min_h;                  // minimum client height from wm hints
    int      last_root_x;            // last pointer position in root coords
    int      last_root_y;
    Cursor   resize_cursor[9];       // cached resize cursors, index 0 = unused
    const char* last_cursor_edge;    // last edge passed to set_resize_cursor, null = arrow
};

// swallow x errors that can occur during optional shm setup or clipboard exchange
int xerr(Display* d, XErrorEvent* e);

// stores an event back into the window's queue so poll can re-deliver it later
void push_saved_event(UiWindow* w, XEvent* ev);

// pops the next event, preferring anything previously saved
bool pop_event(UiWindow* w, XEvent* out);

// releases the current ximage and any shm segment backing it
void destroy_image(UiWindow* w);

// answers a SelectionRequest by pasting our clipboard text into the requestor's property
void handle_selection_request(UiWindow* w, XSelectionRequestEvent* req);

#endif
#endif
// source/libraries/ui/platform/windows/ui_window_windows.h
// Internal Win32 backend declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_WINDOW_WINDOWS_H
#define UI_WINDOW_WINDOWS_H

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "ui_window.h"
#include "ui_theme.h"
#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdbool.h>

// platform window state for the win32 backend
struct UiWindow {
    HWND      hwnd;                    // native window handle
    HDC       mem_dc;                  // memory dc for offscreen drawing
    HBITMAP   hbm;                     // dib section backing the memory dc
    void*     dib_bits;                // raw pixels of the dib section
    int       w, h;                    // current window size in pixels
    int       buf_w, buf_h;            // pixel pitch of the dib backing (>= w, h)
    int       pending_w;               // deferred resize width
    int       pending_h;               // deferred resize height
    bool      open;                    // false once the window is closed
    bool      decorated;               // false when we draw our own titlebar
    int       min_w;                   // minimum client width, 0 = unset
    int       min_h;                   // minimum client height, 0 = unset
    int       mods;                    // last observed modifier bitmask
    WCHAR     pending_high_surrogate;  // surrogate pair carry between WM_CHARs
    int       pending_vk;              // virtual key held for a follow-up WM_CHAR
    int       pending_mods;            // modifier state held for the same

    bool       is_popup;               // true for frameless notification popups
    bool       fullscreen;             // true while in fullscreen mode
    DWORD      fs_style;               // saved window style before fullscreen
    WINDOWPLACEMENT fs_placement;      // saved window placement before fullscreen
    UiWinEvent pending;                // pending event for *this* window
    bool       pending_set;            // whether pending is populated

    UiPaintCallback paint_cb;          // host paint callback, or null
    void*           paint_user;        // opaque pointer passed to the callback

    const char* cursor_override;       // active resize cursor name, null for default
};

// encodes one unicode codepoint as utf-8 into out, returns the byte count
int encode_utf8(uint32_t cp, char* out);

// maps a win32 virtual key to the apex keycode (x11-style keysyms)
int map_vk_to_keysym(int vk);

// reads the current shift/ctrl/alt state into the apex modifier bitmask
int current_mods(void);

// asks dwm for rounded corners; no-op on windows 10
void enable_round_corners(HWND hwnd);

// releases the memory dc, dib section, and cpu buffer
void free_buffer_resources(UiWindow* w);

// allocates a memory dc and dib section sized to nw by nh
bool alloc_buffer_resources(UiWindow* w, int nw, int nh);

// applies a deferred resize recorded by WM_SIZE
void apply_pending_resize(UiWindow* w);

// window procedure: translates native messages into pending ui events
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
#endif
// source/libraries/ui/ui_window_windows.c
// Implementation of Win32 window backend for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "ui_window.h"
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

// platform window state for the win32 backend
struct UiWindow {
    HWND      hwnd;                    // native window handle
    HDC       mem_dc;                  // memory dc for offscreen drawing
    HBITMAP   hbm;                     // dib section backing the memory dc
    void*     dib_bits;                // raw pixels of the dib section
    uint32_t* pixels;                  // cpu-side framebuffer used by the painter
    int       w, h;                    // current framebuffer size
    int       pending_w;               // deferred resize width
    int       pending_h;               // deferred resize height
    bool      open;                    // false once the window is closed
    int       mods;                    // last observed modifier bitmask
    WCHAR     pending_high_surrogate;  // surrogate pair carry between WM_CHARs
    int       pending_vk;              // virtual key held for a follow-up WM_CHAR
    int       pending_mods;            // modifier state held for the same

    UiPaintCallback paint_cb;          // host paint callback, or null
    void*           paint_user;        // opaque pointer passed to the callback
};

// single slot for the next event
static UiWinEvent g_pending;

// true when the slot is populated
static bool       g_pending_set = false;

// encodes one unicode codepoint as utf-8 into out, returns the byte count
static int encode_utf8(uint32_t cp, char* out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }      // 1-byte ascii
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));               // 2-byte sequence
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));              // 3-byte sequence
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));                  // 4-byte sequence
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

// maps a win32 virtual key to the apex keycode (x11-style keysyms)
static int map_vk_to_keysym(int vk) {
    switch (vk) {
        case VK_BACK:   return 0xFF08;                   // backspace
        case VK_TAB:    return 0xFF09;                   // tab
        case VK_RETURN: return 0xFF0D;                   // enter
        case VK_ESCAPE: return 0xFF1B;                   // escape
        case VK_HOME:   return 0xFF50;                   // home
        case VK_LEFT:   return 0xFF51;                   // left arrow
        case VK_UP:     return 0xFF52;                   // up arrow
        case VK_RIGHT:  return 0xFF53;                   // right arrow
        case VK_DOWN:   return 0xFF54;                   // down arrow
        case VK_PRIOR:  return 0xFF55;                   // page up
        case VK_NEXT:   return 0xFF56;                   // page down
        case VK_END:    return 0xFF57;                   // end
        case VK_DELETE: return 0xFFFF;                   // delete
        case VK_F1:     return 0xFFBE;                   // function keys
        case VK_F2:     return 0xFFBF;
        case VK_F3:     return 0xFFC0;
        case VK_F4:     return 0xFFC1;
        case VK_F5:     return 0xFFC2;
        case VK_F6:     return 0xFFC3;
        case VK_F7:     return 0xFFC4;
        case VK_F8:     return 0xFFC5;
        case VK_F9:     return 0xFFC6;
        case VK_F10:    return 0xFFC7;
        case VK_F11:    return 0xFFC8;
        case VK_F12:    return 0xFFC9;
    }
    if (vk >= 'A' && vk <= 'Z') return vk;               // letters pass through
    if (vk >= '0' && vk <= '9') return vk;               // digits pass through
    if (vk == VK_SPACE) return 0x20;                     // space
    return vk;                                           // unknown: raw vk value
}

// reads the current shift/ctrl/alt state into the apex modifier bitmask
static int current_mods(void) {
    int mods = 0;
    if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= 1;     // shift
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= 2;     // ctrl
    if (GetKeyState(VK_MENU)    & 0x8000) mods |= 4;     // alt
    return mods;
}

// releases the memory dc, dib section, and cpu buffer
static void free_buffer_resources(UiWindow* w) {
    if (w->mem_dc) { DeleteDC(w->mem_dc);  w->mem_dc = NULL; }  // delete the memory dc
    if (w->hbm)    { DeleteObject(w->hbm); w->hbm    = NULL; }  // delete the dib section
    w->dib_bits = NULL;                                  // dangling bits pointer cleared
    if (w->pixels) { free(w->pixels);      w->pixels = NULL; }  // free the cpu buffer
}

// allocates a memory dc and dib section sized to nw by nh
static bool alloc_buffer_resources(UiWindow* w, int nw, int nh) {
    uint32_t* new_pixels = (uint32_t*)calloc((size_t)nw * (size_t)nh, 4);  // cpu buffer
    if (!new_pixels) return false;                       // allocation failed

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));                        // zero the header
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = nw;                    // width in pixels
    bmi.bmiHeader.biHeight      = -nh;                   // negative for top-down rows
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;                    // 32 bits per pixel
    bmi.bmiHeader.biCompression = BI_RGB;                // uncompressed

    HDC screen = GetDC(NULL);                            // reference dc for compatibility
    HDC new_dc = CreateCompatibleDC(screen);             // memory dc
    ReleaseDC(NULL, screen);
    if (!new_dc) { free(new_pixels); return false; }     // dc creation failed

    void* new_bits = NULL;                               // dib raw bits pointer
    HBITMAP new_hbm = CreateDIBSection(new_dc, &bmi, DIB_RGB_COLORS,
                                       &new_bits, NULL, 0);
    if (!new_hbm) {                                      // dib creation failed
        DeleteDC(new_dc);
        free(new_pixels);
        return false;
    }
    SelectObject(new_dc, new_hbm);                       // select the dib into the dc

    w->pixels   = new_pixels;                            // publish all four resources
    w->mem_dc   = new_dc;
    w->hbm      = new_hbm;
    w->dib_bits = new_bits;
    w->w        = nw;
    w->h        = nh;
    return true;
}

// applies a deferred resize recorded by WM_SIZE
static void apply_pending_resize(UiWindow* w) {
    if (w->pending_w <= 0 || w->pending_h <= 0) return;  // nothing pending

    int nw = w->pending_w;
    int nh = w->pending_h;
    w->pending_w = 0;                                    // consume the request
    w->pending_h = 0;

    if (nw == w->w && nh == w->h && w->pixels) return;   // unchanged

    uint32_t* old_pixels = w->pixels;                    // save old resources for rollback
    HDC       old_dc     = w->mem_dc;
    HBITMAP   old_hbm    = w->hbm;
    void*     old_bits   = w->dib_bits;
    int       old_w = w->w, old_h = w->h;

    w->pixels   = NULL;                                  // clear so alloc starts fresh
    w->mem_dc   = NULL;
    w->hbm      = NULL;
    w->dib_bits = NULL;

    if (!alloc_buffer_resources(w, nw, nh)) {            // alloc failed: restore old state
        w->pixels   = old_pixels;
        w->mem_dc   = old_dc;
        w->hbm      = old_hbm;
        w->dib_bits = old_bits;
        w->w        = old_w;
        w->h        = old_h;
        return;
    }

    if (old_dc)     DeleteDC(old_dc);                    // release the old resources
    if (old_hbm)    DeleteObject(old_hbm);
    if (old_pixels) free(old_pixels);
}

// window procedure: translates native messages into pending ui events
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UiWindow* w = (UiWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);  // per-window state

    if (msg == WM_NCCREATE) {                            // stash the window pointer on creation
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    if (!w) return DefWindowProcW(hwnd, msg, wp, lp);    // not yet attached: pass through

    switch (msg) {
        case WM_CLOSE:                                   // user or system close request
            g_pending.kind = UW_EV_CLOSE;
            g_pending_set = true;
            w->open = false;
            return 0;

        case WM_DESTROY:                                 // window being destroyed
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND:
            return 1;                                    // suppress flicker: we paint the whole client

        case WM_PAINT: {                                 // client area needs painting
            ValidateRect(hwnd, NULL);
            if (w->paint_cb) {
                w->paint_cb(w->paint_user, w->w, w->h);  // let the host redraw into the buffer
            } else {
                g_pending.kind = UW_EV_EXPOSE;           // otherwise ask the renderer to redraw
                g_pending.w = w->w;
                g_pending.h = w->h;
                g_pending_set = true;
            }
            return 0;
        }

        case WM_SIZE:                                    // client area resized
            if (wp != SIZE_MINIMIZED) {
                int nw = LOWORD(lp);                     // new client width
                int nh = HIWORD(lp);                     // new client height
                if (nw > 0 && nh > 0 && (nw != w->w || nh != w->h)) {
                    w->pending_w = nw;                   // defer the actual realloc until safe
                    w->pending_h = nh;
                    g_pending.kind = UW_EV_RESIZE;
                    g_pending.w = nw;
                    g_pending.h = nh;
                    g_pending_set = true;

                    InvalidateRect(hwnd, NULL, FALSE);   // schedule a repaint
                    UpdateWindow(hwnd);
                }
            }
            return 0;

        case WM_ENTERSIZEMOVE:
            return 0;                                    // no special handling during interactive resize

        case WM_EXITSIZEMOVE:                            // interactive resize finished
            if (w->paint_cb) {
                apply_pending_resize(w);                 // now safe to reallocate
                w->paint_cb(w->paint_user, w->w, w->h);
            }
            return 0;

        case WM_KEYDOWN:                                 // key pressed (or repeated)
        case WM_SYSKEYDOWN: {
            int vk = (int)wp;                            // virtual key code
            int mods = current_mods();                   // current modifiers

            if (msg == WM_SYSKEYDOWN &&                  // leave alt-f4, alt-space, etc to the system
                (vk == VK_F4 || vk == VK_SPACE || vk == VK_TAB || vk == VK_ESCAPE)) {
                return DefWindowProcW(hwnd, msg, wp, lp);
            }

            MSG peek;
            if (PeekMessageW(&peek, hwnd, WM_CHAR, WM_CHAR, PM_NOREMOVE)) {  // a WM_CHAR will follow
                w->pending_vk = vk;                      // hold the key for that message
                w->pending_mods = mods;
                return 0;
            }

            g_pending.kind = UW_EV_KEYDOWN;              // no text: emit a bare key event
            g_pending.key = map_vk_to_keysym(vk);
            g_pending.mods = mods;
            g_pending.utf8_len = 0;
            g_pending_set = true;
            return 0;
        }

        case WM_CHAR: {                                  // character produced by a key
            WCHAR wc = (WCHAR)wp;
            int vk = w->pending_vk;                      // key held from WM_KEYDOWN
            int mods = w->pending_mods;
            w->pending_vk = 0;
            w->pending_mods = 0;

            char utf8[8];
            int utf8_len = 0;

            if (wc >= 0xD800 && wc <= 0xDBFF) {          // high surrogate: wait for low
                w->pending_high_surrogate = wc;
                return 0;
            } else if (wc >= 0xDC00 && wc <= 0xDFFF) {   // low surrogate: combine if paired
                if (w->pending_high_surrogate) {
                    uint32_t cp = 0x10000
                                + (((uint32_t)w->pending_high_surrogate - 0xD800) << 10)
                                + ((uint32_t)wc - 0xDC00);
                    utf8_len = encode_utf8(cp, utf8);
                    w->pending_high_surrogate = 0;
                } else {
                    return 0;                            // stray low surrogate: discard
                }
            } else {                                     // bmp codepoint directly
                if (w->pending_high_surrogate) w->pending_high_surrogate = 0;  // broken pair: reset
                utf8_len = encode_utf8((uint32_t)wc, utf8);
            }
            if (utf8_len == 0) return 0;                 // nothing to emit

            g_pending.kind = UW_EV_KEYDOWN;              // emit the text event
            g_pending.key = vk ? map_vk_to_keysym(vk) : 0;
            g_pending.mods = mods;
            g_pending.utf8_len = utf8_len;
            memcpy(g_pending.utf8, utf8, utf8_len);
            g_pending_set = true;
            return 0;
        }

        case WM_MOUSEMOVE:                               // pointer moved
            g_pending.kind = UW_EV_MOUSEMOVE;
            g_pending.x = GET_X_LPARAM(lp);
            g_pending.y = GET_Y_LPARAM(lp);
            g_pending_set = true;
            return 0;

        case WM_LBUTTONDOWN:                             // left button pressed
            SetCapture(hwnd);                            // capture so drags outside still reach us
            g_pending.kind = UW_EV_MOUSEDOWN;
            g_pending.x = GET_X_LPARAM(lp);
            g_pending.y = GET_Y_LPARAM(lp);
            g_pending.button = 1;
            g_pending_set = true;
            return 0;

        case WM_LBUTTONUP:                               // left button released
            ReleaseCapture();
            g_pending.kind = UW_EV_MOUSEUP;
            g_pending.x = GET_X_LPARAM(lp);
            g_pending.y = GET_Y_LPARAM(lp);
            g_pending.button = 1;
            g_pending_set = true;
            return 0;

        case WM_RBUTTONDOWN:                             // right button pressed
            g_pending.kind = UW_EV_MOUSEDOWN;
            g_pending.x = GET_X_LPARAM(lp);
            g_pending.y = GET_Y_LPARAM(lp);
            g_pending.button = 3;
            g_pending_set = true;
            return 0;

        case WM_RBUTTONUP:                               // right button released
            g_pending.kind = UW_EV_MOUSEUP;
            g_pending.x = GET_X_LPARAM(lp);
            g_pending.y = GET_Y_LPARAM(lp);
            g_pending.button = 3;
            g_pending_set = true;
            return 0;

        case WM_MOUSEWHEEL: {                            // wheel scrolled
            int delta = GET_WHEEL_DELTA_WPARAM(wp);      // positive for wheel up
            POINT pt;
            pt.x = GET_X_LPARAM(lp);                     // screen coordinates
            pt.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &pt);                   // convert to client coordinates
            g_pending.kind = UW_EV_SCROLL;
            g_pending.x = pt.x;
            g_pending.y = pt.y;
            g_pending.scroll_dy = -(double)delta / 120.0;  // wheel up is negative in our convention
            g_pending_set = true;
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {                // reset to the arrow inside the client area
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_NCHITTEST:
            return DefWindowProcW(hwnd, msg, wp, lp);    // default hit testing
    }

    return DefWindowProcW(hwnd, msg, wp, lp);            // everything else: default handling
}

static const WCHAR kClassName[] = L"ApexUiWindowClass";  // registered class name

// creates a native window and its offscreen buffer
UiWindow* ui_window_create(int w, int h, const char* title) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");    // opt into dpi awareness if possible
    if (user32) {
        typedef BOOL (WINAPI *SetDPIAwareFn)(void);
        FARPROC raw = GetProcAddress(user32, "SetProcessDPIAware");
        if (raw) {
            SetDPIAwareFn fn;
            memcpy(&fn, &raw, sizeof(fn));               // avoid a cast between fn and data pointers
            fn();
        }
    }

    HINSTANCE hInst = GetModuleHandleW(NULL);            // module handle for the window class

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));                          // zero the class struct
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;          // full repaint on resize
    wc.lpfnWndProc   = wnd_proc;                         // our window procedure
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;                             // we paint the whole client area
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);                               // idempotent across runs

    RECT r = { 0, 0, w, h };                             // desired client size
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);  // expand to include chrome

    WCHAR wtitle[512];                                   // window title in wide chars
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "Apex UI", -1,
                        wtitle, (int)(sizeof(wtitle)/sizeof(wtitle[0])));

    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    if (!win) return NULL;
    win->w = 0;                                          // reported size until the buffer exists
    win->h = 0;

    HWND hwnd = CreateWindowExW(
        0, kClassName, wtitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInst, win);                         // pass win as lpCreateParams
    if (!hwnd) { free(win); return NULL; }               // creation failed
    win->hwnd = hwnd;

    if (!alloc_buffer_resources(win, w, h)) {            // allocate the offscreen buffer
        DestroyWindow(hwnd);
        free(win);
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOW);                           // show the window
    UpdateWindow(hwnd);                                  // trigger the first paint
    SetFocus(hwnd);                                      // give it keyboard focus

    win->open = true;
    return win;
}

// destroys the window and releases the offscreen buffer
void ui_window_destroy(UiWindow* w) {
    if (!w) return;                                      // null guard
    free_buffer_resources(w);                            // release the memory dc and dib
    if (w->hwnd) DestroyWindow(w->hwnd);                 // destroy the native window
    free(w);                                             // free the window struct
}

// updates the window title
void ui_window_set_title(UiWindow* w, const char* t) {
    WCHAR wt[512];                                       // title converted to wide chars
    MultiByteToWideChar(CP_UTF8, 0, t ? t : "", -1,
                        wt, (int)(sizeof(wt)/sizeof(wt[0])));
    SetWindowTextW(w->hwnd, wt);
}

// stores a paint callback the window can invoke on WM_PAINT
void ui_window_set_paint_callback(UiWindow* w, UiPaintCallback cb, void* user) {
    w->paint_cb = cb;
    w->paint_user = user;
}

// returns the current framebuffer, applying any deferred resize first
uint32_t* ui_window_pixels(UiWindow* w) {
    apply_pending_resize(w);                             // catch up on deferred resizes
    return w->pixels;
}

// current window width in pixels
int ui_window_width(UiWindow* w)  { return w->w; }

// current window height in pixels
int ui_window_height(UiWindow* w) { return w->h; }

// resizes the native window, which will trigger WM_SIZE
void ui_window_resize(UiWindow* w, int nw, int nh) {
    if (nw <= 0 || nh <= 0) return;                      // ignore degenerate sizes
    RECT r = { 0, 0, nw, nh };
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);  // expand for chrome
    SetWindowPos(w->hwnd, NULL, 0, 0,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// blits the cpu framebuffer into the dib section and to the client area
void ui_window_present(UiWindow* w) {
    if (!w->mem_dc || !w->dib_bits || !w->pixels) return;  // nothing to present

    RECT r;
    if (!GetClientRect(w->hwnd, &r)) return;             // client rect failed
    int cw = r.right - r.left;                           // client width
    int ch = r.bottom - r.top;                           // client height
    if (cw <= 0 || ch <= 0) return;                      // minimized or degenerate

    int copy_w = cw < w->w ? cw : w->w;                  // clip to the smaller of the two
    int copy_h = ch < w->h ? ch : w->h;
    if (copy_w <= 0 || copy_h <= 0) return;              // nothing to copy

    for (int y = 0; y < copy_h; y++) {                   // copy row by row into the dib
        memcpy((uint8_t*)w->dib_bits + (size_t)y * (size_t)w->w * 4,
               (uint8_t*)w->pixels   + (size_t)y * (size_t)w->w * 4,
               (size_t)copy_w * 4);
    }

    HDC hdc = GetDC(w->hwnd);                            // window dc
    BitBlt(hdc, 0, 0, copy_w, copy_h, w->mem_dc, 0, 0, SRCCOPY);  // blit the memory dc
    ReleaseDC(w->hwnd, hdc);
}

// waits for input or the timeout to expire
bool ui_window_wait(UiWindow* w, int timeout_ms) {
    (void)w;
    if (g_pending_set) return true;                      // already have an event
    DWORD ret = MsgWaitForMultipleObjects(0, NULL, FALSE,
                                          (DWORD)timeout_ms, QS_ALLINPUT);
    return ret == WAIT_OBJECT_0;                         // true when input is available
}

// translates the next native event into a ui window event
bool ui_window_poll(UiWindow* w, UiWinEvent* out) {
    out->kind = UW_EV_NONE;                              // default to nothing

    if (!g_pending_set && w->hwnd) {                     // synthesize a resize if the client changed
        RECT r;
        if (GetClientRect(w->hwnd, &r)) {
            int cw = r.right - r.left;
            int ch = r.bottom - r.top;
            if (cw > 0 && ch > 0 && (cw != w->w || ch != w->h)) {
                w->pending_w = cw;
                w->pending_h = ch;
                g_pending.kind = UW_EV_RESIZE;
                g_pending.w = cw;
                g_pending.h = ch;
                g_pending_set = true;
            }
        }
    }

    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {  // drain the message queue
        if (msg.message == WM_QUIT) {                    // application quit
            out->kind = UW_EV_CLOSE;
            w->open = false;
            return true;
        }
        TranslateMessage(&msg);                          // let windows synthesize WM_CHAR
        DispatchMessageW(&msg);                          // dispatch to our window procedure
        if (g_pending_set) {                             // window procedure produced an event
            *out = g_pending;
            g_pending_set = false;
            g_pending.kind = UW_EV_NONE;
            return true;
        }
    }

    if (g_pending_set) {                                 // event posted outside the loop
        *out = g_pending;
        g_pending_set = false;
        g_pending.kind = UW_EV_NONE;
        return true;
    }
    return false;                                        // nothing to report
}

// true while the window has not been closed
bool ui_window_is_open(UiWindow* w) { return w->open; }

// marks the window as closed and asks windows to close it
void ui_window_close(UiWindow* w) {
    w->open = false;
    if (w->hwnd) PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

#endif
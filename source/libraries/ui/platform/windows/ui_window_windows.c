// source/libraries/ui/platform/windows/ui_window_windows.c
// Win32 window core: create/destroy/resize/framebuffer for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32

#include "ui_window_windows.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const WCHAR kClassName[] = L"ApexUiWindowClass";  // registered class name

// encodes one unicode codepoint as utf-8 into out, returns the byte count
int encode_utf8(uint32_t cp, char* out) {
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
int map_vk_to_keysym(int vk) {
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
int current_mods(void) {
    int mods = 0;
    if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= 1;     // shift
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= 2;     // ctrl
    if (GetKeyState(VK_MENU)    & 0x8000) mods |= 4;     // alt
    return mods;
}

// asks dwm for rounded corners; no-op on windows 10
void enable_round_corners(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    FARPROC raw = GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (!raw) return;
    typedef HRESULT (WINAPI *Fn)(HWND, DWORD, LPCVOID, DWORD);
    Fn fn; memcpy(&fn, &raw, sizeof(fn));
    DWORD pref = 2;                                      // DWMWCP_ROUND
    fn(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &pref, sizeof(pref));
}

// releases the memory dc, dib section, and cpu buffer
void free_buffer_resources(UiWindow* w) {
    if (w->mem_dc) { DeleteDC(w->mem_dc);  w->mem_dc = NULL; }  // delete the memory dc
    if (w->hbm)    { DeleteObject(w->hbm); w->hbm    = NULL; }  // delete the dib section
    w->dib_bits = NULL;                                  // dangling bits pointer cleared
}

// allocates a memory dc and dib section sized to nw by nh
bool alloc_buffer_resources(UiWindow* w, int nw, int nh) {
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
    if (!new_dc) return false;                           // dc creation failed

    void* new_bits = NULL;                               // dib raw bits pointer
    HBITMAP new_hbm = CreateDIBSection(new_dc, &bmi, DIB_RGB_COLORS,
                                       &new_bits, NULL, 0);
    if (!new_hbm) {                                      // dib creation failed
        DeleteDC(new_dc);
        return false;
    }
    SelectObject(new_dc, new_hbm);                       // select the dib into the dc

    // the dib bits are the one and only framebuffer on win32. the caller
    // owns w/h and buf_w/buf_h: this function only (re)creates the dib.
    w->mem_dc   = new_dc;
    w->hbm      = new_hbm;
    w->dib_bits = new_bits;
    return true;
}

// applies a deferred resize recorded by WM_SIZE
void apply_pending_resize(UiWindow* w) {
    if (w->pending_w <= 0 || w->pending_h <= 0) return;  // nothing pending

    int nw = w->pending_w;
    int nh = w->pending_h;
    w->pending_w = 0;                                    // consume the request
    w->pending_h = 0;

    if (nw == w->w && nh == w->h && w->dib_bits) return; // unchanged

    w->w = nw; w->h = nh;                                // record the new visible size

    // the dib already covers the new size: keep it. the painter and
    // BitBlt both take buf_w as the row pitch, so a dib larger than the
    // window is harmless and the resize becomes a metadata-only update.
    if (w->dib_bits && nw <= w->buf_w && nh <= w->buf_h) return;

    // outgrew the dib: reallocate at 1.5x the request so a few more growth
    // steps land in slack rather than in another allocation
    int cap_w = nw * 3 / 2; if (cap_w < nw + 64) cap_w = nw + 64;
    int cap_h = nh * 3 / 2; if (cap_h < nh + 64) cap_h = nh + 64;

    HDC     old_dc   = w->mem_dc;                        // save old resources for rollback
    HBITMAP old_hbm  = w->hbm;
    void*   old_bits = w->dib_bits;
    int     old_bw = w->buf_w, old_bh = w->buf_h;

    w->mem_dc   = NULL;                                  // clear so alloc starts fresh
    w->hbm      = NULL;
    w->dib_bits = NULL;
    w->buf_w    = cap_w;
    w->buf_h    = cap_h;

    if (!alloc_buffer_resources(w, w->buf_w, w->buf_h)) {  // alloc failed: restore old state
        w->mem_dc   = old_dc;
        w->hbm      = old_hbm;
        w->dib_bits = old_bits;
        w->buf_w    = old_bw;
        w->buf_h    = old_bh;
        return;
    }

    if (old_dc)  DeleteDC(old_dc);                       // release the old resources
    if (old_hbm) DeleteObject(old_hbm);
}

// creates a native window and its offscreen buffer
UiWindow* ui_window_create(int w, int h, const char* title, bool decorated) {
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

    // both cases use the same underlying style
    DWORD style = WS_OVERLAPPEDWINDOW;

    RECT r = { 0, 0, w, h };                             // desired client size
    if (decorated)
        AdjustWindowRectEx(&r, style, FALSE, 0);         // expand for system chrome
    int win_w = r.right - r.left;                        // full window width
    int win_h = r.bottom - r.top;                        // full window height

    // centre the window inside the work area
    RECT work = { 0, 0, 0, 0 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int pos_x = work.left + (work.right  - work.left - win_w) / 2;
    int pos_y = work.top  + (work.bottom - work.top  - win_h) / 2;
    if (pos_x < work.left) pos_x = work.left;            // window wider than the work area
    if (pos_y < work.top)  pos_y = work.top;             // window taller than the work area

    WCHAR wtitle[512];                                   // window title in wide chars
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "Apex UI", -1,
                        wtitle, (int)(sizeof(wtitle)/sizeof(wtitle[0])));

    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    if (!win) return NULL;
    win->w = w;                                          // seed real size before the window exists
    win->h = h;
    win->buf_w = w; win->buf_h = h;                      // backing buffer starts at the window size
    win->decorated = decorated;                          // must be set before WM_NCCALCSIZE fires

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_COMPOSITED,              // composited: DWM holds the frame
        kClassName, wtitle,
        style,
        pos_x, pos_y,                                    // centred inside the work area
        win_w, win_h,
        NULL, NULL, hInst, win);                         // pass win as lpCreateParams
    if (!hwnd) { free(win); return NULL; }               // creation failed
    win->hwnd = hwnd;

    if (!decorated) enable_round_corners(hwnd);          // win11 rounded corners

    if (!alloc_buffer_resources(win, win->buf_w, win->buf_h)) {  // allocate the offscreen buffer
        DestroyWindow(hwnd);
        free(win);
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOW);                           // show the window
    if (!decorated) {
        // tell dwm the frame has changed so it drops the system caption
        // immediately rather than waiting for the first user resize
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
        enable_round_corners(hwnd);                      // re-apply win11 rounding after the swap
    }

    win->open = true;
    return win;
}

// destroys the window and releases the offscreen buffer
void ui_window_destroy(UiWindow* w) {
    if (!w) return;                                      // null guard
    if (w->hwnd) {
        // a layered popup must be hidden before it is destroyed
        if (w->is_popup) ShowWindow(w->hwnd, SW_HIDE);
        DestroyWindow(w->hwnd);
    }
    free_buffer_resources(w);                            // release the memory dc and dib
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
    if (w->hwnd) {                                       // force the first real paint now
        InvalidateRect(w->hwnd, NULL, FALSE);
        UpdateWindow(w->hwnd);
    }
}

// returns the current framebuffer, applying any deferred resize first
uint32_t* ui_window_pixels(UiWindow* w) {
    apply_pending_resize(w);                             // catch up on deferred resizes
    return (uint32_t*)w->dib_bits;                       // painter writes straight into the dib
}

// row pitch of the framebuffer in pixels; can exceed the window width
int ui_window_stride(UiWindow* w) {
    return w ? w->buf_w : 0;
}

// current window width in pixels
int ui_window_width(UiWindow* w)  { return w->w; }

// current window height in pixels
int ui_window_height(UiWindow* w) { return w->h; }

// resizes the native window, which will trigger WM_SIZE
void ui_window_resize(UiWindow* w, int nw, int nh) {
    if (nw <= 0 || nh <= 0) return;                      // ignore degenerate sizes
    if (!w->decorated) {
        SetWindowPos(w->hwnd, NULL, 0, 0, nw, nh,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }
    RECT r = { 0, 0, nw, nh };
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);  // expand for chrome
    SetWindowPos(w->hwnd, NULL, 0, 0,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// blits only the given rects into the dib and then to the client area
void ui_window_present_rects(UiWindow* w, const int* quads, int n) {
    if (!w || n <= 0) return;
    if (!w->mem_dc || !w->dib_bits) return;              // nothing to present

    if (w->is_popup) {
        uint32_t* px = (uint32_t*)w->dib_bits;
        for (int yy = 0; yy < w->h; yy++) {              // row by row: dib pitch is buf_w
            uint32_t* row = px + (size_t)yy * w->buf_w;
            for (int xx = 0; xx < w->w; xx++) {
                uint32_t c = row[xx];
                uint32_t a = (c >> 24) & 0xFF;
                if (a == 255) continue;
                if (a == 0) { row[xx] = 0; continue; }
                uint32_t r = (c >> 16) & 0xFF;
                uint32_t g = (c >>  8) & 0xFF;
                uint32_t b =  c        & 0xFF;
                r = (r * a + 127) / 255;
                g = (g * a + 127) / 255;
                b = (b * a + 127) / 255;
                row[xx] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }

        POINT pt_src = { 0, 0 };
        SIZE  sz     = { w->w, w->h };
        BLENDFUNCTION bf;
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat         = AC_SRC_ALPHA;
        UpdateLayeredWindow(w->hwnd, NULL, NULL, &sz,
                            w->mem_dc, &pt_src, 0, &bf, ULW_ALPHA);
        return;
    }

    RECT r;
    if (!GetClientRect(w->hwnd, &r)) return;             // client rect failed
    int cw = r.right - r.left;                           // client width
    int ch = r.bottom - r.top;                           // client height
    if (cw <= 0 || ch <= 0) return;                      // minimized or degenerate

    HDC hdc = GetDC(w->hwnd);                            // window dc

    for (int i = 0; i < n; i++) {
        const int* q = quads + i*4;
        int x = q[0], y = q[1], rw = q[2], rh = q[3];
        if (rw <= 0 || rh <= 0) continue;
        if (x < 0) { rw += x; x = 0; }                   // clip to the client rect
        if (y < 0) { rh += y; y = 0; }
        if (x + rw > cw) rw = cw - x;
        if (y + rh > ch) rh = ch - y;
        if (rw <= 0 || rh <= 0) continue;
        if (x + rw > w->w) rw = w->w - x;                // clip to the dib bounds too
        if (y + rh > w->h) rh = w->h - y;
        if (rw <= 0 || rh <= 0) continue;

        // the painter already wrote straight into the dib bits
        BitBlt(hdc, x, y, rw, rh, w->mem_dc, x, y, SRCCOPY);
    }

    ReleaseDC(w->hwnd, hdc);
}

// blits the cpu framebuffer into the dib section and to the client area
void ui_window_present(UiWindow* w) {
    if (!w) return;
    int quad[4] = { 0, 0, w->w, w->h };
    ui_window_present_rects(w, quad, 1);
}

// true while the window has not been closed
bool ui_window_is_open(UiWindow* w) { return w->open; }

// marks the window as closed and asks windows to close it
void ui_window_close(UiWindow* w) {
    w->open = false;
    if (w->hwnd) PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

// true when the window is currently maximized
bool ui_window_is_maximized(UiWindow* w) {
    return w && w->hwnd && IsZoomed(w->hwnd);
}

// iconifies the window
void ui_window_minimize(UiWindow* w) {
    if (w->hwnd) ShowWindow(w->hwnd, SW_MINIMIZE);
}

// toggles between maximized and restored
void ui_window_toggle_maximize(UiWindow* w) {
    if (!w->hwnd) return;
    if (IsZoomed(w->hwnd)) {
        ShowWindow(w->hwnd, SW_RESTORE);
    } else {
        // save the current placement so a subsequent restore returns here
        WINDOWPLACEMENT wp;
        memset(&wp, 0, sizeof(wp));
        wp.length = sizeof(wp);
        GetWindowPlacement(w->hwnd, &wp);
        wp.showCmd = SW_MAXIMIZE;
        SetWindowPlacement(w->hwnd, &wp);
    }
}

// clamps the window to a minimum client size via the min-track fields
void ui_window_set_min_size(UiWindow* w, int min_w, int min_h) {
    if (!w || !w->hwnd) return;
    if (min_w <= 0 || min_h <= 0) return;
    w->min_w = min_w;
    w->min_h = min_h;

    // ask the system to re-query WM_GETMINMAXINFO without changing the size
    SetWindowPos(w->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// hands off to the system modal move loop
void ui_window_begin_drag(UiWindow* w) {
    if (!w->hwnd) return;
    ReleaseCapture();
    SendMessageW(w->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

// hands off to the system modal resize loop
void ui_window_begin_resize(UiWindow* w, const char* edge) {
    if (!w->hwnd) return;
    WPARAM ht = HTBOTTOMRIGHT;
    if      (!strcmp(edge, "top-left"))     ht = HTTOPLEFT;
    else if (!strcmp(edge, "top"))          ht = HTTOP;
    else if (!strcmp(edge, "top-right"))    ht = HTTOPRIGHT;
    else if (!strcmp(edge, "right"))        ht = HTRIGHT;
    else if (!strcmp(edge, "bottom-right")) ht = HTBOTTOMRIGHT;
    else if (!strcmp(edge, "bottom"))       ht = HTBOTTOM;
    else if (!strcmp(edge, "bottom-left"))  ht = HTBOTTOMLEFT;
    else if (!strcmp(edge, "left"))         ht = HTLEFT;
    ReleaseCapture();
    SendMessageW(w->hwnd, WM_NCLBUTTONDOWN, ht, 0);
}

// swaps the cursor to the given edge's resize cursor
void ui_window_set_resize_cursor(UiWindow* w, const char* edge) {
    if (!w) return;
    w->cursor_override = edge;
    if (w->hwnd) {
        LPWSTR cursor_name = IDC_ARROW;
        if (edge) {
            if (!strcmp(edge, "left") || !strcmp(edge, "right") || !strcmp(edge, "split-h"))
                cursor_name = IDC_SIZEWE;
            else if (!strcmp(edge, "top") || !strcmp(edge, "bottom") || !strcmp(edge, "split-v"))
                cursor_name = IDC_SIZENS;
            else if (!strcmp(edge, "top-left") || !strcmp(edge, "bottom-right"))
                cursor_name = IDC_SIZENWSE;
            else if (!strcmp(edge, "top-right") || !strcmp(edge, "bottom-left"))
                cursor_name = IDC_SIZENESW;
        }
        SetCursor(LoadCursorW(NULL, cursor_name));
    }
}

// creates a frameless, always-on-top popup window used for notifications
UiWindow* ui_window_create_popup(int x, int y, int w, int h, const char* title) {
    HINSTANCE hInst = GetModuleHandleW(NULL);             // module handle for the window class

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));                           // zero the class struct
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;           // full repaint on resize
    wc.lpfnWndProc   = wnd_proc;                          // shared window procedure
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);      // default arrow cursor
    wc.hbrBackground = NULL;                              // we paint the whole client area
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);                                // idempotent across runs

    WCHAR wtitle[512];                                    // window title in wide chars
    MultiByteToWideChar(CP_UTF8, 0, title ? title : "", -1,
                        wtitle, (int)(sizeof(wtitle)/sizeof(wtitle[0])));

    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    if (!win) return NULL;                                // oom: bail out
    win->w = w; win->h = h;                               // seed real size before window exists
    win->buf_w = w; win->buf_h = h;                       // backing buffer starts at the window size
    win->decorated = false;                               // no system chrome
    win->is_popup  = true;                                // no resizing, no HTCAPTION

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        kClassName, wtitle,
        WS_POPUP,
        x, y, w, h,
        NULL, NULL, hInst, win);
    if (!hwnd) { free(win); return NULL; }                // creation failed
    win->hwnd = hwnd;

    if (!alloc_buffer_resources(win, win->buf_w, win->buf_h)) {  // allocate the offscreen buffer
        DestroyWindow(hwnd);                              // undo the window creation
        free(win);
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);                  // show without stealing focus
    UpdateWindow(hwnd);                                   // force the first paint now
    win->open = true;
    return win;
}

// moves a window to an absolute screen position
void ui_window_move(UiWindow* w, int x, int y) {
    if (!w || !w->hwnd) return;                           // null guard
    SetWindowPos(w->hwnd, HWND_TOPMOST, x, y, 0, 0,       // move only: keep size, order and focus
                 SWP_NOSIZE | SWP_NOACTIVATE);
}

// returns the monitor work area the window sits on
void ui_window_work_area(UiWindow* w, int* ox, int* oy, int* ow, int* oh) {
    HMONITOR mon = MonitorFromWindow(w && w->hwnd ? w->hwnd : NULL,
                                     MONITOR_DEFAULTTONEAREST);  // nearest monitor to the window
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));                           // zero the info struct
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfoW(mon, &mi)) {                      // query succeeded
        *ox = mi.rcWork.left;                             // work area left edge
        *oy = mi.rcWork.top;                              // work area top edge
        *ow = mi.rcWork.right  - mi.rcWork.left;          // work area width
        *oh = mi.rcWork.bottom - mi.rcWork.top;           // work area height
    } else {                                              // fall back to the primary screen
        *ox = 0; *oy = 0;
        *ow = GetSystemMetrics(SM_CXSCREEN);              // full screen width
        *oh = GetSystemMetrics(SM_CYSCREEN);              // full screen height
    }
}

// forces an immediate repaint of the window
void ui_window_invalidate(UiWindow* w) {
    if (!w || !w->hwnd) return;                           // null guard
    InvalidateRect(w->hwnd, NULL, FALSE);                 // mark the whole client area dirty
    UpdateWindow(w->hwnd);                                // deliver WM_PAINT synchronously
}

// no-op on win32; resize sync is x11-only
void ui_window_sync_ack(UiWindow* w) { (void)w; }

// toggles the window between fullscreen and windowed
void ui_window_toggle_fullscreen(UiWindow* w) {
    if (!w || !w->hwnd) return;                          // null guard

    if (!w->fullscreen) {                                // entering fullscreen
        w->fs_style = GetWindowLongW(w->hwnd, GWL_STYLE);
        w->fs_placement.length = sizeof(w->fs_placement);
        GetWindowPlacement(w->hwnd, &w->fs_placement);   // remember where we were

        MONITORINFO mi;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTONEAREST), &mi))
            return;                                      // no monitor info: bail out

        SetWindowLongW(w->hwnd, GWL_STYLE,               // strip the chrome bits
                       w->fs_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(w->hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        w->fullscreen = true;
    } else {                                             // leaving fullscreen
        SetWindowLongW(w->hwnd, GWL_STYLE, w->fs_style); // restore the saved style
        SetWindowPlacement(w->hwnd, &w->fs_placement);   // restore the saved placement
        SetWindowPos(w->hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        w->fullscreen = false;
    }
}

// true when the window is currently fullscreen
bool ui_window_is_fullscreen(UiWindow* w) {
    return w && w->fullscreen;
}

#endif
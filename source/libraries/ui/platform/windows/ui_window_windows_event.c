// source/libraries/ui/platform/windows/ui_window_windows_event.c
// Win32 window procedure + event polling for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32

#include "ui_window_windows.h"
#include "ui_theme.h"
#include <string.h>
#include <stdint.h>

// window procedure: translates native messages into pending ui events
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UiWindow* w = (UiWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);  // per-window state

    if (msg == WM_NCCREATE) {                            // stash the window pointer on creation
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    if (!w) return DefWindowProcW(hwnd, msg, wp, lp);    // not yet attached: pass through

    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp && w && !w->decorated) {
                // on entry rgrc[0] holds the proposed window rect, rgrc[1] the previous.
                // when maximized, windows expands the window rect by the invisible
                // resize border so dwm can compose shadows; pull the client back inside
                // so our content aligns to the actual monitor edges.
                if (IsZoomed(hwnd)) {
                    int bx = GetSystemMetrics(SM_CXSIZEFRAME) +
                             GetSystemMetrics(SM_CXPADDEDBORDER);
                    int by = GetSystemMetrics(SM_CYSIZEFRAME) +
                             GetSystemMetrics(SM_CXPADDEDBORDER);
                    NCCALCSIZE_PARAMS* np = (NCCALCSIZE_PARAMS*)lp;
                    np->rgrc[0].left   += bx;
                    np->rgrc[0].top    += by;
                    np->rgrc[0].right  -= bx;
                    np->rgrc[0].bottom -= by;
                }
                return 0;                                // we handled it: use rgrc[0] as-is
            }
            break;

        case WM_NCHITTEST: {
            if (!w || w->decorated || w->is_popup)
                return DefWindowProcW(hwnd, msg, wp, lp);
            if (IsZoomed(hwnd)) return HTCLIENT;         // no borders while maximized

            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            RECT rc; GetClientRect(hwnd, &rc);

            const int b = UI_TB_RESIZE;
            bool left   = pt.x < b;
            bool right  = pt.x >= rc.right  - b;
            bool top    = pt.y < b;
            bool bottom = pt.y >= rc.bottom - b;

            // corners and sides are resize handles
            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left)            return HTLEFT;
            if (right)           return HTRIGHT;
            if (bottom)          return HTBOTTOM;
            // top-middle is intentionally left as HTCAPTION so snap-to-maximize works

            if (pt.y < UI_TITLEBAR_H) {                  // inside the custom titlebar
                // the three window buttons live in the client; hit test them as HTCLIENT
                const int sz  = UI_TB_BTN_SZ;
                const int gap = UI_TB_BTN_GAP;
                const int pr  = UI_TB_BTN_PAD;
                int by  = (UI_TITLEBAR_H - sz) / 2;
                if (by < UI_TB_RESIZE) by = UI_TB_RESIZE;
                int bx3 = rc.right - pr - sz;
                int bx2 = bx3 - gap - sz;
                int bx1 = bx2 - gap - sz;
                if (pt.y >= by && pt.y < by + sz) {
                    if (pt.x >= bx1 && pt.x < bx1 + sz) return HTCLIENT;
                    if (pt.x >= bx2 && pt.x < bx2 + sz) return HTCLIENT;
                    if (pt.x >= bx3 && pt.x < bx3 + sz) return HTCLIENT;
                }
                return HTCLIENT;                         // let the client see menubar/drag clicks
            }
            return HTCLIENT;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            if (!w) return DefWindowProcW(hwnd, msg, wp, lp);  // before WM_NCCREATE: leave to system
            if (w->min_w > 0) mmi->ptMinTrackSize.x = w->min_w;
            if (w->min_h > 0) mmi->ptMinTrackSize.y = w->min_h;

            // let the os fill the defaults first, then clamp the maximize
            // box to the work area of the monitor the window is on
            DefWindowProcW(hwnd, msg, wp, lp);

            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(MONITORINFO);
            if (GetMonitorInfoW(mon, &mi)) {
                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top;
                mmi->ptMaxSize.x     = mi.rcWork.right  - mi.rcWork.left;
                mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
            }
            return 0;
        }

        case WM_CLOSE:                                   // user or system close request
            w->pending.kind = UW_EV_CLOSE;
            w->pending_set = true;
            w->open = false;
            return 0;

        case WM_DESTROY:                                 // window being destroyed
            // do not call postquitmessage(0): it is thread-wide
            return 0;

        case WM_ERASEBKGND:
            return 1;                                    // suppress flicker: we paint the whole client

        case WM_PAINT: {                                 // client area needs painting
            ValidateRect(hwnd, NULL);
            if (w->paint_cb) {
                w->paint_cb(w->paint_user, w->w, w->h);  // let the host redraw into the buffer
            } else if (!w->is_popup) {
                w->pending.kind = UW_EV_EXPOSE;          // otherwise ask the renderer to redraw
                w->pending.w = w->w;
                w->pending.h = w->h;
                w->pending_set = true;
            }
            return 0;
        }

        case WM_SIZE:                                    // client area resized
            if (wp != SIZE_MINIMIZED) {
                int nw = LOWORD(lp);                     // new client width
                int nh = HIWORD(lp);                     // new client height
                if (nw > 0 && nh > 0 && (nw != w->w || nh != w->h)) {
                    w->pending_w = nw;                   // stash the request
                    w->pending_h = nh;
                    apply_pending_resize(w);             // realloc now: we are not inside a paint
                    w->pending.kind = UW_EV_RESIZE;
                    w->pending.w = nw;
                    w->pending.h = nh;
                    w->pending_set = true;

                    // force an immediate, erasing repaint so the freshly exposed
                    // region is never shown uninitialised by dwm
                    RedrawWindow(hwnd, NULL, NULL,
                                 RDW_INVALIDATE | RDW_UPDATENOW |
                                 RDW_ERASE | RDW_ALLCHILDREN);
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

            w->pending.kind = UW_EV_KEYDOWN;             // no text: emit a bare key event
            w->pending.key = map_vk_to_keysym(vk);
            w->pending.mods = mods;
            w->pending.utf8_len = 0;
            w->pending_set = true;
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

            w->pending.kind = UW_EV_KEYDOWN;             // emit the text event
            w->pending.key = vk ? map_vk_to_keysym(vk) : 0;
            w->pending.mods = mods;
            w->pending.utf8_len = utf8_len;
            memcpy(w->pending.utf8, utf8, utf8_len);
            w->pending_set = true;
            return 0;
        }

        case WM_MOUSEMOVE:                               // pointer moved
            w->pending.kind = UW_EV_MOUSEMOVE;
            w->pending.x = GET_X_LPARAM(lp);
            w->pending.y = GET_Y_LPARAM(lp);
            w->pending_set = true;
            return 0;

        case WM_LBUTTONDOWN:                             // left button pressed
            SetCapture(hwnd);                            // capture so drags outside still reach us
            w->pending.kind = UW_EV_MOUSEDOWN;
            w->pending.x = GET_X_LPARAM(lp);
            w->pending.y = GET_Y_LPARAM(lp);
            w->pending.button = 1;
            w->pending_set = true;
            return 0;

        case WM_LBUTTONUP:                               // left button released
            ReleaseCapture();
            w->pending.kind = UW_EV_MOUSEUP;
            w->pending.x = GET_X_LPARAM(lp);
            w->pending.y = GET_Y_LPARAM(lp);
            w->pending.button = 1;
            w->pending_set = true;
            return 0;

        case WM_RBUTTONDOWN:                             // right button pressed
            w->pending.kind = UW_EV_MOUSEDOWN;
            w->pending.x = GET_X_LPARAM(lp);
            w->pending.y = GET_Y_LPARAM(lp);
            w->pending.button = 3;
            w->pending_set = true;
            return 0;

        case WM_RBUTTONUP:                               // right button released
            w->pending.kind = UW_EV_MOUSEUP;
            w->pending.x = GET_X_LPARAM(lp);
            w->pending.y = GET_Y_LPARAM(lp);
            w->pending.button = 3;
            w->pending_set = true;
            return 0;

        case WM_MOUSEWHEEL: {                            // wheel scrolled
            int delta = GET_WHEEL_DELTA_WPARAM(wp);      // positive for wheel up
            POINT pt;
            pt.x = GET_X_LPARAM(lp);                     // screen coordinates
            pt.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &pt);                   // convert to client coordinates
            w->pending.kind = UW_EV_SCROLL;
            w->pending.x = pt.x;
            w->pending.y = pt.y;
            w->pending.scroll_dy = -(double)delta / 120.0;  // wheel up is negative in our convention
            w->pending_set = true;
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {                // reset to the arrow inside the client area
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
    }

    return DefWindowProcW(hwnd, msg, wp, lp);            // everything else: default handling
}

// waits for input or the timeout to expire
bool ui_window_wait(UiWindow* w, int timeout_ms) {
    if (w->pending_set) return true;                     // already have an event
    DWORD ret = MsgWaitForMultipleObjects(0, NULL, FALSE,
                                          (DWORD)timeout_ms, QS_ALLINPUT);
    return ret == WAIT_OBJECT_0;                         // true when input is available
}

// translates the next native event into a ui window event
bool ui_window_poll(UiWindow* w, UiWinEvent* out) {
    out->kind = UW_EV_NONE;                              // default to nothing

    if (!w->pending_set && w->hwnd) {                    // synthesize a resize if the client changed
        RECT r;
        if (GetClientRect(w->hwnd, &r)) {
            int cw = r.right - r.left;
            int ch = r.bottom - r.top;
            if (cw > 0 && ch > 0 && (cw != w->w || ch != w->h)) {
                w->pending_w = cw;
                w->pending_h = ch;
                w->pending.kind = UW_EV_RESIZE;
                w->pending.w = cw;
                w->pending.h = ch;
                w->pending_set = true;
            }
        }
    }

    MSG msg;
    while (PeekMessageW(&msg, w->hwnd, 0, 0, PM_REMOVE)) {  // drain THIS window's queue only
        TranslateMessage(&msg);                          // let windows synthesize WM_CHAR
        DispatchMessageW(&msg);                          // dispatch to our window procedure
        if (w->pending_set) {                            // window procedure produced an event
            *out = w->pending;
            w->pending_set = false;
            w->pending.kind = UW_EV_NONE;
            return true;
        }
    }

    if (w->pending_set) {                                // event posted outside the loop
        *out = w->pending;
        w->pending_set = false;
        w->pending.kind = UW_EV_NONE;
        return true;
    }
    return false;                                        // nothing to report
}

#endif
// source/libraries/ui/platform/linux/ui_window_linux.c
// X11 window core: create/destroy/resize/framebuffer for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32
#include "ui_window_linux.h"
#include <sys/select.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

// swallow x errors that can occur during optional shm setup or clipboard exchange
int xerr(Display* d, XErrorEvent* e) { (void)d;(void)e; return 0; }

// stores an event back into the window's queue so poll can re-deliver it later
void push_saved_event(UiWindow* w, XEvent* ev) {
    if (w->saved_count >= w->saved_cap) {                // grow the saved-event array
        w->saved_cap = w->saved_cap ? w->saved_cap * 2 : 16;
        w->saved = (XEvent*)realloc(w->saved, sizeof(XEvent) * w->saved_cap);
    }
    w->saved[w->saved_count++] = *ev;                    // copy the full union
}

// pops the next event, preferring anything previously saved
bool pop_event(UiWindow* w, XEvent* out) {
    if (w->saved_count > 0) {                            // re-deliver a saved event
        *out = w->saved[0];
        memmove(&w->saved[0], &w->saved[1], sizeof(XEvent) * (w->saved_count - 1));
        w->saved_count--;
        return true;
    }
    if (XPending(w->dpy) > 0) {                          // otherwise pull from the server
        XNextEvent(w->dpy, out);
        return true;
    }
    return false;                                        // nothing left
}

// allocates the fallback framebuffer on 2 mb-aligned pages
static uint32_t* alloc_framebuffer(int w, int h) {
    size_t need = (size_t)w * (size_t)h * 4;
    size_t round = (need + (2u << 20) - 1) & ~((size_t)(2u << 20) - 1);
    void* p = mmap(NULL, round, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) {
        // fall back to plain calloc if mmap fails
        return (uint32_t*)calloc((size_t)w * (size_t)h, 4);
    }
#ifdef MADV_HUGEPAGE
    madvise(p, round, MADV_HUGEPAGE);        // best-effort; ignored if unsupported
#endif
    return (uint32_t*)p;
}

// releases a framebuffer allocated by alloc_framebuffer
static void free_framebuffer(uint32_t* p, int w, int h) {
    if (!p) return;
    size_t need = (size_t)w * (size_t)h * 4;
    size_t round = (need + (2u << 20) - 1) & ~((size_t)(2u << 20) - 1);
    // try munmap first; if the pointer is not mmap-backed it returns EINVAL
    if (munmap(p, round) != 0) {
        free(p);                             // must have come from calloc
    }
}

// creates and maps a new x11 window, preferring shared memory backing
UiWindow* ui_window_create(int w, int h, const char* title, bool decorated) {
    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    win->dpy = XOpenDisplay(NULL);                       // connect to the default display
    if (!win->dpy) { free(win); return NULL; }           // no display: give up
    win->screen = DefaultScreen(win->dpy);               // default screen
    win->depth  = DefaultDepth(win->dpy, win->screen);   // inherited visual depth
    win->decorated = decorated;                          // remember chrome preference
    win->w = w; win->h = h;
    win->shm.shmid = -1;                                 // sentinel: no live shm segment yet
    win->pixels = NULL;                                  // allocated below only if shm is unavailable

    XSetWindowAttributes a = {0};                        // window attribute set
    a.background_pixel = 0xFF1E1E1Eu;                    // match the ui theme background
    a.bit_gravity = NorthWestGravity;                    // anchor contents at the top-left
    win->win = XCreateWindow(win->dpy, RootWindow(win->dpy, win->screen),
        0,0,(unsigned)w,(unsigned)h,0, CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel|CWBitGravity, &a);
    XStoreName(win->dpy, win->win, title);               // set the window title
    XSelectInput(win->dpy, win->win,
        ExposureMask|KeyPressMask|StructureNotifyMask|
        ButtonPressMask|ButtonReleaseMask|PointerMotionMask|
        FocusChangeMask);                                // focus mask feeds xim
    win->wm_delete = XInternAtom(win->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(win->dpy, win->win, &win->wm_delete, 1);  // receive close requests

    if (!decorated) {
        // ask the wm to drop its own decorations; we draw a custom titlebar
        Atom motif = XInternAtom(win->dpy, "_MOTIF_WM_HINTS", False);
        struct {
            unsigned long flags;
            unsigned long functions;
            unsigned long decorations;
            long          input_mode;
            unsigned long status;
        } hints;
        hints.flags       = 2;           // MWM_HINTS_DECORATIONS
        hints.functions   = 0;
        hints.decorations = 0;           // no decorations
        hints.input_mode  = 0;
        hints.status      = 0;
        XChangeProperty(win->dpy, win->win, motif, motif, 32,
                        PropModeReplace, (unsigned char*)&hints, 5);
    }

    XMapWindow(win->dpy, win->win);                      // make the window visible
    win->gc = XCreateGC(win->dpy, win->win, 0, NULL);    // create the graphics context

    // clipboard atoms: interned once, reused for both ownership and target queries
    win->clipboard_atom = XInternAtom(win->dpy, "CLIPBOARD", False);
    win->utf8_atom      = XInternAtom(win->dpy, "UTF8_STRING", False);
    win->targets_atom   = XInternAtom(win->dpy, "TARGETS", False);

    // input method: open the xim connection and bind an input context to the
    // window. preedit is off because we have no inline composition rendering,
    // and status is off for the same reason. if any step fails we silently
    // fall back to ascii-only lookupstring, which is what the previous
    // implementation did unconditionally.
    win->im = XOpenIM(win->dpy, NULL, NULL, NULL);
    if (win->im) {
        win->ic = XCreateIC(win->im,
                            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, win->win,
                            XNFocusWindow, win->win,
                            NULL);
        if (win->ic) XSetICFocus(win->ic);               // assume focus until told otherwise
    }

    if (XShmQueryExtension(win->dpy)) {                  // try shared memory first
        win->shm.shmid = shmget(IPC_PRIVATE, (size_t)w*h*4, IPC_CREAT|0600);
        if (win->shm.shmid >= 0) {
            win->shm.shmaddr = (char*)shmat(win->shm.shmid, NULL, 0);
            if (win->shm.shmaddr != (char*)-1) {
                win->shm.readOnly = False;
                win->img = XShmCreateImage(win->dpy,
                    DefaultVisual(win->dpy, win->screen),
                    (unsigned)DefaultDepth(win->dpy, win->screen),
                    ZPixmap, NULL, &win->shm, (unsigned)w, (unsigned)h);
                if (win->img) {
                    win->img->data = win->shm.shmaddr;
                    int (*old)(Display*,XErrorEvent*) = XSetErrorHandler(xerr);  // suppress shm errors
                    XShmAttach(win->dpy, &win->shm);
                    XSync(win->dpy, False);              // flush and observe errors
                    XSetErrorHandler(old);               // restore the previous handler
                    win->shm_ok = true;
                }
            }
        }
    }
    if (!win->shm_ok) {                                  // fall back to a plain ximage
        win->pixels = alloc_framebuffer(w, h);           // 2 MB-aligned, THP-backed
        win->img = XCreateImage(win->dpy,
            DefaultVisual(win->dpy, win->screen),
            (unsigned)DefaultDepth(win->dpy, win->screen),
            ZPixmap, 0, (char*)win->pixels, (unsigned)w, (unsigned)h, 32, 0);
    }
    win->open = true;
    return win;
}

// releases the current ximage and any shm segment backing it
void destroy_image(UiWindow* w) {
    if (w->shm_ok) {                                     // detach from the server first
        XShmDetach(w->dpy, &w->shm);
        XSync(w->dpy, False);
        w->shm_ok = false;
    }
    if (w->img) { w->img->data = NULL; XDestroyImage(w->img); w->img = NULL; }  // free the ximage
    if (w->shm.shmaddr && w->shm.shmaddr != (char*)-1) shmdt(w->shm.shmaddr);   // detach our mapping
    if (w->shm.shmid >= 0) shmctl(w->shm.shmid, IPC_RMID, NULL);                // mark the segment for deletion
    memset(&w->shm, 0, sizeof(w->shm));                  // clear the segment record
    w->shm.shmid = -1;                                   // sentinel: no live segment
}

// destroys the window and releases every resource it owns
void ui_window_destroy(UiWindow* w) {
    if (!w) return;                                      // null guard
    destroy_image(w);                                    // release the ximage and shm backing
    for (int i = 0; i < 9; i++)                          // release cached resize cursors
        if (w->resize_cursor[i]) XFreeCursor(w->dpy, w->resize_cursor[i]);
    if (w->ic)  { XDestroyIC(w->ic);  w->ic = NULL; }    // release the input context
    if (w->im)  { XCloseIM(w->im);    w->im = NULL; }    // close the input method
    if (w->gc)  XFreeGC(w->dpy, w->gc);                  // free the graphics context
    if (w->win) XDestroyWindow(w->dpy, w->win);          // destroy the x window
    if (w->dpy) XCloseDisplay(w->dpy);                   // close the display connection
    free_framebuffer(w->pixels, w->w, w->h);             // free the mmap'd cpu framebuffer
    free(w->clipboard_text);                             // free our clipboard copy
    free(w->saved);                                      // free the saved-event queue
    free(w);                                             // free the window struct
}

// updates the window title
void ui_window_set_title(UiWindow* w, const char* t) { XStoreName(w->dpy, w->win, t); }

// paint callback is unused on x11, which repaints on expose
void ui_window_set_paint_callback(UiWindow* w, UiPaintCallback cb, void* user) {
    (void)w; (void)cb; (void)user;                       // no-op on this backend
}

// returns a pointer to the current framebuffer the painter should target
uint32_t* ui_window_pixels(UiWindow* w) {
    if (w->shm_ok) return (uint32_t*)w->shm.shmaddr;     // shm backing: the shared segment
    return w->pixels;                                    // otherwise the cpu-side buffer
}

// current window width in pixels
int  ui_window_width(UiWindow* w)  { return w->w; }

// current window height in pixels
int  ui_window_height(UiWindow* w) { return w->h; }

// resizes the window's backing storage to new pixel dimensions
void ui_window_resize(UiWindow* w, int nw, int nh) {
    if (nw <= 0 || nh <= 0) return;                      // ignore degenerate sizes
    destroy_image(w);                                    // drop the old ximage and shm
    int old_w = w->w, old_h = w->h;                      // remember for the matching free
    w->w = nw; w->h = nh;                                // record the new size
    free_framebuffer(w->pixels, old_w, old_h);           // release the old mmap'd buffer
    w->pixels = NULL;                                    // reallocated below only if shm fails
    if (XShmQueryExtension(w->dpy)) {                    // retry shm at the new size
        w->shm.shmid = shmget(IPC_PRIVATE, (size_t)nw*nh*4, IPC_CREAT|0600);
        if (w->shm.shmid >= 0) {
            w->shm.shmaddr = (char*)shmat(w->shm.shmid, NULL, 0);
            if (w->shm.shmaddr != (char*)-1) {
                w->shm.readOnly = False;
                w->img = XShmCreateImage(w->dpy,
                    DefaultVisual(w->dpy, w->screen),
                    (unsigned)DefaultDepth(w->dpy, w->screen),
                    ZPixmap, NULL, &w->shm, (unsigned)nw, (unsigned)nh);
                if (w->img) {
                    w->img->data = w->shm.shmaddr;
                    int (*old)(Display*,XErrorEvent*) = XSetErrorHandler(xerr);  // suppress shm errors
                    XShmAttach(w->dpy, &w->shm);
                    XSync(w->dpy, False);
                    XSetErrorHandler(old);               // restore handler
                    w->shm_ok = true;
                }
            }
        }
    }
    if (!w->shm_ok) {                                    // fall back to a plain ximage
        w->pixels = alloc_framebuffer(nw, nh);           // 2 MB-aligned, THP-backed
        w->img = XCreateImage(w->dpy,
            DefaultVisual(w->dpy, w->screen),
            (unsigned)DefaultDepth(w->dpy, w->screen),
            ZPixmap, 0, (char*)w->pixels, (unsigned)nw, (unsigned)nh, 32, 0);
    }
}

// max damage rects we will fold into a single xsetcliprectangles call
#define UI_XCLIP_MAX_RECTS 64

// blits only the given rects
void ui_window_present_rects(UiWindow* w, const int* quads, int n) {
    if (!w || !w->img || n <= 0) return;

    // fast path: single rect, no clip
    if (n == 1) {
        const int* q = quads;
        int x = q[0], y = q[1], rw = q[2], rh = q[3];
        if (rw <= 0 || rh <= 0) return;
        if (x < 0) { rw += x; x = 0; }
        if (y < 0) { rh += y; y = 0; }
        if (x + rw > w->w) rw = w->w - x;
        if (y + rh > w->h) rh = w->h - y;
        if (rw <= 0 || rh <= 0) return;

        if (w->shm_ok) {
            XShmPutImage(w->dpy, w->win, w->gc, w->img, x, y, x, y,
                         (unsigned)rw, (unsigned)rh, False);
            XSync(w->dpy, False);
        } else {
            XPutImage(w->dpy, w->win, w->gc, w->img, x, y, x, y,
                      (unsigned)rw, (unsigned)rh);
            XFlush(w->dpy);
        }
        return;
    }

    // count surviving rects and compute the bounding box in a single pass
    int n_live = 0;
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    for (int i = 0; i < n; i++) {
        const int* q = quads + i*4;
        int x = q[0], y = q[1], rw = q[2], rh = q[3];
        if (rw <= 0 || rh <= 0) continue;
        if (x < 0) { rw += x; x = 0; }
        if (y < 0) { rh += y; y = 0; }
        if (x + rw > w->w) rw = w->w - x;
        if (y + rh > w->h) rh = w->h - y;
        if (rw <= 0 || rh <= 0) continue;

        int x1 = x + rw, y1 = y + rh;
        if (n_live == 0) { bx0 = x; by0 = y; bx1 = x1; by1 = y1; }
        else {
            if (x  < bx0) bx0 = x;
            if (y  < by0) by0 = y;
            if (x1 > bx1) bx1 = x1;
            if (y1 > by1) by1 = y1;
        }
        n_live++;
    }
    if (n_live == 0) return;                                  // everything clipped away

    // small counts or oversized lists: per-rect loop
    if (n < 4 || n_live > UI_XCLIP_MAX_RECTS) {
        if (w->shm_ok) {
            for (int i = 0; i < n; i++) {
                const int* q = quads + i*4;
                int x = q[0], y = q[1], rw = q[2], rh = q[3];
                if (rw <= 0 || rh <= 0) continue;
                if (x < 0) { rw += x; x = 0; }
                if (y < 0) { rh += y; y = 0; }
                if (x + rw > w->w) rw = w->w - x;
                if (y + rh > w->h) rh = w->h - y;
                if (rw <= 0 || rh <= 0) continue;
                XShmPutImage(w->dpy, w->win, w->gc, w->img, x, y, x, y,
                             (unsigned)rw, (unsigned)rh, False);
            }
            XSync(w->dpy, False);
        } else {
            for (int i = 0; i < n; i++) {
                const int* q = quads + i*4;
                int x = q[0], y = q[1], rw = q[2], rh = q[3];
                if (rw <= 0 || rh <= 0) continue;
                if (x < 0) { rw += x; x = 0; }
                if (y < 0) { rh += y; y = 0; }
                if (x + rw > w->w) rw = w->w - x;
                if (y + rh > w->h) rh = w->h - y;
                if (rw <= 0 || rh <= 0) continue;
                XPutImage(w->dpy, w->win, w->gc, w->img, x, y, x, y,
                          (unsigned)rw, (unsigned)rh);
            }
            XFlush(w->dpy);
        }
        return;
    }

    // clip path: fill the rect array with the same surviving rects
    XRectangle rects[UI_XCLIP_MAX_RECTS];
    int idx = 0;
    for (int i = 0; i < n; i++) {
        const int* q = quads + i*4;
        int x = q[0], y = q[1], rw = q[2], rh = q[3];
        if (rw <= 0 || rh <= 0) continue;
        if (x < 0) { rw += x; x = 0; }
        if (y < 0) { rh += y; y = 0; }
        if (x + rw > w->w) rw = w->w - x;
        if (y + rh > w->h) rh = w->h - y;
        if (rw <= 0 || rh <= 0) continue;
        rects[idx].x      = (short)x;
        rects[idx].y      = (short)y;
        rects[idx].width  = (unsigned short)rw;
        rects[idx].height = (unsigned short)rh;
        idx++;
    }

    XSetClipRectangles(w->dpy, w->gc, 0, 0, rects, n_live, Unsorted);

    int bw = bx1 - bx0, bh = by1 - by0;
    if (w->shm_ok) {
        XShmPutImage(w->dpy, w->win, w->gc, w->img, bx0, by0, bx0, by0,
                     (unsigned)bw, (unsigned)bh, False);
        XSync(w->dpy, False);
    } else {
        XPutImage(w->dpy, w->win, w->gc, w->img, bx0, by0, bx0, by0,
                  (unsigned)bw, (unsigned)bh);
        XFlush(w->dpy);
    }

    // restore the default "no clip" gc state
    XSetClipMask(w->dpy, w->gc, None);
}

// blits the current framebuffer to the window
void ui_window_present(UiWindow* w) {
    if (!w) return;
    int quad[4] = { 0, 0, w->w, w->h };
    ui_window_present_rects(w, quad, 1);
}

// true while the window has not been closed
bool ui_window_is_open(UiWindow* w) { return w->open; }

// marks the window as closed so callers stop polling it
void ui_window_close(UiWindow* w) { w->open = false; }

// true when the window manager reports us as maximized
bool ui_window_is_maximized(UiWindow* w) {
    return w && w->maximized;
}

// asks the window manager to iconify the window
void ui_window_minimize(UiWindow* w) {
    XIconifyWindow(w->dpy, w->win, w->screen);
    XFlush(w->dpy);
}

// toggles between maximized and normal via _NET_WM_STATE
void ui_window_toggle_maximize(UiWindow* w) {
    Atom state = XInternAtom(w->dpy, "_NET_WM_STATE", False);
    Atom hz    = XInternAtom(w->dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom vt    = XInternAtom(w->dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type                 = ClientMessage;
    xev.xclient.window       = w->win;
    xev.xclient.message_type = state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = w->maximized ? 0 : 1;  // 0=remove, 1=add
    xev.xclient.data.l[1]    = (long)hz;
    xev.xclient.data.l[2]    = (long)vt;
    xev.xclient.data.l[3]    = 1;
    XSendEvent(w->dpy, DefaultRootWindow(w->dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    w->maximized = !w->maximized;   // optimistic flip; PropertyNotify confirms shortly
    XFlush(w->dpy);
}

// clamps the window to a minimum client size via wm size hints
void ui_window_set_min_size(UiWindow* w, int min_w, int min_h) {
    if (min_w <= 0 || min_h <= 0) return;
    w->min_w = min_w;
    w->min_h = min_h;

    XSizeHints hints;
    long supplied = 0;
    if (!XGetWMNormalHints(w->dpy, w->win, &hints, &supplied)) {
        memset(&hints, 0, sizeof(hints));     // no prior hints: start clean
    }
    hints.flags     |= PMinSize;              // keep whatever was already set
    hints.min_width  = min_w;
    hints.min_height = min_h;
    XSetWMNormalHints(w->dpy, w->win, &hints);
    XFlush(w->dpy);
}

// delegates an interactive move to the window manager
void ui_window_begin_drag(UiWindow* w) {
    Atom mr = XInternAtom(w->dpy, "_NET_WM_MOVERESIZE", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type                 = ClientMessage;
    xev.xclient.window       = w->win;
    xev.xclient.message_type = mr;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = w->last_root_x;
    xev.xclient.data.l[1]    = w->last_root_y;
    xev.xclient.data.l[2]    = 8;                     // _NET_WM_MOVERESIZE_MOVE
    xev.xclient.data.l[3]    = 1;                     // button 1
    xev.xclient.data.l[4]    = 1;
    XUngrabPointer(w->dpy, CurrentTime);
    XSendEvent(w->dpy, DefaultRootWindow(w->dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(w->dpy);
}

// delegates an interactive resize to the window manager
void ui_window_begin_resize(UiWindow* w, const char* edge) {
    int dir = 8;                                      // default: treat as a move
    if      (!strcmp(edge, "top-left"))     dir = 0;
    else if (!strcmp(edge, "top"))          dir = 1;
    else if (!strcmp(edge, "top-right"))    dir = 2;
    else if (!strcmp(edge, "right"))        dir = 3;
    else if (!strcmp(edge, "bottom-right")) dir = 4;
    else if (!strcmp(edge, "bottom"))       dir = 5;
    else if (!strcmp(edge, "bottom-left"))  dir = 6;
    else if (!strcmp(edge, "left"))         dir = 7;

    Atom mr = XInternAtom(w->dpy, "_NET_WM_MOVERESIZE", False);
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type                 = ClientMessage;
    xev.xclient.window       = w->win;
    xev.xclient.message_type = mr;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = w->last_root_x;
    xev.xclient.data.l[1]    = w->last_root_y;
    xev.xclient.data.l[2]    = dir;
    xev.xclient.data.l[3]    = 1;
    xev.xclient.data.l[4]    = 1;
    XUngrabPointer(w->dpy, CurrentTime);
    XSendEvent(w->dpy, DefaultRootWindow(w->dpy), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(w->dpy);
}

// lazily creates a cached resize cursor for the given index
static Cursor linux_resize_cursor(UiWindow* w, int idx) {
    if (idx < 1 || idx > 8) return None;
    if (w->resize_cursor[idx] == None) {
        unsigned shape = XC_left_ptr;
        switch (idx) {
            case 1: shape = XC_left_side;          break;  // left
            case 2: shape = XC_right_side;         break;  // right
            case 3: shape = XC_top_side;           break;  // top
            case 4: shape = XC_bottom_side;        break;  // bottom
            case 5: shape = XC_top_left_corner;    break;  // top-left
            case 6: shape = XC_top_right_corner;   break;  // top-right
            case 7: shape = XC_bottom_left_corner; break;  // bottom-left
            case 8: shape = XC_bottom_right_corner;break;  // bottom-right
        }
        w->resize_cursor[idx] = XCreateFontCursor(w->dpy, shape);
    }
    return w->resize_cursor[idx];
}

// swaps the active pointer cursor to match the given edge
void ui_window_set_resize_cursor(UiWindow* w, const char* edge) {
    if (w->last_cursor_edge == edge) return;             // no change, skip the x roundtrip
    w->last_cursor_edge = edge;

    int idx = 0;
    if (edge) {
        if      (!strcmp(edge, "left"))         idx = 1;
        else if (!strcmp(edge, "right"))        idx = 2;
        else if (!strcmp(edge, "top"))          idx = 3;
        else if (!strcmp(edge, "bottom"))       idx = 4;
        else if (!strcmp(edge, "top-left"))     idx = 5;
        else if (!strcmp(edge, "top-right"))    idx = 6;
        else if (!strcmp(edge, "bottom-left"))  idx = 7;
        else if (!strcmp(edge, "bottom-right")) idx = 8;
    }
    Cursor c = (idx == 0) ? None : linux_resize_cursor(w, idx);
    XDefineCursor(w->dpy, w->win, c);
    XFlush(w->dpy);
}

// creates a frameless, always-on-top popup window used for notifications
UiWindow* ui_window_create_popup(int x, int y, int w, int h, const char* title) {
    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));   // zeroed window struct
    win->dpy = XOpenDisplay(NULL);                            // connect to the default display
    if (!win->dpy) { free(win); return NULL; }                // no display: give up
    win->screen = DefaultScreen(win->dpy);                    // default screen index
    win->decorated = false;                                   // no system chrome
    win->w = w; win->h = h;                                   // record the requested size
    win->shm.shmid = -1;                                      // sentinel: no live shm segment

    XVisualInfo vinfo;
    Visual* visual = NULL;
    Colormap cmap  = 0;
    int     depth  = DefaultDepth(win->dpy, win->screen);

    if (XMatchVisualInfo(win->dpy, win->screen, 32, TrueColor, &vinfo)) {
        visual = vinfo.visual;
        depth  = vinfo.depth;
        cmap   = XCreateColormap(win->dpy,
                                 RootWindow(win->dpy, win->screen),
                                 visual, AllocNone);
    } else {
        visual = DefaultVisual(win->dpy, win->screen);
    }
    win->depth = depth;

    XSetWindowAttributes a = {0};                             // window attribute set
    a.background_pixel  = 0;                                  // transparent black in ARGB
    a.bit_gravity       = NorthWestGravity;                   // anchor contents at the top-left
    a.override_redirect = True;                               // bypass the wm entirely
    a.border_pixel      = 0;
    unsigned long mask = CWBackPixel | CWBitGravity | CWOverrideRedirect | CWBorderPixel;
    if (cmap) { a.colormap = cmap; mask |= CWColormap; }

    win->win = XCreateWindow(win->dpy, RootWindow(win->dpy, win->screen),
        x, y, (unsigned)w, (unsigned)h, 0,
        depth, InputOutput, visual, mask, &a);
    XStoreName(win->dpy, win->win, title ? title : "");       // set the window title

    // hint the wm: this is a notification, keep it above everything
    Atom wtype = XInternAtom(win->dpy, "_NET_WM_WINDOW_TYPE", False);        // property atom
    Atom ntype = XInternAtom(win->dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);  // value
    XChangeProperty(win->dpy, win->win, wtype, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&ntype, 1);               // publish the window type
    Atom state = XInternAtom(win->dpy, "_NET_WM_STATE", False);              // state atom
    Atom above = XInternAtom(win->dpy, "_NET_WM_STATE_ABOVE", False);        // value
    XChangeProperty(win->dpy, win->win, state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&above, 1);               // ask to stay above

    XSelectInput(win->dpy, win->win,
        ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);  // events we want

    XMapWindow(win->dpy, win->win);                           // make the window visible
    win->gc = XCreateGC(win->dpy, win->win, 0, NULL);         // graphics context for blits

    if (XShmQueryExtension(win->dpy)) {                       // try shared memory first
        win->shm.shmid = shmget(IPC_PRIVATE, (size_t)w*h*4, IPC_CREAT|0600);  // create segment
        if (win->shm.shmid >= 0) {                            // segment allocated
            win->shm.shmaddr = (char*)shmat(win->shm.shmid, NULL, 0);  // map into our address space
            if (win->shm.shmaddr != (char*)-1) {              // mapping succeeded
                win->shm.readOnly = False;                    // we write into it
                win->img = XShmCreateImage(win->dpy,
                    visual,
                    (unsigned)depth,
                    ZPixmap, NULL, &win->shm, (unsigned)w, (unsigned)h);
                if (win->img) {                               // image created
                    win->img->data = win->shm.shmaddr;        // point it at the mapping
                    int (*old)(Display*,XErrorEvent*) = XSetErrorHandler(xerr);  // suppress errors
                    XShmAttach(win->dpy, &win->shm);          // tell the server about it
                    XSync(win->dpy, False);                   // flush and observe errors
                    XSetErrorHandler(old);                    // restore the previous handler
                    win->shm_ok = true;                       // mark shm as live
                }
            }
        }
    }
    if (!win->shm_ok) {                                       // fall back to a plain ximage
        win->pixels = alloc_framebuffer(w, h);                // 2 MB-aligned, THP-backed
        win->img = XCreateImage(win->dpy,
            visual,
            (unsigned)depth,
            ZPixmap, 0, (char*)win->pixels, (unsigned)w, (unsigned)h, 32, 0);
    }
    win->open = true;                                         // window is live
    return win;                                               // hand it to the caller
}

// moves a window to an absolute screen position
void ui_window_move(UiWindow* w, int x, int y) {
    if (!w || !w->dpy) return;                                // null guard
    XMoveWindow(w->dpy, w->win, x, y);                        // reposition the x window
    XFlush(w->dpy);                                           // push the request out now
}

// returns the monitor work area the window sits on
void ui_window_work_area(UiWindow* w, int* ox, int* oy, int* ow, int* oh) {
    int sx = 0, sy = 0;                                       // work area origin
    int sw = DisplayWidth(w->dpy, w->screen);                 // fallback width: full screen
    int sh = DisplayHeight(w->dpy, w->screen);                // fallback height: full screen
    Atom wa = XInternAtom(w->dpy, "_NET_WORKAREA", False);    // work area property
    Atom actual; int fmt; unsigned long n, after;             // property reply fields
    unsigned char* data = NULL;                               // property payload
    if (XGetWindowProperty(w->dpy, RootWindow(w->dpy, w->screen), wa,
                           0, 4, False, XA_CARDINAL,
                           &actual, &fmt, &n, &after, &data) == Success && data) {  // query ok
        long* v = (long*)data;                                // cardinals as longs
        if (n >= 4) { sx = (int)v[0]; sy = (int)v[1]; sw = (int)v[2]; sh = (int)v[3]; }  // unpack
        XFree(data);                                          // release the reply
    }
    *ox = sx; *oy = sy; *ow = sw; *oh = sh;                   // publish the result
}

// forces an immediate repaint of the window
void ui_window_invalidate(UiWindow* w) {
    if (!w || !w->dpy) return;                                // null guard
    XClearArea(w->dpy, w->win, 0, 0, 0, 0, True);             // True: generate expose events
    XFlush(w->dpy);                                           // push the request out now
}

#endif
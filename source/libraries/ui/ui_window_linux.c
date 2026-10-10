// source/libraries/ui/ui_window_linux.c
// Implementation of X11 window backend for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32
#include "ui_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

// platform window state for the x11 backend
struct UiWindow {
    Display* dpy;         // connection to the x server
    int      screen;      // default screen index
    Window   win;         // x11 window id
    GC       gc;          // graphics context for blits
    Atom     wm_delete;   // WM_DELETE_WINDOW protocol atom
    XIM      im;          // input method, null when unavailable
    XIC      ic;          // input context, null when unavailable
    int      w, h;        // window size in pixels
    bool     open;        // false once the window is closed
    uint32_t* pixels;     // fallback cpu-side framebuffer
    XImage*  img;         // ximage presented to the server
    bool     shm_ok;      // true when shm backing is active
    XShmSegmentInfo shm;  // shm segment info when shm is active
    int      mods;        // last observed modifier bitmask
};

// swallow x errors that can occur during optional shm setup
static int xerr(Display* d, XErrorEvent* e) { (void)d;(void)e; return 0; }

// creates and maps a new x11 window, preferring shared memory backing
UiWindow* ui_window_create(int w, int h, const char* title) {
    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    win->dpy = XOpenDisplay(NULL);                       // connect to the default display
    if (!win->dpy) { free(win); return NULL; }           // no display: give up
    win->screen = DefaultScreen(win->dpy);               // default screen
    win->w = w; win->h = h;
    win->pixels = (uint32_t*)calloc((size_t)w*h, 4);     // cpu-side fallback framebuffer

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
    XMapWindow(win->dpy, win->win);                      // make the window visible
    win->gc = XCreateGC(win->dpy, win->win, 0, NULL);    // create the graphics context

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
        win->img = XCreateImage(win->dpy,
            DefaultVisual(win->dpy, win->screen),
            (unsigned)DefaultDepth(win->dpy, win->screen),
            ZPixmap, 0, (char*)win->pixels, (unsigned)w, (unsigned)h, 32, 0);
    }
    win->open = true;
    return win;
}

// releases the current ximage and any shm segment backing it
static void destroy_image(UiWindow* w) {
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
    if (w->ic)  { XDestroyIC(w->ic);  w->ic = NULL; }    // release the input context
    if (w->im)  { XCloseIM(w->im);    w->im = NULL; }    // close the input method
    if (w->gc)  XFreeGC(w->dpy, w->gc);                  // free the graphics context
    if (w->win) XDestroyWindow(w->dpy, w->win);          // destroy the x window
    if (w->dpy) XCloseDisplay(w->dpy);                   // close the display connection
    free(w->pixels);                                     // free the cpu-side framebuffer
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
    w->w = nw; w->h = nh;                                // record the new size
    free(w->pixels);                                     // discard the old cpu buffer
    w->pixels = (uint32_t*)calloc((size_t)nw*nh, 4);     // allocate the new cpu buffer
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
        w->img = XCreateImage(w->dpy,
            DefaultVisual(w->dpy, w->screen),
            (unsigned)DefaultDepth(w->dpy, w->screen),
            ZPixmap, 0, (char*)w->pixels, (unsigned)nw, (unsigned)nh, 32, 0);
    }
}

// blits the current framebuffer to the window
void ui_window_present(UiWindow* w) {
    if (w->shm_ok) XShmPutImage(w->dpy, w->win, w->gc, w->img, 0,0,0,0,
                                (unsigned)w->w, (unsigned)w->h, False);  // shm fast path
    else XPutImage(w->dpy, w->win, w->gc, w->img, 0,0,0,0,
                   (unsigned)w->w, (unsigned)w->h);                      // plain blit
    XFlush(w->dpy);                                      // ensure the server sees it now
}

// blocks until the server has input, or until the timeout expires
bool ui_window_wait(UiWindow* w, int timeout_ms) {
    if (XPending(w->dpy)) return true;                   // events already queued
    int fd = ConnectionNumber(w->dpy);                   // the x connection fd
    fd_set set; FD_ZERO(&set); FD_SET(fd, &set);         // watch the connection
    struct timeval tv = { timeout_ms/1000, (timeout_ms%1000)*1000 };  // timeout in sec/usec
    return select(fd+1, &set, NULL, NULL, &tv) > 0;      // true when input is pending
}

// fallback keysym-to-ascii translation used when no input method is available
static int map_key(KeySym ks, char* out_utf8, int* out_len) {
    *out_len = 0;
    if (ks >= 0x20 && ks < 0x7F) { out_utf8[0] = (char)ks; *out_len = 1; }  // printable ascii
    return (int)ks;                                      // keycode is the keysym value
}

// polls the x event queue and translates the next event into a ui window event
bool ui_window_poll(UiWindow* w, UiWinEvent* out) {
    out->kind = UW_EV_NONE;                              // default to nothing
    while (XPending(w->dpy)) {                           // drain until a recognized event
        XEvent e; XNextEvent(w->dpy, &e);

        // let the input method consume any event it is working on. without
        // this filter, ongoing compositions would surface as raw keypresses
        // and mix with the committed text once the im fires the final event
        if (w->ic && XFilterEvent(&e, w->win)) continue;

        switch (e.type) {
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == w->wm_delete) {  // window-manager close
                    out->kind = UW_EV_CLOSE; w->open = false; return true;
                }
                break;
            case ConfigureNotify:
                if (e.xconfigure.width != w->w || e.xconfigure.height != w->h) {  // size changed
                    ui_window_resize(w, e.xconfigure.width, e.xconfigure.height);
                    out->kind = UW_EV_RESIZE;
                    out->w = w->w; out->h = w->h;
                    return true;
                }
                break;
            case FocusIn:
                if (w->ic) XSetICFocus(w->ic);           // hand keyboard input to the ic
                break;
            case FocusOut:
                if (w->ic) XUnsetICFocus(w->ic);         // stop feeding the ic
                break;
            case KeyPress: {
                out->mods = 0;                           // build the modifier bitmask
                if (e.xkey.state & ShiftMask)   out->mods |= 1;
                if (e.xkey.state & ControlMask) out->mods |= 2;
                if (e.xkey.state & Mod1Mask)    out->mods |= 4;

                if (w->ic) {                             // xim path: utf-8 aware
                    KeySym ks = 0;
                    Status status = 0;
                    char buf[32];                        // generous buffer for any im output
                    int len = Xutf8LookupString(w->ic, &e.xkey, buf,
                                                (int)sizeof(buf) - 1, &ks, &status);
                    if (status == XBufferOverflow) len = 0;  // too long: drop the batch
                    if (len < 0) len = 0;
                    buf[len] = 0;
                    out->key = (int)ks;                  // keysym for non-text keys
                    if (len > 0 && (status == XLookupChars || status == XLookupBoth)) {
                        int n = len < (int)sizeof(out->utf8) ? len : (int)sizeof(out->utf8);
                        memcpy(out->utf8, buf, n);       // copy what fits, safely
                        out->utf8_len = n;
                    } else {
                        out->utf8_len = 0;
                    }
                    out->kind = UW_EV_KEYDOWN;
                    return true;
                }

                KeySym ks = XLookupKeysym(&e.xkey, 0);   // fallback: ascii only
                char buf[8] = {0}; int len = 0;
                out->key = map_key(ks, buf, &len);       // translate to apex keycode
                if (len) { memcpy(out->utf8, buf, len); out->utf8_len = len; }
                else out->utf8_len = 0;
                out->kind = UW_EV_KEYDOWN;
                return true;
            }
            case MotionNotify:
                out->kind = UW_EV_MOUSEMOVE;             // pointer moved
                out->x = e.xmotion.x; out->y = e.xmotion.y;
                return true;
            case ButtonPress: {
                unsigned b = e.xbutton.button;
                if (b == 4 || b == 5) {                  // wheel up/down
                    out->kind = UW_EV_SCROLL;
                    out->x = e.xbutton.x; out->y = e.xbutton.y;
                    out->scroll_dy = (b == 4) ? -1.0 : 1.0;  // wheel up is negative
                    return true;
                }
                out->kind = UW_EV_MOUSEDOWN;             // button press
                out->x = e.xbutton.x; out->y = e.xbutton.y;
                out->button = (int)b;
                return true;
            }
            case ButtonRelease:
                out->kind = UW_EV_MOUSEUP;               // button release
                out->x = e.xbutton.x; out->y = e.xbutton.y;
                out->button = (int)e.xbutton.button;
                return true;
            case Expose:
                if (e.xexpose.count == 0) {              // last expose in the batch
                    out->kind = UW_EV_EXPOSE;
                    out->w = w->w; out->h = w->h;
                    return true;
                }
                break;
        }
    }
    return false;                                        // nothing recognized this call
}

// true while the window has not been closed
bool ui_window_is_open(UiWindow* w) { return w->open; }

// marks the window as closed so callers stop polling it
void ui_window_close(UiWindow* w) { w->open = false; }

#endif
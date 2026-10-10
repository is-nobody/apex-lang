// source/libraries/ui/ui_window_linux.c
// Implementation of X11 window backend for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32
#include "ui_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
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

    Atom     clipboard_atom;  // CLIPBOARD selection
    Atom     utf8_atom;       // UTF8_STRING target
    Atom     targets_atom;    // TARGETS target

    char*    clipboard_text;  // owned copy of the string we currently own
    int      clipboard_len;   // length of that copy

    XEvent*  saved;           // events displaced by blocking clipboard waits
    int      saved_count;     // number of saved events
    int      saved_cap;       // capacity of the saved-event array

    int      w, h;        // window size in pixels
    bool     open;        // false once the window is closed
    uint32_t* pixels;     // fallback cpu-side framebuffer
    XImage*  img;         // ximage presented to the server
    bool     shm_ok;      // true when shm backing is active
    XShmSegmentInfo shm;  // shm segment info when shm is active
    int      mods;        // last observed modifier bitmask
};

// swallow x errors that can occur during optional shm setup or clipboard exchange
static int xerr(Display* d, XErrorEvent* e) { (void)d;(void)e; return 0; }

// stores an event back into the window's queue so poll can re-deliver it later
static void push_saved_event(UiWindow* w, XEvent* ev) {
    if (w->saved_count >= w->saved_cap) {                // grow the saved-event array
        w->saved_cap = w->saved_cap ? w->saved_cap * 2 : 16;
        w->saved = (XEvent*)realloc(w->saved, sizeof(XEvent) * w->saved_cap);
    }
    w->saved[w->saved_count++] = *ev;                    // copy the full union
}

// pops the next event, preferring anything previously saved
static bool pop_event(UiWindow* w, XEvent* out) {
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
    if (w->saved_count > 0) return true;                 // saved events already pending
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

// answers a SelectionRequest by pasting our clipboard text into the requestor's property
static void handle_selection_request(UiWindow* w, XSelectionRequestEvent* req) {
    XSelectionEvent ev = {0};                            // the reply we send back
    ev.type = SelectionNotify;
    ev.display = req->display;
    ev.requestor = req->requestor;
    ev.selection = req->selection;
    ev.target = req->target;
    ev.time = req->time;
    ev.property = None;

    Atom prop = req->property ? req->property : req->target;  // legacy: no property

    if (req->target == w->targets_atom) {                // TARGETS: advertise utf8
        Atom targets[] = { w->utf8_atom, XA_STRING };
        XChangeProperty(w->dpy, req->requestor, prop, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*)targets, 2);
        ev.property = prop;
    } else if (req->target == w->utf8_atom || req->target == XA_STRING) {
        const char* src = w->clipboard_text ? w->clipboard_text : "";
        int len = w->clipboard_text ? w->clipboard_len : 0;
        XChangeProperty(w->dpy, req->requestor, prop, req->target, 8,
                        PropModeReplace, (const unsigned char*)src, len);
        ev.property = prop;                              // pasted successfully
    }                                                   // anything else: property stays None

    XSendEvent(w->dpy, req->requestor, False, 0, (XEvent*)&ev);
    XFlush(w->dpy);
}

// polls the x event queue and translates the next event into a ui window event
bool ui_window_poll(UiWindow* w, UiWinEvent* out) {
    out->kind = UW_EV_NONE;                              // default to nothing

    XEvent e;
    while (pop_event(w, &e)) {                           // drain until a recognized event

        // let the input method consume any event it is working on
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
            case SelectionRequest:
                handle_selection_request(w, &e.xselectionrequest);  // serve clipboard
                break;
            case SelectionClear:
                free(w->clipboard_text);
                w->clipboard_text = NULL;
                w->clipboard_len = 0;
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

                    // some im implementations report control keys
                    bool printable = true;
                    if (len == 1) {
                        unsigned char b0 = (unsigned char)buf[0];
                        if (b0 < 0x20 || b0 == 0x7F) printable = false;
                    }

                    if (len > 0 && printable &&
                        (status == XLookupChars || status == XLookupBoth)) {
                        int n = len < (int)sizeof(out->utf8) ? len : (int)sizeof(out->utf8);
                        memcpy(out->utf8, buf, n);       // copy what fits, safely
                        out->utf8_len = n;
                    } else {
                        out->utf8_len = 0;               // key gesture, not text
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

// stores a utf-8 string and claims ownership of the CLIPBOARD selection
void ui_window_clipboard_set(UiWindow* w, const char* text, int len) {
    if (!w || !w->dpy) return;
    if (len < 0) len = 0;                                // sanity
    free(w->clipboard_text);                             // release the previous copy
    w->clipboard_text = (char*)malloc((size_t)len + 1);
    if (text && len) memcpy(w->clipboard_text, text, (size_t)len);
    w->clipboard_text[len] = 0;
    w->clipboard_len = len;
    XSetSelectionOwner(w->dpy, w->clipboard_atom, w->win, CurrentTime);
    XSetSelectionOwner(w->dpy, XA_PRIMARY,        w->win, CurrentTime);
    XFlush(w->dpy);
}

// requests the current clipboard contents as utf-8
bool ui_window_clipboard_get(UiWindow* w, char** out_text, int* out_len) {
    if (out_text) *out_text = NULL;
    if (out_len)  *out_len = 0;
    if (!w || !w->dpy) return false;

    if (XGetSelectionOwner(w->dpy, w->clipboard_atom) == w->win) {
        if (!w->clipboard_text) return false;            // own the selection but empty
        int n = w->clipboard_len;
        char* copy = (char*)malloc((size_t)n + 1);
        memcpy(copy, w->clipboard_text, (size_t)n);
        copy[n] = 0;
        if (out_text) *out_text = copy; else free(copy);
        if (out_len)  *out_len = n;
        return true;
    }

    XConvertSelection(w->dpy, w->clipboard_atom, w->utf8_atom,
                      w->clipboard_atom, w->win, CurrentTime);
    XFlush(w->dpy);

    int fd = ConnectionNumber(w->dpy);                   // x connection fd for select
    int waited_ms = 0;                                   // total elapsed time

    while (waited_ms < 500) {                            // half-second is plenty
        if (XPending(w->dpy) || w->saved_count > 0) {
            XEvent e;
            pop_event(w, &e);                            // works for saved and live
            if (e.type == SelectionNotify && e.xselection.selection == w->clipboard_atom) {
                if (e.xselection.property == None) return false;  // refused
                Atom type = 0;
                int fmt = 0;
                unsigned long nitems = 0, after = 0;
                unsigned char* data = NULL;
                int (*old)(Display*,XErrorEvent*) = XSetErrorHandler(xerr);
                XGetWindowProperty(w->dpy, w->win, e.xselection.property,
                                   0, (long)(~0UL), True, AnyPropertyType,
                                   &type, &fmt, &nitems, &after, &data);
                XSetErrorHandler(old);
                if (!data) return false;                 // owner gave us nothing
                if (out_text) *out_text = (char*)data; else XFree(data);
                if (out_len)  *out_len = (int)nitems;
                return true;
            }
            push_saved_event(w, &e);                     // not ours: hold for later
            continue;                                    // check next event immediately
        }
        struct timeval tv = { 0, 1000 };                 // 1 ms nap between polls
        select(fd + 1, NULL, NULL, NULL, &tv);
        waited_ms++;
    }
    return false;                                        // no reply from the owner
}

#endif
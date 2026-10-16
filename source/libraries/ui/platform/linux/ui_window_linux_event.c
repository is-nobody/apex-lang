// source/libraries/ui/platform/linux/ui_window_linux_event.c
// X11 event polling and key translation for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32
#include "ui_window_linux.h"
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
                if ((Atom)e.xclient.data.l[0] == w->sync_request) {
                    // WM wants frame N on screen before committing the resize
                    XSyncIntsToValue(&w->sync_value,
                                     (unsigned int)e.xclient.data.l[2],
                                     (int)e.xclient.data.l[3]);
                    w->sync_pending = true;
                    out->kind = UW_EV_EXPOSE;
                    out->w = w->w; out->h = w->h;
                    return true;
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
            case PropertyNotify:
                // track _NET_WM_STATE so the titlebar can reflect maximize and fullscreen state
                {
                    Atom net_state = XInternAtom(w->dpy, "_NET_WM_STATE", False);
                    Atom hz = XInternAtom(w->dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
                    Atom vt = XInternAtom(w->dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
                    Atom fs = XInternAtom(w->dpy, "_NET_WM_STATE_FULLSCREEN", False);
                    if (e.xproperty.atom == net_state && e.xproperty.state == PropertyNewValue) {
                        Atom* atoms = NULL; int n = 0;
                        Atom actual; int fmt; unsigned long nitems, after;
                        unsigned char* data = NULL;
                        if (XGetWindowProperty(w->dpy, w->win, net_state,
                                               0, 32, False, XA_ATOM,
                                               &actual, &fmt, &nitems, &after, &data) == Success) {
                            atoms = (Atom*)data; n = (int)nitems;
                        }
                        bool mh = false, mv = false, f = false;
                        for (int i = 0; i < n; i++) {
                            if (atoms[i] == hz) mh = true;
                            if (atoms[i] == vt) mv = true;
                            if (atoms[i] == fs) f  = true;
                        }
                        w->maximized  = mh && mv;
                        w->fullscreen = f;
                        if (data) XFree(data);
                    }
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
                w->last_root_x = e.xmotion.x_root;       // remember for _NET_WM_MOVERESIZE
                w->last_root_y = e.xmotion.y_root;
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
                w->last_root_x = e.xbutton.x_root;
                w->last_root_y = e.xbutton.y_root;
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

#endif
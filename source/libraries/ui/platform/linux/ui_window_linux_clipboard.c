// source/libraries/ui/platform/linux/ui_window_linux_clipboard.c
// X11 clipboard ownership and exchange for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef _WIN32
#include "ui_window_linux.h"
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>

// answers a SelectionRequest by pasting our clipboard text into the requestor's property
void handle_selection_request(UiWindow* w, XSelectionRequestEvent* req) {
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
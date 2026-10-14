// source/libraries/ui/platform/macos/ui_window_macos_event.c
// NSEvent polling and key translation for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef __APPLE__

#include "ui_window_macos.h"
#include <stdlib.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>

// lookup a class by name
#define OBJC_CLS(n) ((id)objc_getClass(n))

// intern a selector
#define OBJC_SEL(n) sel_registerName(n)

// nsevent type codes (avoids importing appkit)
#define NS_EV_LEFT_MOUSE_DOWN        1
#define NS_EV_LEFT_MOUSE_UP          2
#define NS_EV_RIGHT_MOUSE_DOWN       3
#define NS_EV_RIGHT_MOUSE_UP         4
#define NS_EV_MOUSE_MOVED            5
#define NS_EV_LEFT_MOUSE_DRAGGED     6
#define NS_EV_RIGHT_MOUSE_DRAGGED    7
#define NS_EV_KEY_DOWN              10
#define NS_EV_FLAGS_CHANGED         12
#define NS_EV_SCROLL_WHEEL          22
#define NS_EV_OTHER_MOUSE_DOWN      25
#define NS_EV_OTHER_MOUSE_UP        26
#define NS_EV_OTHER_MOUSE_DRAGGED   27

// nseventmodifiermask bits
#define NS_MOD_SHIFT    0x00020000
#define NS_MOD_CONTROL  0x00040000
#define NS_MOD_OPTION   0x00080000
#define NS_MOD_COMMAND  0x00100000

// one event stashed for another window while draining the shared queue
typedef struct {
    id event;        // retained nsevent
    id target_win;   // the nswindow it was addressed to
} SavedEvent;

#define SAVED_MAX 64
static SavedEvent g_saved[SAVED_MAX];    // saved-event ring
static int        g_saved_count = 0;     // number of live entries

// stores an event back into the shared queue so the right window can pick it up later
static void save_event(id ev, id win) {
    if (g_saved_count >= SAVED_MAX) return;                  // cap reached: drop
    ((id (*)(id, SEL))objc_msgSend)(ev, OBJC_SEL("retain")); // take a ref
    g_saved[g_saved_count].event      = ev;
    g_saved[g_saved_count].target_win = win;
    g_saved_count++;
}

// returns the next saved event for win, or null
static id pop_saved_for(id win, id* out_event) {
    for (int i = 0; i < g_saved_count; i++) {
        if (g_saved[i].target_win == win) {                  // matches the target window
            *out_event = g_saved[i].event;
            memmove(&g_saved[i], &g_saved[i + 1],            // shift the rest forward
                    sizeof(SavedEvent) * (g_saved_count - i - 1));
            g_saved_count--;
            return win;
        }
    }
    return NULL;                                             // nothing for this window
}

// true when a saved event is waiting for the given window
static int has_saved_for(id win) {
    for (int i = 0; i < g_saved_count; i++)
        if (g_saved[i].target_win == win) return 1;          // found a saved event
    return 0;
}

// build an nsdate for now + sec seconds
static id date_from_now(double sec) {
    id alloc = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSDate"),
                                               OBJC_SEL("alloc"));
    return ((id (*)(id, SEL, double))objc_msgSend)(
        alloc, OBJC_SEL("initWithTimeIntervalSinceNow:"), sec);  // nsdate in the future
}

// build the default run loop mode string
static id default_run_loop_mode_str(void) {
    id alloc = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSString"),
                                               OBJC_SEL("alloc"));
    // nsdefaultrunloopmode == "kcfrunloopdefaultmode"
    return ((id (*)(id, SEL, const char*))objc_msgSend)(
        alloc, OBJC_SEL("initWithUTF8String:"), "kCFRunLoopDefaultMode");
}

// dequeue one event with the given timeout (0 = non-blocking)
static id dequeue_event(double timeout_sec) {
    id app = ((id (*)(id, SEL))objc_msgSend)(OBJC_CLS("NSApplication"),
                                             OBJC_SEL("sharedApplication"));
    if (!app) return NULL;                                   // no app: nothing to dequeue

    id date = date_from_now(timeout_sec);                    // until-date for the wait
    id mode = default_run_loop_mode_str();                   // default run loop mode

    unsigned long long mask = ~0ULL;                         // nseventmaskany
    id ev = ((id (*)(id, SEL, unsigned long long, id, id, BOOL))objc_msgSend)(
        app, OBJC_SEL("nextEventMatchingMask:untilDate:inMode:dequeue:"),
        mask, date, mode, YES);

    ((void (*)(id, SEL))objc_msgSend)(mode, OBJC_SEL("release"));  // release the mode string
    ((void (*)(id, SEL))objc_msgSend)(date, OBJC_SEL("release"));  // release the date
    return ev;
}

// encode a utf-32 codepoint into utf-8; returns bytes written
int macos_encode_utf8(uint32_t cp, char* out) {
    if (cp < 0x80) {                                         // 1-byte ascii
        out[0] = (char)cp; return 1;
    }
    if (cp < 0x800) {                                        // 2-byte sequence
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {                                      // 3-byte sequence
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));                      // 4-byte sequence
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

// translate a macos virtual keycode into an apex keysym (x11-style)
int macos_map_keycode(unsigned short keycode) {
    switch (keycode) {
        case 36:  return 0xFF0D;  // return
        case 48:  return 0xFF09;  // tab
        case 51:  return 0xFF08;  // delete (backspace on mac keyboards)
        case 53:  return 0xFF1B;  // escape
        case 71:  return 0xFF7F;  // clear
        case 76:  return 0xFF8D;  // keypad enter
        case 115: return 0xFF50;  // home
        case 116: return 0xFF55;  // page up
        case 117: return 0xFFFF;  // forward delete
        case 119: return 0xFF57;  // end
        case 121: return 0xFF56;  // page down
        case 123: return 0xFF51;  // left arrow
        case 124: return 0xFF53;  // right arrow
        case 125: return 0xFF54;  // down arrow
        case 126: return 0xFF52;  // up arrow
        case 122: return 0xFFBE;  // f1
        case 120: return 0xFFBF;  // f2
        case 99:  return 0xFFC0;  // f3
        case 118: return 0xFFC1;  // f4
        case 96:  return 0xFFC2;  // f5
        case 97:  return 0xFFC3;  // f6
        case 98:  return 0xFFC4;  // f7
        case 100: return 0xFFC5;  // f8
        case 101: return 0xFFC6;  // f9
        case 109: return 0xFFC7;  // f10
        case 103: return 0xFFC8;  // f11
        case 111: return 0xFFC9;  // f12
        case 49:  return 0x20;    // space
    }
    return -1;                                               // letters / digits get their code from the event
}

// flip the y coordinate: appkit view coords are bottom-up, we use top-down
static int y_flip(int win_h, double view_y) {
    int y = win_h - (int)(view_y + 0.5);                     // flip the origin
    if (y < 0) y = 0;                                        // clamp to the top edge
    if (y > win_h) y = win_h;                                // clamp to the bottom edge
    return y;
}

// translate a native nsevent into a ui window event
static bool translate_event(UiWindow* w, id ev, UiWinEvent* out) {
    long type = ((long (*)(id, SEL))objc_msgSend)(ev, OBJC_SEL("type"));  // event kind
    unsigned long long flags = ((unsigned long long (*)(id, SEL))objc_msgSend)(
        ev, OBJC_SEL("modifierFlags"));                      // modifier bitmask

    int mods = 0;                                            // build the apex modifier mask
    if (flags & NS_MOD_SHIFT)   mods |= 1;                   // shift
    if (flags & NS_MOD_CONTROL) mods |= 2;                   // control
    if (flags & NS_MOD_OPTION)  mods |= 4;                   // option / alt

    // locationinwindow returns nspoint (two doubles): no stret needed
    ApexNSPoint loc = { 0, 0 };                              // view-space location
    loc = ((ApexNSPoint (*)(id, SEL))objc_msgSend)(
        ev, OBJC_SEL("locationInWindow"));

    int px = (int)(loc.x + 0.5);                             // rounded x
    int py = y_flip(w->h, loc.y);                            // flipped y

    switch (type) {
        case NS_EV_LEFT_MOUSE_DOWN:
            out->kind   = UW_EV_MOUSEDOWN;                   // left press
            out->x      = px;
            out->y      = py;
            out->button = 1;
            return true;

        case NS_EV_LEFT_MOUSE_UP:
            out->kind   = UW_EV_MOUSEUP;                     // left release
            out->x      = px;
            out->y      = py;
            out->button = 1;
            return true;

        case NS_EV_RIGHT_MOUSE_DOWN:
        case NS_EV_OTHER_MOUSE_DOWN:
            out->kind   = UW_EV_MOUSEDOWN;                   // right / other press
            out->x      = px;
            out->y      = py;
            out->button = 3;
            return true;

        case NS_EV_RIGHT_MOUSE_UP:
        case NS_EV_OTHER_MOUSE_UP:
            out->kind   = UW_EV_MOUSEUP;                     // right / other release
            out->x      = px;
            out->y      = py;
            out->button = 3;
            return true;

        case NS_EV_MOUSE_MOVED:
        case NS_EV_LEFT_MOUSE_DRAGGED:
        case NS_EV_RIGHT_MOUSE_DRAGGED:
        case NS_EV_OTHER_MOUSE_DRAGGED:
            out->kind = UW_EV_MOUSEMOVE;                     // pointer moved
            out->x    = px;
            out->y    = py;
            return true;

        case NS_EV_SCROLL_WHEEL: {
            out->kind = UW_EV_SCROLL;                        // wheel scrolled
            out->x    = px;
            out->y    = py;
            double dy = ((double (*)(id, SEL))objc_msgSend)(
                ev, OBJC_SEL("scrollingDeltaY"));            // precise pixel delta
            double d  = ((double (*)(id, SEL))objc_msgSend)(
                ev, OBJC_SEL("deltaY"));                     // line-count fallback
            double use = (dy != 0.0) ? (dy / 10.0) : d;      // prefer precise, scale down
            if (use == 0.0) use = (dy < 0) ? 1.0 : -1.0;     // sign-only events
            out->scroll_dy = -use;                           // wheel up is negative in our convention
            return true;
        }

        case NS_EV_KEY_DOWN: {
            unsigned short kc = ((unsigned short (*)(id, SEL))objc_msgSend)(
                ev, OBJC_SEL("keyCode"));                    // virtual key code

            out->kind = UW_EV_KEYDOWN;                       // key pressed
            out->mods = mods;
            out->key  = macos_map_keycode(kc);               // translated keysym

            // grab the characters for printable keys
            id chars = ((id (*)(id, SEL))objc_msgSend)(ev, OBJC_SEL("characters"));
            if (chars) {
                const char* c = ((const char* (*)(id, SEL))objc_msgSend)(
                    chars, OBJC_SEL("UTF8String"));          // utf-8 representation
                if (c && *c) {
                    unsigned char c0 = (unsigned char)c[0];
                    // filter out the appkit private-use function keys and controls
                    if (c0 >= 0x20 && c0 != 0x7F &&
                        !(c0 == 0xEF && (unsigned char)c[1] == 0x9C)) {  // not u+f700..u+f8ff range
                        size_t n = strlen(c);
                        if (n < sizeof(out->utf8)) {
                            memcpy(out->utf8, c, n);         // copy the utf-8 payload
                            out->utf8[n] = 0;
                            out->utf8_len = (int)n;

                            // for a printable key with no special keysym, use the
                            // uppercased ascii code as the key value to match win32
                            if (out->key < 0 && n == 1) {
                                char ch = c[0];
                                if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
                                out->key = (unsigned char)ch;
                            }
                        }
                    } else {
                        out->utf8_len = 0;                   // non-printable: drop the text
                    }
                } else {
                    out->utf8_len = 0;                       // empty characters: no text
                }
            } else {
                out->utf8_len = 0;                           // no characters field: no text
            }

            // if we still don't have a key value, look at charactersignoringmodifiers
            if (out->key < 0) {
                id cim = ((id (*)(id, SEL))objc_msgSend)(
                    ev, OBJC_SEL("charactersIgnoringModifiers"));
                if (cim) {
                    const char* c = ((const char* (*)(id, SEL))objc_msgSend)(
                        cim, OBJC_SEL("UTF8String"));
                    if (c && c[0]) {
                        char ch = c[0];
                        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');  // uppercase
                        if ((unsigned char)ch >= 0x20 && (unsigned char)ch < 0x7F)
                            out->key = (unsigned char)ch;
                    }
                }
            }
            return true;
        }

        default:
            return false;                                    // unrecognized event kind
    }
}

// blocks until the server has input, or until the timeout expires
bool ui_window_wait(UiWindow* w, int timeout_ms) {
    if (!w || !w->ns_window) return false;                   // null guard
    if (w->pending_set) return true;                         // event already queued
    if (has_saved_for(w->ns_window)) return true;            // saved events already pending

    long start = (long)(CFAbsoluteTimeGetCurrent() * 1000.0);  // wall-clock start
    for (;;) {
        long elapsed = (long)(CFAbsoluteTimeGetCurrent() * 1000.0) - start;
        long remain  = timeout_ms - elapsed;
        if (remain < 0) remain = 0;                          // clamp to zero

        id ev = dequeue_event((double)remain / 1000.0);      // block for the remainder
        if (!ev) return false;                               // timeout with no event

        id ev_win = ((id (*)(id, SEL))objc_msgSend)(ev, OBJC_SEL("window"));  // target window

        if (ev_win == w->ns_window || ev_win == nil) {
            save_event(ev, ev_win);                          // keep for our poll
            if (ev_win == w->ns_window) return true;         // ours: ready to poll
            // nil window: app-level event, keep waiting
        } else {
            save_event(ev, ev_win);                          // belongs to another window: stash
        }

        if (elapsed >= timeout_ms) {
            return has_saved_for(w->ns_window) ? true : false;  // report saved or timed out
        }
    }
}

// polls the shared queue and translates the next event for this window
bool ui_window_poll(UiWindow* w, UiWinEvent* out) {
    out->kind = UW_EV_NONE;                                  // default to nothing
    if (!w) return false;                                    // null guard

    if (w->pending_set) {                                    // a queued event wins
        *out = w->pending;
        w->pending_set  = false;
        w->pending.kind = UW_EV_NONE;
        return true;
    }

    // drain events for this window first
    id ev = NULL;
    if (pop_saved_for(w->ns_window, &ev) && ev) {            // one of our saved events
        bool r = translate_event(w, ev, out);
        ((void (*)(id, SEL))objc_msgSend)(ev, OBJC_SEL("release"));  // drop our ref
        if (r) return true;
    }

    // then from the os queue
    for (int spin = 0; spin < 128; spin++) {                 // bounded drain
        id fresh = dequeue_event(0.0);                       // non-blocking
        if (!fresh) return false;                            // queue empty

        id ev_win = ((id (*)(id, SEL))objc_msgSend)(fresh, OBJC_SEL("window"));  // target

        if (ev_win == w->ns_window) {                        // ours
            bool r = translate_event(w, fresh, out);
            if (r) return true;
            // unmatched event types are just dropped
        } else if (ev_win == nil) {
            // app-level events: drop for now
        } else {
            save_event(fresh, ev_win);                       // belongs to another window: stash
        }
    }
    return false;                                            // nothing recognized this call
}

#endif
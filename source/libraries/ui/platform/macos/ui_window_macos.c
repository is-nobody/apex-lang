// source/libraries/ui/platform/macos/ui_window_macos.c
// Cocoa window core: create/destroy/resize/framebuffer for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef __APPLE__

#include "ui_window_macos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ApplicationServices/ApplicationServices.h>

// lookup a class by name
#define OBJC_CLS(n)  ((id)objc_getClass(n))

// intern a selector
#define OBJC_SEL(n)  sel_registerName(n)

// msgSend cast for id f()
#define MSG_id0(r, s)             ((id      (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))

// msgSend cast for id f(id)
#define MSG_id1(r, s, a)          ((id      (*)(id, SEL, id))objc_msgSend)((id)(r), (SEL)(s), (id)(a))

// msgSend cast for id f(nsinteger)
#define MSG_id_i(r, s, a)         ((id      (*)(id, SEL, NSInteger))objc_msgSend)((id)(r), (SEL)(s), (NSInteger)(a))

// msgSend cast for void f()
#define MSG_void0(r, s)           ((void    (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))

// msgSend cast for void f(id)
#define MSG_void1(r, s, a)        ((void    (*)(id, SEL, id))objc_msgSend)((id)(r), (SEL)(s), (id)(a))

// msgSend cast for void f(bool)
#define MSG_void_b(r, s, a)       ((void    (*)(id, SEL, BOOL))objc_msgSend)((id)(r), (SEL)(s), (BOOL)(a))

// msgSend cast for void f(nsinteger)
#define MSG_void_i(r, s, a)       ((void    (*)(id, SEL, NSInteger))objc_msgSend)((id)(r), (SEL)(s), (NSInteger)(a))

// msgSend cast for void f(double)
#define MSG_void_d(r, s, a)       ((void    (*)(id, SEL, double))objc_msgSend)((id)(r), (SEL)(s), (double)(a))

// msgSend cast for bool f()
#define MSG_bool0(r, s)           ((BOOL    (*)(id, SEL))objc_msgSend)((id)(r), (SEL)(s))

// window initialiser takes the rect as the first argument
typedef id (*InitWithRectFn)(id, SEL, ApexNSRect, NSInteger, NSInteger, BOOL);

// dispatch an initWithContentRect:styleMask:backing:defer: call
#define MSG_init_window(r, s, rect, style, backing, defer) \
    ((InitWithRectFn)objc_msgSend)((id)(r), (SEL)(s), (ApexNSRect)(rect), \
                                    (NSInteger)(style), (NSInteger)(backing), (BOOL)(defer))

// view initialiser takes only the rect
typedef id (*InitWithFrameFn)(id, SEL, ApexNSRect);

// dispatch an initWithFrame: call on a view
#define MSG_init_view(r, s, rect) \
    ((InitWithFrameFn)objc_msgSend)((id)(r), (SEL)(s), (ApexNSRect)(rect))

// setter taking an nssize-shaped struct by value
typedef void (*SetSizeFn)(id, SEL, ApexNSRect);

// dispatch a setter that takes an nssize struct
#define MSG_set_size(r, s, sz) ((SetSizeFn)objc_msgSend)((id)(r), (SEL)(s), (ApexNSRect)(sz))

// setter taking an nspoint-shaped struct by value
typedef void (*SetPointFn)(id, SEL, ApexNSRect);

// dispatch a setter that takes an nspoint struct
#define MSG_set_point(r, s, pt) ((SetPointFn)objc_msgSend)((id)(r), (SEL)(s), (ApexNSRect)(pt))

// associated object key for delegate -> uiwindow mapping
static const char kUiWindowKey;

// runtime classes, created on first use
static Class g_win_cls      = NULL;
static Class g_view_cls     = NULL;
static Class g_delegate_cls = NULL;
static int   g_app_inited   = 0;

// nswindowstylemask bits
#define NS_WIN_STYLE_TITLED          (1u << 0)
#define NS_WIN_STYLE_CLOSABLE        (1u << 1)
#define NS_WIN_STYLE_MINIATURIZABLE  (1u << 2)
#define NS_WIN_STYLE_RESIZABLE       (1u << 3)

// nsbackingstorebuffered
#define NS_BACKING_BUFFERED          2

// nsapplicationactivationpolicyregular
#define NS_APP_ACTIVATION_REGULAR    0

// nswindowlevel floating
#define NS_WINDOW_LEVEL_FLOATING     3

// a plain borderless nswindow refuses key status unless we say otherwise
static BOOL apex_win_can_become_key(id self, SEL _cmd) {
    (void)self; (void)_cmd; return YES;                      // borderless windows must accept keys
}

// borderless windows also need to opt in to main-window status
static BOOL apex_win_can_become_main(id self, SEL _cmd) {
    (void)self; (void)_cmd; return YES;                      // and become the main window
}

// the layer-backed view wants to be first responder so keys reach us
static BOOL apex_view_accepts_first(id self, SEL _cmd) {
    (void)self; (void)_cmd; return YES;                      // view participates in the responder chain
}

// top-down coordinates to match the rest of the ui library
static BOOL apex_view_is_flipped(id self, SEL _cmd) {
    (void)self; (void)_cmd; return YES;                      // match the win32/x11 origin convention
}

// we handle titlebar dragging ourselves
static BOOL apex_view_no_drag(id self, SEL _cmd) {
    (void)self; (void)_cmd; return NO;                       // do not drag from a mouse-down
}

// translate a system close request into a pending close event and refuse the actual close
static BOOL apex_delegate_should_close(id self, SEL _cmd, id sender) {
    (void)_cmd; (void)sender;                                // unused arguments
    UiWindow* w = (UiWindow*)objc_getAssociatedObject(self, &kUiWindowKey);  // retrieve the owning window
    if (w && w->open) {
        w->pending_set  = true;                              // queue a close event
        w->pending.kind = UW_EV_CLOSE;
        w->open         = false;
    }
    return NO;                                               // let the library decide when to destroy
}

// no-op resize notification; resize is detected by polling the frame
static void apex_delegate_did_resize(id self, SEL _cmd, id notif) {
    (void)self; (void)_cmd; (void)notif;                     // unused arguments
}

// build the three runtime classes; safe to call repeatedly
static void macos_setup_classes(void) {
    if (g_win_cls && g_view_cls && g_delegate_cls) return;   // already built

    if (!g_win_cls) {
        Class base = objc_getClass("NSWindow");              // nswindow as base class
        g_win_cls = objc_allocateClassPair(base, "ApexUiWindow", 0);
        class_addMethod(g_win_cls, OBJC_SEL("canBecomeKeyWindow"),
                        (IMP)apex_win_can_become_key, "B@:");  // enable key status
        class_addMethod(g_win_cls, OBJC_SEL("canBecomeMainWindow"),
                        (IMP)apex_win_can_become_main, "B@:"); // enable main status
        objc_registerClassPair(g_win_cls);
    }
    if (!g_view_cls) {
        Class base = objc_getClass("NSView");                // nsview as base class
        g_view_cls = objc_allocateClassPair(base, "ApexUiView", 0);
        class_addMethod(g_view_cls, OBJC_SEL("acceptsFirstResponder"),
                        (IMP)apex_view_accepts_first, "B@:");  // accept keyboard focus
        class_addMethod(g_view_cls, OBJC_SEL("isFlipped"),
                        (IMP)apex_view_is_flipped, "B@:");     // top-left origin
        class_addMethod(g_view_cls, OBJC_SEL("mouseDownCanMoveWindow"),
                        (IMP)apex_view_no_drag, "B@:");        // no implicit window drag
        objc_registerClassPair(g_view_cls);
    }
    if (!g_delegate_cls) {
        Class base = objc_getClass("NSObject");              // nsobject as base class
        g_delegate_cls = objc_allocateClassPair(base, "ApexUiWindowDelegate", 0);
        class_addMethod(g_delegate_cls, OBJC_SEL("windowShouldClose:"),
                        (IMP)apex_delegate_should_close, "B@:@");  // close handshake
        class_addMethod(g_delegate_cls, OBJC_SEL("windowDidResize:"),
                        (IMP)apex_delegate_did_resize, "v@:@");    // no-op resize notification
        objc_registerClassPair(g_delegate_cls);
    }
}

// return the apexuiwindow runtime class, building it on first use
Class macos_apex_window_class(void)   { macos_setup_classes(); return g_win_cls; }

// return the apexuiview runtime class, building it on first use
Class macos_apex_view_class(void)     { macos_setup_classes(); return g_view_cls; }

// return the apexuiwindowdelegate runtime class, building it on first use
Class macos_apex_delegate_class(void) { macos_setup_classes(); return g_delegate_cls; }

// initialise the shared nsapplication and set its activation policy
int macos_init_app(void) {
    if (g_app_inited) return 0;                              // already initialised

    id app = MSG_id0(OBJC_CLS("NSApplication"), OBJC_SEL("sharedApplication"));
    if (!app) return -1;                                     // no nsapplication: bail

    MSG_void_i(app, OBJC_SEL("setActivationPolicy:"), NS_APP_ACTIVATION_REGULAR);  // show in the dock

    MSG_void0(app, OBJC_SEL("finishLaunching"));             // complete startup before any window

    g_app_inited = 1;
    return 0;
}

// build a fresh nsstring from a utf-8 c string
id macos_nsstring_from_utf8(const char* s) {
    if (!s) s = "";                                          // null guard
    id alloc = MSG_id0(OBJC_CLS("NSString"), OBJC_SEL("alloc"));
    return MSG_id1(alloc, OBJC_SEL("initWithUTF8String:"), (id)s);  // owned nsstring
}

// duplicate an nsstring's utf-8 contents into a malloc'd buffer
char* macos_utf8_from_nsstring(id nsstr) {
    if (!nsstr) return NULL;
    const char* c = ((const char* (*)(id, SEL))objc_msgSend)(
        (id)nsstr, OBJC_SEL("UTF8String"));                  // borrow the utf-8 buffer
    if (!c) return NULL;
    size_t n = strlen(c);
    char* out = (char*)malloc(n + 1);                        // caller owns the copy
    if (!out) return NULL;
    memcpy(out, c, n + 1);
    return out;
}

// allocate a zeroed pixel buffer sized for the given dimensions
static int alloc_pixels(UiWindow* w, int pw, int ph) {
    w->pixels = (uint32_t*)calloc((size_t)pw * (size_t)ph, 4);  // bgra buffer
    if (!w->pixels) return 0;                                // oom
    w->w = pw;
    w->h = ph;
    return 1;
}

// release the current pixel buffer
static void free_pixels(UiWindow* w) {
    free(w->pixels);                                         // release the pixel buffer
    w->pixels = NULL;
}

// creates and shows a native window, decorated=false strips system chrome
UiWindow* ui_window_create(int w, int h, const char* title, bool decorated) {
    macos_setup_classes();                                   // build the runtime classes
    if (macos_init_app() < 0) return NULL;                   // need nsapplication running

    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    if (!win) return NULL;

    win->decorated     = decorated;                          // remember chrome preference
    win->open          = true;
    win->backing_scale = 1.0;
    win->point_w       = w;
    win->point_h       = h;

    if (!alloc_pixels(win, w, h)) { free(win); return NULL; }  // allocate the bgra buffer

    NSInteger style = 0;                                     // borderless by default
    if (decorated) {
        style = NS_WIN_STYLE_TITLED                          // normal titlebar
              | NS_WIN_STYLE_CLOSABLE                        // close button
              | NS_WIN_STYLE_MINIATURIZABLE                  // minimize button
              | NS_WIN_STYLE_RESIZABLE;                      // resize handles
    }

    ApexNSRect rect = { 120, 120, (double)w, (double)h };    // initial placement

    id alloc = MSG_id0((id)g_win_cls, OBJC_SEL("alloc"));
    win->ns_window = MSG_init_window(alloc,
        OBJC_SEL("initWithContentRect:styleMask:backing:defer:"),
        rect, style, NS_BACKING_BUFFERED, NO);
    if (!win->ns_window) { free_pixels(win); free(win); return NULL; }

    id ns_title = macos_nsstring_from_utf8(title ? title : "Apex UI");  // utf-8 -> nsstring
    MSG_void1(win->ns_window, OBJC_SEL("setTitle:"), ns_title);
    MSG_void0(ns_title, OBJC_SEL("release"));

    id valloc = MSG_id0((id)g_view_cls, OBJC_SEL("alloc"));
    win->ns_view = MSG_init_view(valloc, OBJC_SEL("initWithFrame:"), rect);  // content view
    MSG_void_b(win->ns_view, OBJC_SEL("setWantsLayer:"), YES);   // layer-backed so we can blit
    MSG_void1(win->ns_window, OBJC_SEL("setContentView:"), win->ns_view);

    id dalloc = MSG_id0((id)g_delegate_cls, OBJC_SEL("alloc"));
    win->ns_delegate = MSG_id0(dalloc, OBJC_SEL("init"));    // window delegate
    objc_setAssociatedObject(win->ns_delegate, &kUiWindowKey, (id)win,
                             OBJC_ASSOCIATION_ASSIGN);       // weak back-pointer to the window
    MSG_void1(win->ns_window, OBJC_SEL("setDelegate:"), win->ns_delegate);

    MSG_void_b(win->ns_window, OBJC_SEL("setAcceptsMouseMovedEvents:"), YES);  // hover without press

    win->ns_layer = MSG_id0(win->ns_view, OBJC_SEL("layer"));  // the backing calayer
    if (win->ns_layer) {
        id gravity = macos_nsstring_from_utf8("resize");     // stretch the image to fill
        MSG_void1(win->ns_layer, OBJC_SEL("setContentsGravity:"), gravity);
        MSG_void0(gravity, OBJC_SEL("release"));
        MSG_void_d(win->ns_layer, OBJC_SEL("setContentsScale:"), 1.0);  // 1:1 with our pixels
    }

    MSG_void1(win->ns_window, OBJC_SEL("makeKeyAndOrderFront:"), (id)nil);  // show and focus
    id app = MSG_id0(OBJC_CLS("NSApplication"), OBJC_SEL("sharedApplication"));
    MSG_void_b(app, OBJC_SEL("activateIgnoringOtherApps:"), YES);  // bring the app forward

    return win;
}

// destroys the window and releases every resource it owns
void ui_window_destroy(UiWindow* w) {
    if (!w) return;                                          // null guard

    if (w->cg_image) {                                       // drop the cached cgimage
        CGImageRelease(w->cg_image);
        w->cg_image = NULL;
    }

    if (w->ns_window) {
        MSG_void1(w->ns_window, OBJC_SEL("setDelegate:"), (id)nil);  // detach the delegate
        MSG_void0(w->ns_window, OBJC_SEL("close"));          // close the ns window
        MSG_void0(w->ns_window, OBJC_SEL("release"));        // we alloc'd it
        w->ns_window = NULL;
    }
    if (w->ns_view) {
        MSG_void0(w->ns_view, OBJC_SEL("release"));          // content view, we alloc'd it
        w->ns_view = NULL;
    }
    if (w->ns_delegate) {
        MSG_void0(w->ns_delegate, OBJC_SEL("release"));      // delegate, we alloc'd it
        w->ns_delegate = NULL;
    }
    w->ns_layer = NULL;                                      // borrowed from the view

    free_pixels(w);                                          // free the pixel buffer
    free(w->clipboard_text);                                 // free our clipboard copy
    free(w);                                                 // free the window struct
}

// updates the native window title
void ui_window_set_title(UiWindow* w, const char* t) {
    if (!w || !w->ns_window) return;                         // null guard
    id s = macos_nsstring_from_utf8(t ? t : "");             // utf-8 -> nsstring
    MSG_void1(w->ns_window, OBJC_SEL("setTitle:"), s);
    MSG_void0(s, OBJC_SEL("release"));
}

// stores a paint callback the window can invoke on redraw
void ui_window_set_paint_callback(UiWindow* w, UiPaintCallback cb, void* user) {
    if (!w) return;                                          // null guard
    w->paint_cb   = cb;
    w->paint_user = user;
}

// returns a pointer to the current framebuffer
uint32_t* ui_window_pixels(UiWindow* w) { return w ? w->pixels : NULL; }

// current window width in pixels
int ui_window_width (UiWindow* w)       { return w ? w->w : 0; }

// current window height in pixels
int ui_window_height(UiWindow* w)       { return w ? w->h : 0; }

// resizes the window's backing storage to new pixel dimensions
void ui_window_resize(UiWindow* w, int nw, int nh) {
    if (!w || nw <= 0 || nh <= 0) return;                    // ignore degenerate sizes
    if (nw == w->w && nh == w->h) return;                    // unchanged

    free_pixels(w);                                          // drop the old buffer
    if (!alloc_pixels(w, nw, nh)) return;                    // allocate the new one
    w->point_w = nw;
    w->point_h = nh;

    if (w->ns_window) {
        ApexNSRect sz = { 0, 0, (double)nw, (double)nh };    // new content size
        MSG_set_size(w->ns_window, OBJC_SEL("setContentSize:"), sz);
    }

    if (w->paint_cb) w->paint_cb(w->paint_user, w->w, w->h); // notify the host
}

// blits only the given rects; we always refresh the whole surface
void ui_window_present_rects(UiWindow* w, const int* quads, int n) {
    (void)quads; (void)n;                                    // whole-surface refresh
    if (!w || !w->ns_layer || !w->pixels) return;            // nothing to present

    if (w->cg_image) {                                       // drop the previous frame
        CGImageRelease(w->cg_image);
        w->cg_image = NULL;
    }

    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, w->pixels, (size_t)w->w * (size_t)w->h * 4, NULL);  // wrap the cpu buffer
    if (!provider) return;                                   // provider alloc failed

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();      // device rgb colorspace
    if (!cs) { CGDataProviderRelease(provider); return; }    // colorspace alloc failed

    // our pixels are aarrggbb stored as bytes b, g, r, a on little-endian
    // machines. that matches kcgimagealphafirst with kcgbitmapbyteorder32little.
    // we never pre-multiply, so use the non-premultiplied alpha variant.
    CGBitmapInfo info = kCGImageAlphaFirst | kCGBitmapByteOrder32Little;

    CGImageRef img = CGImageCreate(
        w->w, w->h, 8, 32, (size_t)w->w * 4,                // dims, bpc, bpp, bytes-per-row
        cs, info, provider, NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);                                 // release the colorspace
    CGDataProviderRelease(provider);                         // release the provider
    if (!img) return;                                        // image creation failed

    w->cg_image = img;                                       // keep for next release
    MSG_void1(w->ns_layer, OBJC_SEL("setContents:"), (id)img);  // publish to the layer
}

// blits the current framebuffer to the window
void ui_window_present(UiWindow* w) {
    if (!w) return;                                          // null guard
    int q[4] = { 0, 0, w->w, w->h };                         // whole-window rect
    ui_window_present_rects(w, q, 1);
}

// true while the window has not been closed
bool ui_window_is_open(UiWindow* w) { return w && w->open; }

// marks the window as closed and hides it
void ui_window_close(UiWindow* w) {
    if (!w) return;                                          // null guard
    w->open = false;
    if (w->ns_window) MSG_void1(w->ns_window, OBJC_SEL("orderOut:"), (id)nil);  // hide the window
}

// true when the window manager reports us as maximized
bool ui_window_is_maximized(UiWindow* w) {
    if (!w || !w->ns_window) return false;                   // null guard
    return MSG_bool0(w->ns_window, OBJC_SEL("isZoomed")) ? true : false;  // nswindow zoomed state
}

// asks appkit to iconify the window
void ui_window_minimize(UiWindow* w) {
    if (!w || !w->ns_window) return;                         // null guard
    MSG_void1(w->ns_window, OBJC_SEL("miniaturize:"), (id)nil);  // ask appkit to minimize
}

// toggles between zoomed and normal via nswindow zoom
void ui_window_toggle_maximize(UiWindow* w) {
    if (!w || !w->ns_window) return;                         // null guard
    MSG_void1(w->ns_window, OBJC_SEL("zoom:"), (id)nil);     // toggle zoom state
}

// clamps the window to a minimum client size via content-size hints
void ui_window_set_min_size(UiWindow* w, int min_w, int min_h) {
    if (!w || min_w <= 0 || min_h <= 0) return;              // ignore degenerate requests
    w->min_w = min_w;
    w->min_h = min_h;
    if (w->ns_window) {
        ApexNSRect sz = { 0, 0, (double)min_w, (double)min_h };  // content-size floor
        MSG_set_size(w->ns_window, OBJC_SEL("setMinSize:"), sz);
    }
}

// delegates an interactive move to appkit
void ui_window_begin_drag(UiWindow* w) {
    if (!w || !w->ns_window) return;                         // null guard
    id app = MSG_id0(OBJC_CLS("NSApplication"), OBJC_SEL("sharedApplication"));
    id cur = MSG_id0(app, OBJC_SEL("currentEvent"));         // the mouse-down event
    if (cur) MSG_void1(w->ns_window, OBJC_SEL("performWindowDragWithEvent:"), cur);
}

// decorated windows have system resize handles; borderless resize comes from the event loop
void ui_window_begin_resize(UiWindow* w, const char* edge) {
    (void)w; (void)edge;                                     // no-op on this backend
}

// the system cursor is set by appkit for decorated windows
void ui_window_set_resize_cursor(UiWindow* w, const char* edge) {
    if (w) w->cursor_edge = edge;                            // remember for future use
}

// creates a frameless, floating popup window used for notifications
UiWindow* ui_window_create_popup(int x, int y, int w, int h, const char* title) {
    macos_setup_classes();                                   // build the runtime classes
    if (macos_init_app() < 0) return NULL;                   // need nsapplication running

    UiWindow* win = (UiWindow*)calloc(1, sizeof(UiWindow));  // zeroed window struct
    if (!win) return NULL;

    win->decorated     = false;                              // no chrome
    win->is_popup      = true;                               // frameless notification
    win->open          = true;
    win->backing_scale = 1.0;
    win->point_w       = w;
    win->point_h       = h;

    if (!alloc_pixels(win, w, h)) { free(win); return NULL; }  // allocate the bgra buffer

    ApexNSRect rect = { (double)x, (double)y, (double)w, (double)h };  // requested placement
    id alloc = MSG_id0((id)g_win_cls, OBJC_SEL("alloc"));
    win->ns_window = MSG_init_window(alloc,
        OBJC_SEL("initWithContentRect:styleMask:backing:defer:"),
        rect, 0, NS_BACKING_BUFFERED, NO);                   // borderless popup
    if (!win->ns_window) { free_pixels(win); free(win); return NULL; }

    id ns_title = macos_nsstring_from_utf8(title ? title : "");  // may be empty
    MSG_void1(win->ns_window, OBJC_SEL("setTitle:"), ns_title);
    MSG_void0(ns_title, OBJC_SEL("release"));

    MSG_void_b(win->ns_window, OBJC_SEL("setOpaque:"), NO);  // transparent shell
    id clear = MSG_id0(OBJC_CLS("NSColor"), OBJC_SEL("clearColor"));
    MSG_void1(win->ns_window, OBJC_SEL("setBackgroundColor:"), clear);

    MSG_void_i(win->ns_window, OBJC_SEL("setLevel:"), NS_WINDOW_LEVEL_FLOATING);  // float above normal
    MSG_void_b(win->ns_window, OBJC_SEL("setHasShadow:"), NO);   // no drop shadow on notifications

    id valloc = MSG_id0((id)g_view_cls, OBJC_SEL("alloc"));
    win->ns_view = MSG_init_view(valloc, OBJC_SEL("initWithFrame:"), rect);  // content view
    MSG_void_b(win->ns_view, OBJC_SEL("setWantsLayer:"), YES);   // layer-backed
    MSG_void1(win->ns_window, OBJC_SEL("setContentView:"), win->ns_view);

    id dalloc = MSG_id0((id)g_delegate_cls, OBJC_SEL("alloc"));
    win->ns_delegate = MSG_id0(dalloc, OBJC_SEL("init"));    // window delegate
    objc_setAssociatedObject(win->ns_delegate, &kUiWindowKey, (id)win,
                             OBJC_ASSOCIATION_ASSIGN);       // weak back-pointer to the window
    MSG_void1(win->ns_window, OBJC_SEL("setDelegate:"), win->ns_delegate);
    MSG_void_b(win->ns_window, OBJC_SEL("setAcceptsMouseMovedEvents:"), YES);  // hover events

    win->ns_layer = MSG_id0(win->ns_view, OBJC_SEL("layer"));  // the backing calayer
    if (win->ns_layer) {
        id gravity = macos_nsstring_from_utf8("resize");     // stretch to fill
        MSG_void1(win->ns_layer, OBJC_SEL("setContentsGravity:"), gravity);
        MSG_void0(gravity, OBJC_SEL("release"));
        MSG_void_d(win->ns_layer, OBJC_SEL("setContentsScale:"), 1.0);  // 1:1 with our pixels
        MSG_void_b(win->ns_layer, OBJC_SEL("setOpaque:"), NO);   // layer keeps alpha

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();  // transparent layer background
        if (cs) {
            CGFloat comps[4] = { 0, 0, 0, 0 };               // rgba(0,0,0,0)
            CGColorRef cc = CGColorCreate(cs, comps);
            if (cc) {
                MSG_void1(win->ns_layer, OBJC_SEL("setBackgroundColor:"), (id)cc);
                CGColorRelease(cc);
            }
            CGColorSpaceRelease(cs);
        }
    }

    MSG_void1(win->ns_window, OBJC_SEL("orderFront:"), (id)nil);  // show without activating

    return win;
}

// moves a window to an absolute screen position
void ui_window_move(UiWindow* w, int x, int y) {
    if (!w || !w->ns_window) return;                         // null guard
    ApexNSRect pt = { (double)x, (double)y, 0, 0 };          // top-left point in screen coords
    MSG_set_point(w->ns_window, OBJC_SEL("setFrameTopLeftPoint:"), pt);
}

// returns the monitor work area the window sits on
void ui_window_work_area(UiWindow* w, int* ox, int* oy, int* ow, int* oh) {
    (void)w;                                                 // the query is screen-wide, not window-wide
    id screen = MSG_id0(OBJC_CLS("NSScreen"), OBJC_SEL("mainScreen"));  // primary screen
    if (screen) {
        ApexNSRect f;                                        // visible frame
        ApexNSRect full;                                     // full frame, for the height
#if defined(__x86_64__) && !defined(__arm64__)
        // x86_64 sysv returns structs larger than 16 bytes via a hidden pointer
        typedef void (*StretFn)(ApexNSRect*, id, SEL);       // (ret, self, sel)
        StretFn visible = (StretFn)objc_msgSend_stret;
        StretFn frame   = (StretFn)objc_msgSend_stret;
        visible(&f,    screen, OBJC_SEL("visibleFrame"));
        frame  (&full, screen, OBJC_SEL("frame"));
#else
        // arm64 uses a hidden x8 pointer
        f    = ((ApexNSRect (*)(id, SEL))objc_msgSend)(screen,
            OBJC_SEL("visibleFrame"));
        full = ((ApexNSRect (*)(id, SEL))objc_msgSend)(screen,
            OBJC_SEL("frame"));
#endif
        int screen_h = (int)full.h;                          // nsscreen origin is bottom-left
        *ox = (int)f.x;                                      // left edge of the work area
        *oy = screen_h - (int)(f.y + f.h);                   // flip to top-left origin
        *ow = (int)f.w;                                      // work area width
        *oh = (int)f.h;                                      // work area height
        return;
    }
    // fallback: use the raw display bounds
    CGRect b = CGDisplayBounds(CGMainDisplayID());           // main display rect
    *ox = (int)b.origin.x;
    *oy = (int)b.origin.y;
    *ow = (int)b.size.width;
    *oh = (int)b.size.height;
}

// forces an immediate repaint of the window
void ui_window_invalidate(UiWindow* w) {
    if (!w || !w->ns_view) return;                           // null guard
    MSG_void_b(w->ns_view, OBJC_SEL("setNeedsDisplay:"), YES);  // mark the view dirty
    if (w->paint_cb) w->paint_cb(w->paint_user, w->w, w->h); // and let the host redraw
}

#endif
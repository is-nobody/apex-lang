// source/libraries/ui/widgets/ui_notif.c
// Notification popup lifecycle for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_window.h"
#include "ui_render.h"
#include <stdlib.h>
#include <string.h>

// duplicates a c string, mapping null to ""
static char* notif_dup(const char* s) {
    if (!s) s = "";                                      // null guard: treat as empty
    size_t n = strlen(s);                                // payload length
    char* out = (char*)malloc(n + 1);                    // payload plus terminator
    if (!out) return NULL;                               // oom: hand back null
    memcpy(out, s, n + 1);                               // copy bytes and null
    return out;                                          // caller owns the copy
}

// releases every owned allocation and zeroes the notification
void notif_clear(UiNotification* n) {
    if (!n) return;                                      // null guard
    if (n->window) ui_window_destroy(n->window);         // tear down the popup window
    free(n->key);                                        // release the script key
    free(n->title);                                      // release the title
    free(n->message);                                    // release the body text
    memset(n, 0, sizeof(*n));                            // wipe every field
}

// places every live notification stacked from the bottom-right corner
void notif_place_all(UiContext* ctx) {
    int from_bottom = 0;                                 // running vertical offset from the bottom
    for (int i = ctx->notif_count - 1; i >= 0; i--) {    // newest first: sits closest to the corner
        UiNotification* n = ctx->notifs[i];
        if (!n->window) continue;                        // skip entries without a live window

        int px = n->work_x + n->work_w - n->win_w - UI_NOTIF_MARGIN;  // right edge of the work area
        int py = n->work_y + n->work_h - n->win_h - UI_NOTIF_MARGIN - from_bottom;  // stacked upward

        // ease-out cubic: fast takeoff, settles gently into place
        double t = n->anim_t;                            // 0 = off-screen, 1 = settled
        if (t < 0) t = 0;                                // clamp low
        if (t > 1) t = 1;                                // clamp high
        double inv  = 1.0 - t;                           // complement of the phase
        double ease = 1.0 - inv*inv*inv;                 // cubic ease-out curve

        // slide in from the right while opening, slide out to the right while closing
        px += (int)((1.0 - ease) * UI_NOTIF_SLIDE_MAX);

        if (px != n->last_x || py != n->last_y) {        // only move when the target changed
            ui_window_move(n->window, px, py);           // push the new position
            n->last_x = px;                              // remember what we set
            n->last_y = py;
        }
        n->slot = from_bottom;                           // record the slot for debug or future use
        from_bottom += n->win_h + UI_NOTIF_GAP;          // advance the stack cursor
    }
}

// emits a notification close event to the script queue
void notif_emit_close(UiContext* ctx, UiNotification* n) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));              // zero the event
    ev.kind = EV_NOTIFICATION;                           // notification kind
    strncpy(ev.widget_type, "notification", 15);         // widget type name
    if (n->key) strncpy(ev.key_name, n->key, 63);        // propagate the script key
    ev.bool_value = true;                                // closed
    ui_event_push(ctx, &ev);                             // enqueue the event
}

// computes the intrinsic size of a notification box
static void notif_measure(UiContext* ctx, UiNotification* n, int* out_w, int* out_h) {
    int w = UI_NOTIF_W;                                  // fixed outer width
    int h = UI_NOTIF_PAD;                                // start with top padding

    float title_h = 0;                                   // vertical space reserved for the title
    if (n->title && *n->title) title_h = UI_NOTIF_TITLE_FS + 6;  // line box plus gap
    h += (int)title_h;                                   // add to the running height

    // message: count wrapped lines against the inner width
    int inner = w - 2*UI_NOTIF_PAD;                      // usable width inside the padding
    if (n->show_close) inner -= UI_NOTIF_CLOSE_SZ + 6;   // reserve room for the close button

    int lines = 0;                                       // wrapped line count
    if (n->message && *n->message) {                     // only when there is body text
        const char* s = n->message;
        int len = (int)strlen(s);
        float cur = 0;                                   // width of the current line
        int i = 0;
        lines = 1;                                       // assume at least one line
        while (i < len) {
            while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;  // skip leading whitespace
            if (i >= len) break;                         // nothing left to scan
            int ws = i;                                  // first byte of this word
            while (i < len && s[i] != ' ' && s[i] != '\t') i++;  // advance past the word
            float ww = ui_text_measure_cached(ctx, s + ws, i - ws, UI_NOTIF_BODY_FS);  // word width
            float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, UI_NOTIF_BODY_FS) : 0;  // space before it
            if (cur + sp + ww > (float)inner && cur > 0) { lines++; cur = ww; }  // wrap: start new line
            else                                          { cur += sp + ww; }   // append to current line
        }
    }
    h += lines * UI_NOTIF_LINE_H;                        // add the wrapped block height
    h += UI_NOTIF_PAD;                                   // bottom padding

    if (h < 60) h = 60;                                  // enforce a minimum box height
    *out_w = w;                                          // publish the width
    *out_h = h;                                          // publish the height
}

// creates and queues a new notification, sliding it in
void ui_notification_show(UiContext* ctx, const char* key, const char* title,
                          const char* message, double timeout, bool show_close) {
    if (ctx->notif_count >= UI_NOTIF_MAX) {              // cap reached: evict the oldest
        notif_clear(ctx->notifs[0]);                     // release its resources
        free(ctx->notifs[0]);                            // free the struct
        memmove(&ctx->notifs[0], &ctx->notifs[1],        // shift the rest forward
                sizeof(UiNotification*) * (ctx->notif_count - 1));
        ctx->notif_count--;                              // one fewer live entry
    }

    UiNotification* n = (UiNotification*)calloc(1, sizeof(UiNotification));  // zeroed struct
    if (!n) return;                                      // oom: give up

    n->ctx         = ctx;                                // back-pointer for later calls
    n->key         = key    ? notif_dup(key)     : NULL; // copy the script key
    n->title       = title  ? notif_dup(title)   : NULL; // copy the title
    n->message     = message ? notif_dup(message) : NULL; // copy the body text
    n->timeout     = timeout > 0 ? timeout : 0;          // non-positive means manual close
    n->elapsed     = 0;                                  // no time elapsed yet
    n->anim_t      = 0;                                  // start fully transparent and offset
    n->closing     = false;                              // not fading out
    n->anim_started_at = apex_now_seconds();             // anchor the open on wall-clock time
    n->show_close  = show_close;                         // honour the close button flag

    notif_measure(ctx, n, &n->win_w, &n->win_h);         // resolve the popup size

    // resolve and cache the work area once; later frames skip the x roundtrip
    int wx = 0, wy = 0, ww = 1920, wh = 1080;            // sane defaults for headless
    if (ctx->window) ui_window_work_area(ctx->window, &wx, &wy, &ww, &wh);  // live query
    n->work_x = wx; n->work_y = wy;                      // cache the origin
    n->work_w = ww; n->work_h = wh;                      // cache the size

    // start off-screen to the right so the slide-in is visible from frame 0
    int final_x   = wx + ww - n->win_w - UI_NOTIF_MARGIN;  // resting position
    int final_y   = wy + wh - n->win_h - UI_NOTIF_MARGIN;  // bottom-right corner
    int initial_x = final_x + UI_NOTIF_SLIDE_MAX;          // offset start for the slide

    n->window = ui_window_create_popup(initial_x, final_y, n->win_w, n->win_h,
                                       (n->title && *n->title) ? n->title : "notification");
    if (!n->window) { notif_clear(n); free(n); return; } // creation failed: bail out
    n->last_x = initial_x;                               // remember the spawn position
    n->last_y = final_y;

    if (ctx->notif_count >= ctx->notif_cap) {            // grow the pointer array
        ctx->notif_cap = ctx->notif_cap ? ctx->notif_cap * 2 : 4;  // double, or start at 4
        ctx->notifs = (UiNotification**)realloc(ctx->notifs,
                        sizeof(UiNotification*) * ctx->notif_cap);  // grow
    }
    ctx->notifs[ctx->notif_count++] = n;                 // publish the new entry

    // draw the first frame immediately, then stack everyone
    ui_render_notification(n);                           // paint the initial state
    notif_place_all(ctx);                                // place it and re-flow the stack
}

// closes a notification, optionally emitting a script event
void ui_notification_close(UiContext* ctx, int index, bool emit) {
    if (index < 0 || index >= ctx->notif_count) return;  // bounds check
    UiNotification* n = ctx->notifs[index];
    if (n->closing) return;                              // already closing
    n->closing = true;                                   // start the slide-out
    n->anim_started_at = apex_now_seconds();             // anchor the close on wall-clock time
    if (emit) notif_emit_close(ctx, n);                  // notify the script
}

// pumps events from every notification popup window
void ui_notification_pump(UiContext* ctx) {
    for (int i = 0; i < ctx->notif_count; i++) {         // walk every live notification
        UiNotification* n = ctx->notifs[i];
        if (!n->window) continue;                        // no window: nothing to pump

        UiWinEvent we;
        while (ui_window_poll(n->window, &we)) {         // drain this window's queue
            switch (we.kind) {
                case UW_EV_CLOSE:                        // system close request
                    ui_notification_close(ctx, i, true); // fade out and emit
                    break;
                case UW_EV_EXPOSE:                       // window needs a repaint
                    ui_render_notification(n);           // redraw into its framebuffer
                    break;
                case UW_EV_MOUSEMOVE: {                  // pointer moved over the popup
                    int cx = n->win_w - UI_NOTIF_PAD - UI_NOTIF_CLOSE_SZ / 2;  // close center x
                    int cy = UI_NOTIF_PAD + UI_NOTIF_CLOSE_SZ / 2;             // close center y
                    int dx = we.x - cx, dy = we.y - cy;  // offset from the close center
                    bool over = n->show_close &&         // is the pointer over the close glyph?
                                (dx*dx + dy*dy) < (UI_NOTIF_CLOSE_SZ * UI_NOTIF_CLOSE_SZ / 4);
                    if (over != n->hover_close) {        // hover state changed
                        n->hover_close = over;           // store the new flag
                        ui_render_notification(n);       // repaint to show the highlight
                    }
                    break;
                }
                case UW_EV_MOUSEDOWN:                    // press inside the popup
                    if (we.button == 1 && n->hover_close) {  // left click on the close button
                        n->press_close = true;           // latch the pressed state
                        ui_render_notification(n);       // repaint
                    }
                    break;
                case UW_EV_MOUSEUP:                      // release inside the popup
                    if (we.button == 1) {                // left button only
                        bool was = n->press_close;       // remember the press state
                        n->press_close = false;          // clear it
                        if (was && n->hover_close)       // released on the close button
                            ui_notification_close(ctx, i, true);  // accept the close
                        ui_render_notification(n);       // repaint either way
                    }
                    break;
                default: break;                          // ignore everything else
            }
        }
    }
}

// advances slide-in, slide-out and auto-close timers
void ui_notification_tick(UiContext* ctx, double dt) {
    double now = apex_now_seconds();                     // one wall-clock sample for the whole pass
    for (int i = 0; i < ctx->notif_count; ) {            // manual index: entries may be removed
        UiNotification* n = ctx->notifs[i];
        bool dirty = false;                              // did anything change this tick?

        if (n->closing) {                                // sliding out to the right
            // derive the phase from wall-clock time
            double t = 1.0 - (now - n->anim_started_at) / UI_NOTIF_CLOSE_SEC;
            if (t <= 0.0) {                              // fully off-screen: remove it
                n->anim_t = 0.0;
                ui_render_notification(n);               // paint one last empty frame
                notif_clear(n);                          // release resources
                free(n);                                 // free the struct
                memmove(&ctx->notifs[i], &ctx->notifs[i + 1],
                        sizeof(UiNotification*) * (ctx->notif_count - i - 1));
                ctx->notif_count--;                      // one fewer entry
                notif_place_all(ctx);                    // re-flow the stack
                continue;                                // do not advance i: same slot has a new entry
            }
            if (t > 1.0) t = 1.0;                        // clamp at fully open
            if (t != n->anim_t) {                        // phase changed
                n->anim_t = t;
                dirty = true;                            // repaint
            }
        } else {
            if (n->anim_t < 1.0) {                       // still sliding in
                // anim_started_at was seeded in ui_notification_show
                double t = (now - n->anim_started_at) / UI_NOTIF_OPEN_SEC;
                if (t > 1.0) t = 1.0;                    // clamp at fully open
                if (t != n->anim_t) {                    // phase changed
                    n->anim_t = t;
                    dirty = true;                        // repaint
                }
            }
            n->elapsed += dt;                            // accumulate open time
            if (n->timeout > 0 && n->elapsed >= n->timeout) {  // auto-close window elapsed
                n->closing = true;                       // start the slide-out silently
                n->anim_started_at = now;                // anchor the close from this tick
                dirty = true;                            // repaint
            }
        }

        if (dirty) {
            notif_place_all(ctx);                        // recompute the slide offset
            ui_render_notification(n);                   // repaint this popup
        }
        i++;                                             // advance to the next entry
    }
}

// releases every live notification and their popup windows
void ui_notification_destroy(UiContext* ctx) {
    for (int i = 0; i < ctx->notif_count; i++) {         // walk every live notification
        notif_clear(ctx->notifs[i]);                     // release its resources
        free(ctx->notifs[i]);                            // free the struct
    }
    ctx->notif_count = 0;                                // no more live entries
    free(ctx->notifs);                                   // free the pointer array
    ctx->notifs = NULL;                                  // clear dangling pointer
    ctx->notif_cap = 0;                                  // reset capacity
}
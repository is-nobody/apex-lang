// source/libraries/ui/core/ui_runtime.c
// Message loop and builtin entry points for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_hash.h"
#include "ui_table.h"
#include "ui_widgets.h"
#include "ui_interaction.h"
#include "ui_render.h"
#include "ui_window.h"
#include "ui_text.h"
#include <string.h>
#include <stdlib.h>

// rebuilds the widget tree from the last descriptor and paints it
void rebuild_and_draw(UiContext* ctx) {
    if (!ctx->window) {                                           // window is gone: nothing to draw into
        ctx->redraw_needed = false;
        ctx->full_repaint  = false;
        return;
    }

    // decide whether we can reuse the tree from the previous frame
    bool can_reuse = false;
    uint64_t new_hash = 0;

    if (ctx->has_tree && ctx->descr_hash_valid && ctx->root)
    {
        if (ctx->last_tree == ctx->hashed_tree) {
            new_hash = ctx->last_descr_hash;                      // same Value, cached hash
        } else {
            new_hash = hash_descr_recursive(ctx, ctx->last_tree);
        }
        if (new_hash == ctx->last_descr_hash) can_reuse = true;
    }

    if (!can_reuse) {
        ui_widgets_reset(ctx);                                    // drop the previous tree
        if (ctx->has_tree) {
            if (IS_TABLE(ctx->last_tree)) {
                Table* t = AS_TABLE(ctx->last_tree);
                Value first;
                if (table_get(t, MAKE_NUMBER(1.0), &first)) {     // array-style: multiple root children
                    value_decref(first);
                    Widget* root = ui_widget_alloc(ctx);          // synthesize a vbox root
                    root->kind = WK_VBOX;
                    root->id = ui_hash_id(0, "__root__", 0);
                    root->padding = UI_PAD_SCREEN;
                    root->gap = UI_GAP;
                    for (int i = 1; ; i++) {                      // walk 1-based array children
                        Value vv;
                        if (!table_get(t, MAKE_NUMBER((double)i), &vv)) break;
                        Widget* ch = ui_build_tree(ctx, vv, root, i - 1);
                        if (ch) widget_add_child(ctx, root, ch);
                        value_decref(vv);
                    }
                    ctx->root = root;
                } else {                                          // single descriptor root
                    ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);
                }
            } else {
                ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);  // non-table descriptor
            }
        }
        if (!ctx->root) {                                         // no tree: synthesize an empty root
            Widget* root = ui_widget_alloc(ctx);
            root->kind = WK_VBOX;
            root->id = ui_hash_id(0, "__root__", 0);
            root->padding = UI_PAD_SCREEN;
            ctx->root = root;
        }
        // remember what we just built so the next frame can compare
        ctx->last_descr_hash = new_hash;
        ctx->hashed_tree     = ctx->last_tree;
        ctx->descr_hash_valid = true;
    }

    int top       = ctx->custom_chrome ? UI_TITLEBAR_H : 0;       // titlebar reserved at the top
    int mb_h      = ui_menubar_height(ctx);                       // menu bar reserved below it
    int content_t = top + mb_h;                                   // top edge of the widget tree
    ctx->root->x = 0;
    ctx->root->y = (float)content_t;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - content_t);
    if (ctx->geometry_dirty) {                                    // skip when nothing moved
        ui_layout(ctx);                                           // compute widget geometry
        ctx->geometry_dirty = false;
    }

    // snapshot the freshly laid-out tree, diff against the previous frame to compute damage
    ui_snapshot_capture(ctx, ctx->root);
    ui_damage_begin(ctx);

    // the modal dialog is painted on top of everything
    bool dlg_animating =
        ctx->dialog.active &&
        ((ctx->dialog.closing && ctx->dialog.anim_t > 0.0) ||
         (!ctx->dialog.closing && ctx->dialog.anim_t < 1.0));

    if (ctx->full_repaint || dlg_animating) {
        ui_damage_add(ctx, 0, 0, ctx->win_w, ctx->win_h);
        ctx->full_repaint = false;
    } else {
        ui_damage_diff(ctx);
        if (ctx->custom_chrome)
            ui_damage_add(ctx, 0, 0, ctx->win_w, UI_TITLEBAR_H);
    }
    ui_damage_finalize(ctx);

    if (ctx->damage_count > 0) {
        long total = 0, area = (long)ctx->win_w * (long)ctx->win_h;
        for (int i = 0; i < ctx->damage_count; i++)
            total += (long)ctx->damage[i].w * (long)ctx->damage[i].h;

        ui_render_frame_damage(ctx, ctx->damage, ctx->damage_count);
        if (ctx->dialog.active || total * 2 >= area) {
            // a modal dialog dims the whole window and paints on top of everything
            ui_window_present(ctx->window);                       // whole-window blit
        } else {
            ui_window_present_rects(ctx->window,                  // small damage: per-rect blits
                                    (const int*)ctx->damage,
                                    ctx->damage_count);
        }
    }
    if (ctx->window) ui_window_sync_ack(ctx->window);             // ack the pending resize
    ui_damage_swap(ctx);
    ctx->redraw_needed = false;                                   // repaint satisfied

    // state gc is only needed when the tree was actually rebuilt
    if (!can_reuse) {
        state_gc_mark_root(ctx, ctx->root);                       // mark reachable state
        state_gc_sweep(ctx);                                      // drop the rest

        // clear interaction ids that no longer match a live widget after the rebuild
        if (ctx->focused_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->focused_id) found = true;
            if (!found) ctx->focused_id = 0;
        }
        if (ctx->hovered_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->hovered_id) found = true;
            if (!found) ctx->hovered_id = 0;
        }
        if (ctx->drag_widget_id) {
            bool found = false;
            for (int i = 0; i < ctx->pool_used && !found; i++)
                if (ctx->pool[i].id == ctx->drag_widget_id) found = true;
            if (!found) ctx->drag_widget_id = 0;
        }
    }
}

// repaints the existing widget tree without rebuilding it
void redraw_only(UiContext* ctx) {
    if (!ctx->root) return;                                       // nothing to draw
    if (!ctx->window) {                                           // window is gone: nothing to blit
        ctx->redraw_needed = false;
        ctx->full_repaint  = false;
        return;
    }

    int top       = ctx->custom_chrome ? UI_TITLEBAR_H : 0;       // titlebar reserved at the top
    int mb_h      = ui_menubar_height(ctx);                       // menu bar reserved below it
    int content_t = top + mb_h;                                   // top edge of the widget tree
    ctx->root->x = 0;
    ctx->root->y = (float)content_t;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)(ctx->win_h - content_t);

    if (ctx->geometry_dirty) {                                    // skip when nothing moved
        ui_layout(ctx);                                           // recompute geometry
        ctx->geometry_dirty = false;
    }
    ui_render_frame(ctx);                                         // redraw into the pixel buffer
    ui_window_present(ctx->window);                               // blit to the screen
    if (ctx->window) ui_window_sync_ack(ctx->window);             // ack the pending resize
    ctx->redraw_needed = false;                                   // repaint satisfied
    ctx->full_repaint  = false;                                   // full repaint satisfied
}

// host window paint hook: refreshes buffers and repaints the existing tree
void ui_window_paint_cb(void* user, int w, int h) {
    UiContext* ctx = (UiContext*)user;
    if (!ctx || !ctx->window) return;                             // nothing to paint

    ctx->pixels = ui_window_pixels(ctx->window);                  // refresh pixel buffer
    if (w != ctx->win_w || h != ctx->win_h) {                     // client rect changed under us
        ctx->win_w = w;
        ctx->win_h = h;
        ctx->geometry_dirty = true;
    }

    redraw_only(ctx);                                             // repaint the current tree
}

// pumps the window event loop once, returning true if events were queued
bool pump(UiContext* ctx) {
    if (!ctx->window) return false;                               // no window: nothing to pump

    // advance animated state first, before any early return
    static double s_last_tick = 0.0;                              // previous wall-clock reading
    double now = apex_now_seconds();
    double dt = now - s_last_tick;
    s_last_tick = now;
    if (dt < 0.0) dt = 0.0;                                       // clock went backwards
    if (dt > 0.25) dt = 0.25;                                     // clamp after a long pause
    ui_tick(ctx, dt);

    UiWinEvent we;
    while (ui_window_poll(ctx->window, &we)) {                    // drain pending events
        handle_win_event(ctx, &we);
        if (we.kind == UW_EV_CLOSE) break;                        // stop on close
    }
    ui_notification_pump(ctx);                                    // pump popup events too
    if (ctx->queue.count > 0) return true;                        // events already queued
    if (!ui_window_is_open(ctx->window)) return false;            // window closed

    int timeout;
    if (ctx->redraw_needed)        timeout = 8;                   // ~120 fps: repaint pending
    else if (ui_animating(ctx))    timeout = 8;                   // ~120 fps: scroll or notif ease
    else if (ctx->focused_id)      timeout = 62;                  // ~16 hz: only caret blink
    else if (ctx->dialog.active)   timeout = 62;                  // dialog animation / blink
    else if (ctx->notif_count > 0) timeout = 62;                  // idle notifications: poll close btn
    else                           timeout = 1000;                // fully idle
    if (ui_window_wait(ctx->window, timeout)) {                   // block briefly for input
        while (ui_window_poll(ctx->window, &we)) {                // drain newly arrived events
            handle_win_event(ctx, &we);
            if (we.kind == UW_EV_CLOSE) break;
        }
    }

    return ctx->queue.count > 0;                                  // true if anything was queued
}

// builtin: creates the native window if not already open
Value ui_impl_init(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) return MAKE_BOOL(true);                      // already initialised
    int w = 640, h = 480;                                         // default window size
    int min_w = UI_MIN_W, min_h = UI_MIN_H;                       // default minimum size
    char* title = NULL;
    bool chrome = true;                                           // custom titlebar on by default
    if (argc >= 1 && IS_TABLE(argv[0])) {                         // optional options table
        Table* t = AS_TABLE(argv[0]);
        w = (int)table_get_num(vm, t, "width", 640);              // width override
        h = (int)table_get_num(vm, t, "height", 480);             // height override
        title  = table_get_str_dup(vm, t, "title");               // title override
        chrome = table_get_bool(vm, t, "titlebar", true);         // default: on
        min_w = (int)table_get_num(vm, t, "min_width",  UI_MIN_W);  // minimum width
        min_h = (int)table_get_num(vm, t, "min_height", UI_MIN_H);  // minimum height
    }
    const char* use_title = title ? title : "Apex UI";            // fall back to default title
    ctx->custom_chrome = chrome;                                  // remember for the render pass
    ctx->tb_hot = ctx->tb_active = 0;                             // clear button state
    strncpy(ctx->title, use_title, sizeof(ctx->title) - 1);       // mirror the title
    ctx->title[sizeof(ctx->title) - 1] = 0;
    ctx->window = ui_window_create(w, h, use_title, !chrome);     // create the host window
    free(title);                                                  // release the temp title
    if (!ctx->window) return MAKE_BOOL(false);                    // creation failed
    ctx->win_w = w;                                               // cache window size
    ctx->win_h = h;
    ctx->pixels = ui_window_pixels(ctx->window);                  // cache pixel buffer

    ctx->redraw_needed = true;                                    // force first paint
    ctx->full_repaint  = true;                                    // first frame is undefined
    ui_window_set_min_size(ctx->window, min_w, min_h);            // enforce a floor on resize
    ui_window_set_paint_callback(ctx->window, ui_window_paint_cb, ctx);  // install paint hook

    return MAKE_BOOL(true);                                       // success
}

// builtin: records the current widget tree descriptor for the next repaint
Value ui_impl_render(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->has_tree && (ctx->last_tree & QNAN) == QNAN)
        value_decref(ctx->last_tree);                             // release previous tree reference
    if (argc >= 1 && (argv[0] & QNAN) == QNAN) {                  // new tree is heap-tagged
        value_incref(argv[0]);                                    // take a reference
        ctx->last_tree = argv[0];
        ctx->has_tree = true;
    } else {
        ctx->last_tree = MAKE_NONE();                             // no tree or non-heap value
        ctx->has_tree = false;
    }
    ctx->redraw_needed = true;                                    // schedule a repaint
    return MAKE_NONE();
}

// builtin: blocks until an event is available, returns it as a table
Value ui_impl_next_event(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (!ctx->window) return MAKE_NONE();                         // no window: nothing to wait on

    int safety = 0;                                               // guard against a runaway loop
    while (ctx->queue.count == 0) {                               // wait for an event
        if (ctx->quit_requested || !ui_window_is_open(ctx->window)) {
            UiEvent ev; memset(&ev, 0, sizeof(ev));               // synthesize a close event
            ev.kind = EV_CLOSE;
            ui_event_push(ctx, &ev);
            break;
        }
        if (ctx->redraw_needed) rebuild_and_draw(ctx);            // repaint if requested
        pump(ctx);                                                // drain native events
        if (ctx->queue.count > 0) break;                          // event arrived
        if (++safety > 5000) break;                               // give up after too many empty spins
    }
    if (ctx->queue.count == 0) return MAKE_NONE();                // still nothing: return none

    UiEvent ev = ctx->queue.events[0];                            // peek the front event
    memmove(&ctx->queue.events[0], &ctx->queue.events[1],
            sizeof(UiEvent) * (ctx->queue.count - 1));            // shift the queue down
    ctx->queue.count--;                                           // one fewer queued event

    Value t = ui_event_to_table(vm, &ev);                         // convert to an apex table
    free(ev.str_value);                                           // release the event's owned string
    return t;                                                     // return the event table
}

// drive the message loop with batched repaints
Value ui_impl_run(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                   // ensure the context exists
    if (!ctx->window) return MAKE_NONE();             // no window: nothing to run
    if (argc < 1 || !IS_FUNCTION(argv[0])) return MAKE_NONE();  // callback required

    Value callback = argv[0];
    value_incref(callback);                           // keep alive across the loop

    if (ctx->redraw_needed) rebuild_and_draw(ctx);    // honour a pre-run ui.render

    while (!ctx->quit_requested && ui_window_is_open(ctx->window)) {
        pump(ctx);                                    // block, drain native events

        bool rebuilt = false;                         // did any callback return a tree?

        while (ctx->queue.count > 0) {                // process the whole batch
            UiEvent ev = ctx->queue.events[0];        // peek front
            memmove(&ctx->queue.events[0], &ctx->queue.events[1],
                    sizeof(UiEvent) * (ctx->queue.count - 1));  // shift down
            ctx->queue.count--;                       // one fewer queued event

            Value ev_tbl = ui_event_to_table(vm, &ev);  // convert to Apex table
            free(ev.str_value);                       // release the event's owned string

            Value result = apex_call_function(vm, callback, 1, &ev_tbl);
            value_decref(ev_tbl);                     // release our local reference

            if (IS_TABLE(result)) {                   // callback staged a new tree
                if (ctx->has_tree && (ctx->last_tree & QNAN) == QNAN)
                    value_decref(ctx->last_tree);     // release previous tree
                ctx->last_tree = result;              // take ownership of the new one
                ctx->has_tree  = true;
                ctx->redraw_needed = true;
                rebuilt = true;
            } else {
                value_decref(result);                 // none or non-table: drop it
            }

            if (ctx->quit_requested) break;           // callback shut down: stop the batch
        }

        if (ctx->quit_requested) break;               // bail before any render pass

        if (rebuilt || ctx->redraw_needed)            // one rebuild per batch
            rebuild_and_draw(ctx);
    }

    value_decref(callback);                           // release the callback
    return MAKE_NONE();
}

// builtin: destroys the native window and marks the context as shut down
Value ui_impl_shutdown(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) {                                            // tear down the window if present
        ui_window_destroy(ctx->window);                           // releases the framebuffer too
        ctx->window = NULL;                                       // drop the handle
        ctx->pixels = NULL;                                       // buffer is freed with the window
        ctx->redraw_needed = false;                               // no render pass should run now
        ctx->full_repaint  = false;
        ctx->geometry_dirty = false;
    }
    ctx->quit_requested = true;                                   // mark the context as quit
    return MAKE_NONE();
}

// builtin: sets the native window title
Value ui_impl_set_title(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (!ctx->window) return MAKE_NONE();                         // no window: no-op
    if (argc >= 1 && IS_STRING(argv[0])) {                        // only string titles are applied
        const char* s = AS_STRING(argv[0])->chars;
        ui_window_set_title(ctx->window, s);
        strncpy(ctx->title, s, sizeof(ctx->title) - 1);           // mirror for the custom titlebar
        ctx->title[sizeof(ctx->title) - 1] = 0;
        ctx->redraw_needed = true;                                // schedule a repaint
    }
    return MAKE_NONE();
}

// builtin: returns the current window size as a {w, h} table
Value ui_impl_window_size(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    Table* t = table_create(4);                                   // small result table
    Value k, v;
    k = ui_intern(vm, "w"); v = MAKE_NUMBER(ctx->win_w);          // width field
    table_set(t, k, v); value_decref(k);
    k = ui_intern(vm, "h"); v = MAKE_NUMBER(ctx->win_h);          // height field
    table_set(t, k, v); value_decref(k);
    return MAKE_TABLE(t);                                         // hand the table to the caller
}

// parses the optional options table shared by the two pickers
static void parse_picker_opts(VM* vm, int argc, Value* argv,
                              char** out_title, char** out_default,
                              UiFileFilter** out_filters, int* out_filter_count) {
    *out_title = NULL;
    *out_default = NULL;
    *out_filters = NULL;
    *out_filter_count = 0;
    if (argc < 1 || !IS_TABLE(argv[0])) return;

    Table* t = AS_TABLE(argv[0]);
    *out_title   = table_get_str_dup(vm, t, "title");
    *out_default = table_get_str_dup(vm, t, "default");

    Value fv;
    Value k_filters = ui_intern(vm, "filters");
    if (!table_get(t, k_filters, &fv)) { value_decref(k_filters); return; }
    value_decref(k_filters);

    if (IS_TABLE(fv)) {
        Table* ft = AS_TABLE(fv);
        int n = ft->array_part ? ft->array_count : 0;
        UiFileFilter* arr = (UiFileFilter*)calloc((size_t)n + 1, sizeof(UiFileFilter));
        if (arr) {
            for (int i = 0; i < n; i++) {
                Value item = ft->array_part[i];
                if (!IS_TABLE(item)) continue;
                Table* it = AS_TABLE(item);
                char* name = table_get_str_dup(vm, it, "name");
                char* pat  = table_get_str_dup(vm, it, "pattern");
                if (name && pat) {
                    arr[*out_filter_count].name    = name;
                    arr[*out_filter_count].pattern = pat;
                    (*out_filter_count)++;
                } else {
                    free(name); free(pat);
                }
            }
            *out_filters = arr;
        }
    }
    value_decref(fv);
}

static void free_picker_opts(char* title, char* def,
                             UiFileFilter* filters, int count) {
    for (int i = 0; i < count; i++) {
        free((void*)filters[i].name);
        free((void*)filters[i].pattern);
    }
    free(filters);
    free(title);
    free(def);
}

// builtin: ui.open_file(opts?) — native "open file" dialog
Value ui_impl_open_file(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);
    if (!ctx->window) return MAKE_NONE();                 // no native window: nothing to anchor to

    char* title = NULL;
    char* def   = NULL;
    UiFileFilter* filters = NULL;
    int filter_count = 0;
    parse_picker_opts(vm, argc, argv, &title, &def, &filters, &filter_count);

    char* picked = ui_window_open_file(ctx->window, title, def, filters);
    free_picker_opts(title, def, filters, filter_count);

    if (!picked) return MAKE_NONE();                      // cancelled or unavailable
    Value v = MAKE_STRING(string_create(picked, (int)strlen(picked)));
    free(picked);
    return v;
}

// builtin: ui.open_folder(opts?) — native "open folder" dialog
Value ui_impl_open_folder(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);
    if (!ctx->window) return MAKE_NONE();

    char* title = NULL;
    char* def   = NULL;
    UiFileFilter* filters = NULL;
    int filter_count = 0;
    parse_picker_opts(vm, argc, argv, &title, &def, &filters, &filter_count);

    // filters are meaningless for a folder picker
    char* picked = ui_window_open_folder(ctx->window, title, def);
    free_picker_opts(title, def, filters, filter_count);

    if (!picked) return MAKE_NONE();
    Value v = MAKE_STRING(string_create(picked, (int)strlen(picked)));
    free(picked);
    return v;
}
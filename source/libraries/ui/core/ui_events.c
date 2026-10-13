// source/libraries/ui/core/ui_events.c
// Event queue, table conversion, apex callback bridge for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_table.h"
#include "ui_hash.h"
#include <stdlib.h>
#include <string.h>

// appends an event to the pending event queue
void ui_event_push(UiContext* ctx, const UiEvent* ev) {
    if (ev->kind == EV_CHANGE && ev->widget_id != 0) {
        for (int i = 0; i < ctx->queue.count; i++) {
            UiEvent* q = &ctx->queue.events[i];
            if (q->kind == EV_CHANGE && q->widget_id == ev->widget_id) {
                free(q->str_value);            // release the superseded payload
                *q = *ev;                      // take ownership of the new one
                return;
            }
        }
    }

    if (ctx->queue.count >= ctx->queue.cap) {             // event queue is full
        ctx->queue.cap = ctx->queue.cap ? ctx->queue.cap * 2 : 32;  // start at 32, then double
        ctx->queue.events = (UiEvent*)realloc(ctx->queue.events,
                                sizeof(UiEvent) * ctx->queue.cap);
    }
    ctx->queue.events[ctx->queue.count++] = *ev;          // copy event into the queue
}

// converts a native ui event into an apex table value for the script layer
Value ui_event_to_table(VM* vm, const UiEvent* ev) {
    Table* t = table_create(14);                          // event table with room for ~10 fields
    Value k, v;
    const char* ty = "none";                              // default type string
    switch (ev->kind) {
        case EV_CLOSE:  ty = "close";  break;
        case EV_CLICK:  ty = "click";  break;
        case EV_CHANGE: ty = "change"; break;
        case EV_KEY:    ty = "key";    break;
        case EV_SCROLL: ty = "scroll"; break;
        case EV_HOVER:  ty = "hover";  break;
        case EV_RESIZE: ty = "resize"; break;
        case EV_DIALOG: ty = "dialog"; break;
        default: break;
    }

    k = ui_intern(vm, "type");                               // "type" = event kind string
    v = ui_intern(vm, ty);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = ui_intern(vm, "key");                                // "key" = user key string
    if (ev->key_name[0]) {
        v = MAKE_STRING(string_create(ev->key_name, (int)strlen(ev->key_name)));
        table_set(t, k, v); value_decref(k); value_decref(v);
    } else {
        v = ui_intern(vm, "");                               // empty string when no user key
        table_set(t, k, v); value_decref(k); value_decref(v);
    }

    k = ui_intern(vm, "widget");                             // "widget" = widget type name
    v = ui_intern(vm, ev->widget_type);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = ui_intern(vm, "value");                              // "value" = event payload, type varies
    if (ev->str_value) {
        v = MAKE_STRING(string_create(ev->str_value, (int)strlen(ev->str_value)));
    } else if (strcmp(ev->widget_type, "slider") == 0 ||
               strcmp(ev->widget_type, "progress") == 0 ||
               ev->kind == EV_SCROLL) {
        v = MAKE_NUMBER(ev->num_value);                   // numeric payload
    } else {
        v = MAKE_BOOL(ev->bool_value);                    // default to boolean payload
    }
    table_set(t, k, v); value_decref(k);
    if ((v & QNAN) == QNAN) value_decref(v);              // release heap-tagged payload

    if (ev->kind == EV_KEY) {                             // extra fields for key events
        k = ui_intern(vm, "key_code"); v = MAKE_NUMBER((double)ev->key_code);
        table_set(t, k, v); value_decref(k);
        k = ui_intern(vm, "mods"); v = MAKE_NUMBER((double)ev->mods);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_SCROLL) {                          // extra field for scroll events
        k = ui_intern(vm, "scroll_dy"); v = MAKE_NUMBER(ev->scroll_dy);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_CLICK || ev->kind == EV_HOVER) {   // extra fields for pointer events
        k = ui_intern(vm, "x"); v = MAKE_NUMBER((double)ev->mouse_x);
        table_set(t, k, v); value_decref(k);
        k = ui_intern(vm, "y"); v = MAKE_NUMBER((double)ev->mouse_y);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_DIALOG) {                          // "action" = ok / cancel / close
        k = ui_intern(vm, "action");
        const char* act = (ev->num_value == 1.0) ? "ok" : "cancel";
        v = ui_intern(vm, act);
        table_set(t, k, v); value_decref(k); value_decref(v);
    }
    return MAKE_TABLE(t);                                 // hand the event table to the caller
}

// emits a click event for a widget, plus a change event for checkboxes
void fire_click(UiContext* ctx, Widget* w) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));                       // zero the event
    ev.kind = EV_CLICK;                                           // click kind
    ev.widget_id = w->id;                                         // source widget id
    strncpy(ev.widget_type, widget_kind_name(w->kind), 15);       // widget type name
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);         // user key if present
    ev.mouse_x = ctx->input.x;                                    // record pointer position
    ev.mouse_y = ctx->input.y;
    ui_event_push(ctx, &ev);                                      // enqueue the click

    if (w->kind == WK_CHECKBOX) {                                 // checkboxes also fire change
        UiEvent cev; memset(&cev, 0, sizeof(cev));
        cev.kind = EV_CHANGE;
        cev.widget_id = w->id;
        strncpy(cev.widget_type, "checkbox", 15);
        if (w->key_str) strncpy(cev.key_name, w->key_str, 63);
        cev.bool_value = !w->checked;                             // new value is the inverse
        ui_event_push(ctx, &cev);
    }
    ctx->redraw_needed = true;                                    // force a repaint
}

// calls an apex function value synchronously by running it as a one-shot coroutine
Value apex_call_function(VM* vm, Value func, int argc, Value* argv) {
    if (!IS_FUNCTION(func)) return MAKE_NONE();       // not callable
    int func_idx = AS_FUNCTION(func);                 // unwrap packed function index

    FutureObject* fut = (FutureObject*)calloc(1, sizeof(FutureObject));
    if (!fut) return MAKE_NONE();                     // oom: no result
    fut->header.ref_count = 1;                        // fresh object
    fut->header.type      = VAL_FUTURE;               // mark as future
    fut->result           = MAKE_NONE();
    fut->state            = 0;                        // pending
    fut->func_idx         = func_idx;
    fut->arg_count        = argc;
    fut->args             = argc > 0 ? (Value*)malloc(sizeof(Value) * argc) : NULL;
    fut->saved_dest_reg   = -1;
    fut->awaiting         = MAKE_NONE();
    fut->saved_iter_depth = -1;
    fut->saved_table_iter_depth = -1;
    fut->saved_call_depth = 0;
    for (int i = 0; i < argc; i++) {                  // capture args, keeping refs alive
        if ((argv[i] & QNAN) == QNAN) value_incref(argv[i]);
        fut->args[i] = argv[i];
    }

    future_start(vm, fut);                            // enqueue the coroutine
    Value target = MAKE_FUTURE(fut);
    Value result;
    vm_drive_until(vm, target, &result);              // run it to completion
    value_decref(target);                             // drop our local reference
    return result;                                    // caller owns one ref
}
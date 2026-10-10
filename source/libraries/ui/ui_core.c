// source/libraries/ui/ui_core.c
// Implementation of UI library core for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_internal.h"
#include "ui_window.h"
#include "ui_text.h"
#include "ui_module.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static void rebuild_and_draw(UiContext* ctx);       // full widget tree rebuild then repaint
static void redraw_only(UiContext* ctx);            // repaint without rebuilding the tree
static void ui_window_paint_cb(void* user, int w, int h);  // host window paint hook

// fnv-1a hash of parent id + key (or index) to produce a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx) {
    uint64_t h = 0xCBF29CE484222325ULL ^ parent;     // fold parent id into the seed
    if (key) {
        while (*key) { h ^= (unsigned char)*key++; h *= 0x100000001B3ULL; }  // hash each key byte
    } else {
        h ^= (uint64_t)(idx + 1);                    // no key: mix in 1-based positional index
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1;                                // never return 0, id 0 is reserved for "none"
}

// grows the widget pool, doubling capacity each time
static void pool_grow(UiContext* ctx) {
    ctx->pool_cap = ctx->pool_cap ? ctx->pool_cap * 2 : 256;  // start at 256, then double
    ctx->pool = (Widget*)realloc(ctx->pool, sizeof(Widget) * ctx->pool_cap);
}

// allocates and zero-initialises the next widget slot from the pool
Widget* ui_widget_alloc(UiContext* ctx) {
    if (ctx->pool_used >= ctx->pool_cap) pool_grow(ctx);  // grow pool if exhausted
    Widget* w = &ctx->pool[ctx->pool_used++];             // take next slot
    memset(w, 0, sizeof(*w));                             // clear every field
    w->req_w = w->req_h = -1;                             // no explicit size requested
    w->align = AL_STRETCH;                                // default align: fill parent
    w->justify = JU_START;                                // default justify: pack to start
    w->visible = true;                                    // visible by default
    w->weight = 0;                                        // no flex weight
    w->gap = UI_GAP;                                      // default child gap
    w->padding = 0;                                       // no padding
    w->child_cap = 0; w->children = NULL;                 // no child storage yet
    w->key_str = NULL;                                    // no user key
    return w;                                             // return freshly initialised slot
}

// releases every heap allocation owned by pooled widgets and rewinds the pool
void ui_widgets_reset(UiContext* ctx) {
    for (int i = 0; i < ctx->pool_used; i++) {            // walk each live widget
        free(ctx->pool[i].children);                      // free child pointer array
        free(ctx->pool[i].text.ptr);                      // free label/content string
        free(ctx->pool[i].input_value.ptr);               // free input buffer
        free(ctx->pool[i].group.ptr);                     // free radio group name
        free(ctx->pool[i].key_str);                       // free user key string
    }
    ctx->pool_used = 0;                                   // reset pool cursor
    ctx->root = NULL;                                     // drop tree root
}

// appends a child to a widget, growing the child array as needed
static void widget_add_child(Widget* p, Widget* c) {
    if (p->child_count >= p->child_cap) {                 // child array is full
        p->child_cap = p->child_cap ? p->child_cap * 2 : 4;  // start at 4, then double
        p->children = (Widget**)realloc(p->children, sizeof(Widget*) * p->child_cap);
    }
    p->children[p->child_count++] = c;                    // append at end
    c->parent = p;                                        // back-link to parent
}

// looks up persistent per-widget state by id, creating an empty entry on miss
WidgetState* ui_state_get(UiContext* ctx, uint64_t id) {
    for (int i = 0; i < ctx->state.count; i++)
        if (ctx->state.entries[i].id == id) return &ctx->state.entries[i];  // existing entry
    if (ctx->state.count >= ctx->state.cap) {             // state table is full
        ctx->state.cap = ctx->state.cap ? ctx->state.cap * 2 : 64;  // start at 64, then double
        ctx->state.entries = (WidgetState*)realloc(ctx->state.entries,
                                sizeof(WidgetState) * ctx->state.cap);
    }
    WidgetState* s = &ctx->state.entries[ctx->state.count++];  // take next slot
    memset(s, 0, sizeof(*s));                             // clear the entry
    s->id = id;                                           // bind to the widget id
    s->cursor = -1;                                       // no cursor override yet
    return s;                                             // return the fresh entry
}

// appends an event to the pending event queue
void ui_event_push(UiContext* ctx, const UiEvent* ev) {
    if (ctx->queue.count >= ctx->queue.cap) {             // event queue is full
        ctx->queue.cap = ctx->queue.cap ? ctx->queue.cap * 2 : 32;  // start at 32, then double
        ctx->queue.events = (UiEvent*)realloc(ctx->queue.events,
                                sizeof(UiEvent) * ctx->queue.cap);
    }
    ctx->queue.events[ctx->queue.count++] = *ev;          // copy event into the queue
}

// maps a widget kind enum to its lowercase string name
static const char* widget_kind_name(WidgetKind k) {
    switch (k) {
        case WK_TEXT: return "text";
        case WK_HEADING: return "heading";
        case WK_BUTTON: return "button";
        case WK_LINK: return "link";
        case WK_CHECKBOX: return "checkbox";
        case WK_RADIO: return "radio";
        case WK_SLIDER: return "slider";
        case WK_PROGRESS: return "progress";
        case WK_INPUT: return "input";
        case WK_BADGE: return "badge";
        case WK_IMAGE: return "image";
        case WK_SEPARATOR: return "separator";
        case WK_SPACER: return "spacer";
        case WK_VBOX: return "vbox";
        case WK_HBOX: return "hbox";
        case WK_PANEL: return "panel";
        case WK_FRAME: return "frame";
        case WK_SCROLL: return "scroll";
        case WK_HSCROLL: return "hscroll";
        case WK_GRID: return "grid";
    }
    return "widget";                                      // fallback for unknown kinds
}

// interns a c string into the vm's intern table, returning a tagged string value
static Value intern(VM* vm, const char* s) {
    return MAKE_STRING(string_intern(&vm->intern_table, s, (int)strlen(s)));
}

// converts a native ui event into an apex table value for the script layer
//
// "key" is now always a string: the widget's user key when one was declared,
// or the empty string otherwise. this replaces the previous "key_name"
// convention and lets scripts match with `ev["key"] == "..."` unconditionally.
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
        default: break;
    }

    k = intern(vm, "type");                               // "type" = event kind string
    v = intern(vm, ty);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = intern(vm, "key");                                // "key" = user key string, always present
    if (ev->key_name[0]) {
        v = MAKE_STRING(string_create(ev->key_name, (int)strlen(ev->key_name)));
        table_set(t, k, v); value_decref(k); value_decref(v);
    } else {
        v = intern(vm, "");                               // empty string when no user key
        table_set(t, k, v); value_decref(k); value_decref(v);
    }

    k = intern(vm, "widget");                             // "widget" = widget type name
    v = intern(vm, ev->widget_type);
    table_set(t, k, v); value_decref(k); value_decref(v);

    k = intern(vm, "value");                              // "value" = event payload, type varies
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
        k = intern(vm, "key_code"); v = MAKE_NUMBER((double)ev->key_code);
        table_set(t, k, v); value_decref(k);
        k = intern(vm, "mods"); v = MAKE_NUMBER((double)ev->mods);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_SCROLL) {                          // extra field for scroll events
        k = intern(vm, "scroll_dy"); v = MAKE_NUMBER(ev->scroll_dy);
        table_set(t, k, v); value_decref(k);
    }
    if (ev->kind == EV_CLICK || ev->kind == EV_HOVER) {   // extra fields for pointer events
        k = intern(vm, "x"); v = MAKE_NUMBER((double)ev->mouse_x);
        table_set(t, k, v); value_decref(k);
        k = intern(vm, "y"); v = MAKE_NUMBER((double)ev->mouse_y);
        table_set(t, k, v); value_decref(k);
    }
    return MAKE_TABLE(t);                                 // hand the event table to the caller
}

// parses a cross-axis alignment name into its enum value
static Align parse_align(const char* s) {
    if (!s) return AL_STRETCH;                            // default: stretch
    if (!strcmp(s, "start"))   return AL_START;
    if (!strcmp(s, "center"))  return AL_CENTER;
    if (!strcmp(s, "end"))     return AL_END;
    if (!strcmp(s, "stretch")) return AL_STRETCH;
    return AL_STRETCH;                                    // fallback: stretch
}

// parses a main-axis justification name into its enum value
static Justify parse_justify(const char* s) {
    if (!s) return JU_START;                              // default: pack to start
    if (!strcmp(s, "center")) return JU_CENTER;
    if (!strcmp(s, "end"))    return JU_END;
    if (!strcmp(s, "space-between")) return JU_SPACE_BETWEEN;
    if (!strcmp(s, "space-around"))  return JU_SPACE_AROUND;
    if (!strcmp(s, "space-evenly"))  return JU_SPACE_EVENLY;
    return JU_START;                                      // fallback: start
}

// parses a widget kind name into its enum value
static WidgetKind kind_from_str(const char* s) {
    if (!strcmp(s, "text"))      return WK_TEXT;
    if (!strcmp(s, "heading"))   return WK_HEADING;
    if (!strcmp(s, "button"))    return WK_BUTTON;
    if (!strcmp(s, "link"))      return WK_LINK;
    if (!strcmp(s, "checkbox"))  return WK_CHECKBOX;
    if (!strcmp(s, "radio"))     return WK_RADIO;
    if (!strcmp(s, "slider"))    return WK_SLIDER;
    if (!strcmp(s, "progress"))  return WK_PROGRESS;
    if (!strcmp(s, "input"))     return WK_INPUT;
    if (!strcmp(s, "badge"))     return WK_BADGE;
    if (!strcmp(s, "image"))     return WK_IMAGE;
    if (!strcmp(s, "separator")) return WK_SEPARATOR;
    if (!strcmp(s, "spacer"))    return WK_SPACER;
    if (!strcmp(s, "vbox"))      return WK_VBOX;
    if (!strcmp(s, "hbox"))      return WK_HBOX;
    if (!strcmp(s, "panel"))     return WK_PANEL;
    if (!strcmp(s, "frame"))     return WK_FRAME;
    if (!strcmp(s, "scroll"))    return WK_SCROLL;
    if (!strcmp(s, "hscroll"))   return WK_HSCROLL;
    if (!strcmp(s, "grid"))      return WK_GRID;
    return WK_TEXT;                                       // fallback: plain text
}

// reads a string field from a table, returns a heap copy or null on miss
static char* table_get_str_dup(VM* vm, Table* t, const char* key) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return NULL; }  // key absent
    value_decref(k);
    if (!IS_STRING(v)) { value_decref(v); return NULL; }         // wrong type
    StringObject* s = AS_STRING(v);
    char* out = (char*)malloc(s->length + 1);                    // allocate copy buffer
    memcpy(out, s->chars, s->length);                            // copy bytes
    out[s->length] = 0;                                          // null terminate
    value_decref(v);                                             // release the table's value
    return out;                                                  // caller owns the copy
}

// reads a number field from a table, returns default on miss or wrong type
static double table_get_num(VM* vm, Table* t, const char* key, double def) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    double r = IS_NUMBER(v) ? AS_NUMBER(v) : def;                // use value only if numeric
    value_decref(v);
    return r;
}

// reads a boolean field from a table, returns default on miss or wrong type
static bool table_get_bool(VM* vm, Table* t, const char* key, bool def) {
    Value k = intern(vm, key), v;
    if (!table_get(t, k, &v)) { value_decref(k); return def; }   // key absent
    value_decref(k);
    bool r = IS_BOOL(v) ? AS_BOOL(v) : def;                      // use value only if boolean
    value_decref(v);
    return r;
}

// duplicates n bytes of a c string into a heap-owned Str
static Str str_dup(const char* s, int n) {
    Str r;
    r.ptr = (char*)malloc(n + 1);                        // allocate room for payload + null
    memcpy(r.ptr, s, n);                                 // copy bytes
    r.ptr[n] = 0;                                        // null terminate
    r.len = n;                                           // record length
    return r;                                            // return by value
}

// recursively builds a Widget subtree from an apex table descriptor
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx) {
    if (!IS_TABLE(descr)) return NULL;                   // descriptors must be tables
    Table* t = AS_TABLE(descr);
    VM* vm = ctx->vm;

    char* ty = table_get_str_dup(vm, t, "__t");          // "__t" = widget kind name
    if (!ty) { free(ty); return NULL; }                  // missing kind: skip this node

    Widget* w = ui_widget_alloc(ctx);                    // allocate the widget slot
    w->kind = kind_from_str(ty);                         // resolve the kind enum
    free(ty);                                            // release the temp kind string

    char* key = table_get_str_dup(vm, t, "key");         // optional user key for id derivation
    uint64_t parent_id = parent ? parent->id : 0;        // root widgets hash from 0
    w->id = ui_hash_id(parent_id, key, idx);             // derive stable id
    if (key) w->key_str = strdup(key);                   // keep the key for event payloads
    free(key);

    char* al = table_get_str_dup(vm, t, "align");        // optional align override
    if (al) { w->align = parse_align(al); free(al); }
    char* ju = table_get_str_dup(vm, t, "justify");      // optional justify override
    if (ju) { w->justify = parse_justify(ju); free(ju); }
    w->gap     = (float)table_get_num(vm, t, "gap", 8);          // child spacing
    w->padding = (float)table_get_num(vm, t, "padding", 0);      // inner padding
    w->weight  = (float)table_get_num(vm, t, "weight", 0);       // flex weight
    w->min_w   = (float)table_get_num(vm, t, "min_width", 0);    // minimum width
    w->max_w   = (float)table_get_num(vm, t, "max_width", 0);    // maximum width
    w->disabled = table_get_bool(vm, t, "disabled", false);      // disabled flag
    w->visible  = table_get_bool(vm, t, "visible", true);        // visibility flag

    Value wk, wv;
    wk = intern(vm, "width");                            // explicit width override
    if (table_get(t, wk, &wv)) {
        if (IS_NUMBER(wv)) w->req_w = (float)AS_NUMBER(wv);
        value_decref(wv);
    }
    value_decref(wk);
    wk = intern(vm, "height");                           // explicit height override
    if (table_get(t, wk, &wv)) {
        if (IS_NUMBER(wv)) w->req_h = (float)AS_NUMBER(wv);
        value_decref(wv);
    }
    value_decref(wk);

    char* txt = table_get_str_dup(vm, t, "content");     // "content" preferred for text
    if (!txt) txt = table_get_str_dup(vm, t, "label");   // "label" as fallback
    if (txt) { w->text = str_dup(txt, (int)strlen(txt)); free(txt); }

    w->checked = table_get_bool(vm, t, "checked", false);         // checkbox/radio state
    char* grp = table_get_str_dup(vm, t, "group");                // radio group name
    if (grp) { w->group = str_dup(grp, (int)strlen(grp)); free(grp); }
    w->num_value = table_get_num(vm, t, "value", 0);              // slider/progress value
    w->num_min   = table_get_num(vm, t, "min", 0);                // slider/progress min
    w->num_max   = table_get_num(vm, t, "max", 100);              // slider/progress max

    char* ival = table_get_str_dup(vm, t, "value_text");          // explicit input text
    if (!ival && w->kind == WK_INPUT) ival = table_get_str_dup(vm, t, "content");  // fall back to content
    if (ival) {
        w->input_value = str_dup(ival, (int)strlen(ival));        // copy into owned buffer
        free(ival);
    } else if (w->kind == WK_INPUT) {
        w->input_value = str_dup("", 0);                          // input widgets always own a buffer
    }

    WidgetState* st = ui_state_get(ctx, w->id);                   // pull persistent state
    w->hovered = st->hovered;                                     // restore hover flag
    w->pressed = st->pressed;                                     // restore press flag
    w->focused = (ctx->focused_id == w->id);                      // reflect current focus
    w->cursor  = st->cursor < 0 ? (int)w->input_value.len : st->cursor;  // default cursor at end
    w->scroll_y = st->scroll_y;                                   // restore scroll offset

    Value ck = intern(vm, "children"), cv;
    if (table_get(t, ck, &cv)) {                                  // child descriptor present?
        if (IS_TABLE(cv)) {
            Table* ct = AS_TABLE(cv);
            Value probe;
            if (table_get(ct, MAKE_NUMBER(1.0), &probe)) {        // array-style children list
                value_decref(probe);
                for (int i = 1; ; i++) {                          // walk 1-based array part
                    Value vv;
                    if (!table_get(ct, MAKE_NUMBER((double)i), &vv)) break;
                    Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                    if (ch) widget_add_child(w, ch);
                    value_decref(vv);
                }
            }
        } else if (!IS_NONE(cv)) {                                // single child descriptor
            Widget* ch = ui_build_tree(ctx, cv, w, 0);
            if (ch) widget_add_child(w, ch);
        }
        value_decref(cv);
    }
    value_decref(ck);

    return w;                                                     // return the built subtree
}

// lazily creates the ui context on first use, attached to the vm
UiContext* ui_ensure(VM* vm) {
    if (vm->ui) return vm->ui;                                    // already created
    UiContext* ctx = (UiContext*)calloc(1, sizeof(UiContext));    // fresh zeroed context
    ctx->vm = vm;                                                 // back-pointer to the vm
    ctx->focused_id = 0;                                          // nothing focused
    ctx->last_tree = MAKE_NONE();                                 // no tree rendered yet

    ctx->font = ui_font_load_system();                            // try to load a default font
    if (!ctx->font) {
        fprintf(stderr, "[ui] no system font available; text will not render\n");
    }
    vm->ui = ctx;                                                 // publish on the vm
    return ctx;
}

// tears down the ui context and clears the vm's back-pointer
void ui_context_destroy(UiContext* ctx) {
    if (!ctx) return;                                             // null guard
    VM* saved_vm = ctx->vm;                                       // remember vm for back-pointer clear
    ui_widgets_reset(ctx);                                        // free all per-widget allocations
    free(ctx->pool);                                              // free the widget pool array
    free(ctx->state.entries);                                     // free the persistent state table
    for (int i = 0; i < ctx->queue.count; i++) free(ctx->queue.events[i].str_value);  // release queued event strings
    free(ctx->queue.events);                                      // free the event queue array
    if (ctx->font)   ui_font_free(ctx->font);                     // release the font
    if (ctx->window) ui_window_destroy(ctx->window);              // release the host window
    ctx->last_tree = MAKE_NONE();                                 // clear the cached tree
    ctx->has_tree = false;
    free(ctx);                                                    // free the context itself
    if (saved_vm) saved_vm->ui = NULL;                            // drop the vm's back-pointer
}

// finds the deepest visible widget containing (x, y), accounting for scroll offset
static Widget* hit_test(Widget* w, int x, int y) {
    if (!w || !w->visible) return NULL;                           // invisible widgets are never hit
    if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h) return NULL;  // outside bounds

    if (w->kind == WK_SCROLL) {                                   // scroll containers clip and offset
        int cx = x, cy = y + (int)w->scroll_y;                    // translate into content space
        for (int i = w->child_count - 1; i >= 0; i--) {           // topmost child first
            Widget* r = hit_test(w->children[i], cx, cy);
            if (r) return r;
        }
        return w;                                                 // fall back to the container itself
    }
    for (int i = w->child_count - 1; i >= 0; i--) {               // topmost child first
        Widget* r = hit_test(w->children[i], x, y);
        if (r) return r;
    }
    return w;                                                     // no child hit: return this widget
}

// returns true if the widget kind participates in keyboard focus
static bool widget_focusable(WidgetKind k) {
    return k == WK_BUTTON || k == WK_CHECKBOX || k == WK_RADIO ||
           k == WK_SLIDER || k == WK_INPUT || k == WK_LINK;
}

// collects focusable widgets in draw order into a caller-supplied array
static void collect_focusable(Widget* w, Widget** arr, int* n, int cap) {
    if (!w || !w->visible || w->disabled) return;                 // skip invisible/disabled
    if (widget_focusable(w->kind) && *n < cap) arr[(*n)++] = w;   // append if focusable and room
    for (int i = 0; i < w->child_count; i++)
        collect_focusable(w->children[i], arr, n, cap);           // recurse into children
}

// emits a click event for a widget, plus a change event for checkboxes
static void fire_click(UiContext* ctx, Widget* w) {
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

// emits a change event for an input widget, copying the new string
static void push_input_change(UiContext* ctx, Widget* w, const char* str_val) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));                       // zero the event
    ev.kind = EV_CHANGE;                                          // change kind
    ev.widget_id = w->id;                                         // source widget id
    strncpy(ev.widget_type, "input", 15);                         // widget type name
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);         // user key if present
    ev.str_value = (char*)malloc(strlen(str_val) + 1);            // own a copy of the new text
    memcpy(ev.str_value, str_val, strlen(str_val) + 1);
    ui_event_push(ctx, &ev);                                      // enqueue the change
}

// translates a raw window event into ui-layer events and state changes
static void handle_win_event(UiContext* ctx, const UiWinEvent* e) {
    switch (e->kind) {
        case UW_EV_CLOSE: {                                       // window close requested
            UiEvent ev; memset(&ev, 0, sizeof(ev));
            ev.kind = EV_CLOSE;
            ui_event_push(ctx, &ev);
            ctx->quit_requested = true;                           // mark shutdown
            break;
        }
        case UW_EV_EXPOSE: {                                      // window exposed, needs paint
            ctx->redraw_needed = true;
            break;
        }
        case UW_EV_RESIZE: {                                      // window resized
            ctx->pixels = ui_window_pixels(ctx->window);          // refresh pixel buffer
            ctx->win_w = e->w;                                    // new width from the event
            ctx->win_h = e->h;                                    // new height from the event
            UiEvent ev; memset(&ev, 0, sizeof(ev));
            ev.kind = EV_RESIZE;
            ev.num_value = e->w;                                  // pack new width in num_value
            ev.scroll_dy = e->h;                                  // pack new height in scroll_dy
            ui_event_push(ctx, &ev);
            ctx->redraw_needed = true;
            break;
        }
        case UW_EV_MOUSEMOVE: {                                   // pointer moved
            ctx->input.x = e->x;                                  // track current pointer
            ctx->input.y = e->y;

            if (ctx->input.lmb && ctx->drag_widget_id) {          // dragging an active slider
                Widget* dw = NULL;
                for (int i = 0; i < ctx->pool_used; i++)
                    if (ctx->pool[i].id == ctx->drag_widget_id) {
                        dw = &ctx->pool[i];
                        break;
                    }
                if (dw && dw->kind == WK_SLIDER) {
                    double range = dw->num_max - dw->num_min;     // value range
                    int thumb_w = 16;                             // thumb width in pixels
                    int track_w = (int)dw->w - thumb_w;           // usable track width
                    if (track_w < 1) track_w = 1;
                    int local = e->x - (int)dw->x - thumb_w / 2;  // pointer position on the track
                    if (local < 0) local = 0;                     // clamp to track start
                    if (local > track_w) local = track_w;         // clamp to track end
                    double t = (double)local / (double)track_w;   // normalised position
                    double v = dw->num_min + t * range;           // map to value range

                    dw->num_value = v;                            // update the live widget

                    UiEvent cev; memset(&cev, 0, sizeof(cev));    // emit a change event
                    cev.kind = EV_CHANGE;
                    cev.widget_id = dw->id;
                    strncpy(cev.widget_type, "slider", 15);
                    if (dw->key_str) strncpy(cev.key_name, dw->key_str, 63);
                    cev.num_value = v;
                    ui_event_push(ctx, &cev);

                    ctx->redraw_needed = true;
                }
            }

            if (ctx->root) {                                      // update hover state
                Widget* h = hit_test(ctx->root, e->x, e->y);
                uint64_t new_hover = h ? h->id : 0;
                for (int i = 0; i < ctx->pool_used; i++) {        // update live widget flags
                    if (ctx->pool[i].id == new_hover && !ctx->pool[i].hovered) {
                        ctx->pool[i].hovered = true;
                        ctx->redraw_needed = true;
                    } else if (ctx->pool[i].id != new_hover && ctx->pool[i].hovered) {
                        ctx->pool[i].hovered = false;
                        ctx->redraw_needed = true;
                    }
                }
                for (int i = 0; i < ctx->state.count; i++)        // clear persistent hover flags
                    ctx->state.entries[i].hovered = false;
                if (h) {                                          // set persistent hover on new target
                    WidgetState* s = ui_state_get(ctx, h->id);
                    s->hovered = true;
                }
            }
            break;
        }
        case UW_EV_MOUSEDOWN:
            if (e->button == 1) {                                 // left button only
                ctx->input.lmb = true;                            // record button as held
                ctx->input.x = e->x;
                ctx->input.y = e->y;
                if (ctx->root) {
                    Widget* h = hit_test(ctx->root, e->x, e->y);
                    if (h) {
                        WidgetState* s = ui_state_get(ctx, h->id);
                        s->pressed = true;                        // mark pressed
                        // focus follows the click: focusables take it, anything
                        // else drops it, so the caret ring never lingers on a
                        // widget the user has clearly moved away from
                        if (widget_focusable(h->kind)) ctx->focused_id = h->id;
                        else                           ctx->focused_id = 0;

                        if (h->kind == WK_SLIDER && !h->disabled) {  // begin slider drag
                            ctx->drag_widget_id = h->id;
                            double range = h->num_max - h->num_min;
                            int thumb_w = 16;
                            int track_w = (int)h->w - thumb_w;
                            if (track_w < 1) track_w = 1;
                            int local = e->x - (int)h->x - thumb_w / 2;
                            if (local < 0) local = 0;
                            if (local > track_w) local = track_w;
                            double t = (double)local / (double)track_w;
                            double v = h->num_min + t * range;
                            h->num_value = v;                     // jump to pointer position

                            UiEvent cev; memset(&cev, 0, sizeof(cev));  // emit change on down
                            cev.kind = EV_CHANGE;
                            cev.widget_id = h->id;
                            strncpy(cev.widget_type, "slider", 15);
                            if (h->key_str) strncpy(cev.key_name, h->key_str, 63);
                            cev.num_value = v;
                            ui_event_push(ctx, &cev);
                        }
                        ctx->redraw_needed = true;
                    } else {
                        ctx->focused_id = 0;                      // clicked outside any widget
                    }
                }
            }
            break;
        case UW_EV_MOUSEUP:
            if (e->button == 1) {                                 // left button released
                ctx->input.lmb = false;
                ctx->drag_widget_id = 0;                          // end any drag
                if (ctx->root) {
                    Widget* h = hit_test(ctx->root, e->x, e->y);
                    for (int i = 0; i < ctx->state.count; i++) {  // release all pressed flags
                        if (ctx->state.entries[i].pressed) {
                            ctx->state.entries[i].pressed = false;
                            if (h && h->id == ctx->state.entries[i].id) {
                                fire_click(ctx, h);               // click only if released on same widget
                            }
                        }
                    }
                    for (int i = 0; i < ctx->pool_used; i++)      // clear pool press flags
                        ctx->pool[i].pressed = false;
                    ctx->redraw_needed = true;
                }
            }
            break;
        case UW_EV_SCROLL: {                                      // wheel scrolled
            if (ctx->root) {
                Widget* h = hit_test(ctx->root, e->x, e->y);
                Widget* sc = h;
                while (sc && sc->kind != WK_SCROLL && sc->kind != WK_HSCROLL)
                    sc = sc->parent;                              // find enclosing scroll container
                if (sc && sc->kind == WK_SCROLL) {                // vertical scroll container
                    WidgetState* s = ui_state_get(ctx, sc->id);
                    s->scroll_y -= e->scroll_dy * 40.0;           // apply wheel delta
                    if (s->scroll_y < 0) s->scroll_y = 0;         // clamp to top
                    double max = sc->content_h - sc->h;           // max scrollable offset
                    if (max < 0) max = 0;
                    if (s->scroll_y > max) s->scroll_y = max;     // clamp to bottom
                    ctx->redraw_needed = true;
                } else {                                          // no container: emit raw scroll event
                    UiEvent ev; memset(&ev, 0, sizeof(ev));
                    ev.kind = EV_SCROLL;
                    ev.scroll_dy = e->scroll_dy;
                    ev.mouse_x = e->x;
                    ev.mouse_y = e->y;
                    ui_event_push(ctx, &ev);
                }
            }
            break;
        }
        case UW_EV_KEYDOWN: {                                     // key pressed
            if (ctx->focused_id && ctx->root) {                   // deliver to focused widget
                for (int i = 0; i < ctx->pool_used; i++) {
                    Widget* w = &ctx->pool[i];
                    if (w->id != ctx->focused_id) continue;       // skip non-focused

                    if (w->kind == WK_INPUT) {                    // input field editing
                        WidgetState* s = ui_state_get(ctx, w->id);
                        int cur = s->cursor < 0 ? (int)w->input_value.len : s->cursor;
                        int len = (int)w->input_value.len;

                        if (e->utf8_len > 0 && !(e->mods & 2)) {  // printable text, no ctrl
                            int need = len + e->utf8_len;
                            char* nb = (char*)malloc(need + 1);   // build new buffer
                            memcpy(nb, w->input_value.ptr, cur);  // prefix before cursor
                            memcpy(nb + cur, e->utf8, e->utf8_len);  // inserted text
                            memcpy(nb + cur + e->utf8_len,
                                   w->input_value.ptr + cur, len - cur);  // suffix after cursor
                            nb[need] = 0;
                            free(w->input_value.ptr);             // release old buffer
                            w->input_value.ptr = nb;              // install new buffer
                            w->input_value.len = need;
                            s->cursor = cur + e->utf8_len;        // advance cursor past insert
                            push_input_change(ctx, w, nb);        // emit change event
                            ctx->redraw_needed = true;
                            break;
                        }

                        if (e->key == 0xFF08 && cur > 0) {        // backspace
                            int prev = ui_utf8_prev(w->input_value.ptr, cur);
                            int need = len - (cur - prev);        // shrink by one codepoint
                            char* nb = (char*)malloc(need + 1);
                            memcpy(nb, w->input_value.ptr, prev); // prefix before removed char
                            memcpy(nb + prev, w->input_value.ptr + cur, len - cur);  // suffix after cursor
                            nb[need] = 0;
                            free(w->input_value.ptr);
                            w->input_value.ptr = nb;
                            w->input_value.len = need;
                            s->cursor = prev;                     // cursor moves to removed char
                            push_input_change(ctx, w, nb);
                            ctx->redraw_needed = true;
                            break;
                        }

                        if (e->key == 0xFF51) { s->cursor = ui_utf8_prev(w->input_value.ptr, cur); ctx->redraw_needed = true; break; }  // left arrow
                        if (e->key == 0xFF53) { s->cursor = ui_utf8_next(w->input_value.ptr, cur, len); ctx->redraw_needed = true; break; }  // right arrow
                        if (e->key == 0xFF50) { s->cursor = 0;   ctx->redraw_needed = true; break; }  // home
                        if (e->key == 0xFF57) { s->cursor = len; ctx->redraw_needed = true; break; }  // end
                    }

                    if (w->kind == WK_BUTTON &&
                        (e->key == 0x20 || e->key == 0xFF0D)) {   // space or enter activates
                        fire_click(ctx, w);
                        break;
                    }
                    if (w->kind == WK_CHECKBOX &&
                        (e->key == 0x20 || e->key == 0xFF0D)) {   // space or enter toggles
                        fire_click(ctx, w);
                        break;
                    }
                }
            }

            if (e->key == 0xFF09 && ctx->root) {                  // tab cycles focus
                Widget* focs[512];
                int n = 0;
                collect_focusable(ctx->root, focs, &n, 512);      // gather in draw order
                if (n > 0) {
                    int cur_idx = -1;
                    for (int i = 0; i < n; i++)
                        if (focs[i]->id == ctx->focused_id) { cur_idx = i; break; }
                    if (e->mods & 1) cur_idx = (cur_idx <= 0) ? n - 1 : cur_idx - 1;          // shift-tab goes back
                    else             cur_idx = (cur_idx < 0 || cur_idx == n - 1) ? 0 : cur_idx + 1;  // tab goes forward
                    ctx->focused_id = focs[cur_idx]->id;          // apply new focus
                    for (int i = 0; i < ctx->pool_used; i++)      // reflect on live widgets
                        ctx->pool[i].focused = (ctx->pool[i].id == ctx->focused_id);
                    ctx->redraw_needed = true;
                }
            }

            UiEvent ev; memset(&ev, 0, sizeof(ev));               // always also emit a raw key event
            ev.kind = EV_KEY;
            ev.widget_id = ctx->focused_id;
            ev.key_code = e->key;
            ev.mods = e->mods;
            if (e->utf8_len > 0) {                                // copy utf8 payload if any
                ev.str_value = (char*)malloc(e->utf8_len + 1);
                memcpy(ev.str_value, e->utf8, e->utf8_len);
                ev.str_value[e->utf8_len] = 0;
            }
            ui_event_push(ctx, &ev);
            break;
        }
        default: break;                                           // ignore other events
    }
}

// advances time-based widget state
//
// the only time-based state right now is the caret blink of the focused
// widget. dt is the wall-clock delta since the previous call; it is
// clamped by the caller so long pauses do not jump the phase forward.
// a repaint is requested only when the caret crosses the visible/hidden
// boundary so idle frames stay cheap.
void ui_tick(UiContext* ctx, double dt) {
    if (!ctx->focused_id) return;                        // nothing focused: nothing to animate
    if (dt <= 0.0) return;                               // no time passed

    WidgetState* s = ui_state_get(ctx, ctx->focused_id);
    double old_phase = s->blink_phase;                   // remember for boundary detection
    s->blink_phase += dt;                                // advance the blink clock
    if (s->blink_phase >= 1.0) s->blink_phase -= 1.0;    // wrap at one second

    if ((old_phase < 0.5) != (s->blink_phase < 0.5))     // crossed the visibility edge
        ctx->redraw_needed = true;                       // schedule a repaint
}

// pumps the window event loop once, returning true if events were queued
static bool pump(UiContext* ctx) {
    if (!ctx->window) return false;                               // no window: nothing to pump
    UiWinEvent we;
    while (ui_window_poll(ctx->window, &we)) {                    // drain pending events
        handle_win_event(ctx, &we);
        if (we.kind == UW_EV_CLOSE) break;                        // stop on close
    }
    if (ctx->queue.count > 0) return true;                        // events already queued
    if (!ui_window_is_open(ctx->window)) return false;            // window closed

    int timeout = ctx->redraw_needed ? 0 : 1;                     // poll quickly if repaint pending
    if (ui_window_wait(ctx->window, timeout)) {                   // block briefly for input
        while (ui_window_poll(ctx->window, &we)) {                // drain newly arrived events
            handle_win_event(ctx, &we);
            if (we.kind == UW_EV_CLOSE) break;
        }
    }

    // advance the blink clock; a static holds the previous wall-clock reading
    // because the tick has no other natural home between pumps
    static double s_last_tick = 0.0;
    double now = apex_now_seconds();
    double dt = now - s_last_tick;
    s_last_tick = now;
    if (dt < 0.0) dt = 0.0;                                       // clock went backwards
    if (dt > 0.25) dt = 0.25;                                     // clamp after a long pause
    ui_tick(ctx, dt);

    return ctx->queue.count > 0;                                  // true if anything was queued
}

// builtin: creates the native window if not already open
Value ui_impl_init(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) return MAKE_BOOL(true);                      // already initialised
    int w = 640, h = 480;                                         // default window size
    char* title = NULL;
    if (argc >= 1 && IS_TABLE(argv[0])) {                         // optional options table
        Table* t = AS_TABLE(argv[0]);
        w = (int)table_get_num(vm, t, "width", 640);              // width override
        h = (int)table_get_num(vm, t, "height", 480);             // height override
        title = table_get_str_dup(vm, t, "title");                // title override
    }
    const char* use_title = title ? title : "Apex UI";            // fall back to default title
    ctx->window = ui_window_create(w, h, use_title);              // create the host window
    free(title);                                                  // release the temp title
    if (!ctx->window) return MAKE_BOOL(false);                    // creation failed
    ctx->win_w = w;                                               // cache window size
    ctx->win_h = h;
    ctx->pixels = ui_window_pixels(ctx->window);                  // cache pixel buffer
    ctx->redraw_needed = true;                                    // force first paint

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

// rebuilds the widget tree from the last descriptor and paints it
static void rebuild_and_draw(UiContext* ctx) {
    ui_widgets_reset(ctx);                                        // drop the previous tree
    if (ctx->has_tree) {
        if (IS_TABLE(ctx->last_tree)) {
            Table* t = AS_TABLE(ctx->last_tree);
            Value first;
            if (table_get(t, MAKE_NUMBER(1.0), &first)) {         // array-style: multiple root children
                value_decref(first);
                Widget* root = ui_widget_alloc(ctx);              // synthesize a vbox root
                root->kind = WK_VBOX;
                root->id = ui_hash_id(0, "__root__", 0);
                root->padding = UI_PAD_SCREEN;
                root->gap = UI_GAP;
                for (int i = 1; ; i++) {                          // walk 1-based array children
                    Value vv;
                    if (!table_get(t, MAKE_NUMBER((double)i), &vv)) break;
                    Widget* ch = ui_build_tree(ctx, vv, root, i - 1);
                    if (ch) widget_add_child(root, ch);
                    value_decref(vv);
                }
                ctx->root = root;
            } else {                                              // single descriptor root
                ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);
            }
        } else {
            ctx->root = ui_build_tree(ctx, ctx->last_tree, NULL, 0);  // non-table descriptor
        }
    }
    if (!ctx->root) {                                             // no tree: synthesize an empty root
        Widget* root = ui_widget_alloc(ctx);
        root->kind = WK_VBOX;
        root->id = ui_hash_id(0, "__root__", 0);
        root->padding = UI_PAD_SCREEN;
        ctx->root = root;
    }
    ctx->root->x = 0;                                             // root fills the whole window
    ctx->root->y = 0;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)ctx->win_h;
    ui_layout(ctx);                                               // compute widget geometry
    ui_render_frame(ctx);                                         // draw into the pixel buffer
    ui_window_present(ctx->window);                               // blit to the screen
    ctx->redraw_needed = false;                                   // repaint satisfied
}

// repaints the existing widget tree without rebuilding it
static void redraw_only(UiContext* ctx) {
    if (!ctx->root) return;                                       // nothing to draw

    ctx->root->x = 0;                                             // root fills the whole window
    ctx->root->y = 0;
    ctx->root->w = (float)ctx->win_w;
    ctx->root->h = (float)ctx->win_h;

    ui_layout(ctx);                                               // recompute geometry
    ui_render_frame(ctx);                                         // redraw into the pixel buffer
    ui_window_present(ctx->window);                               // blit to the screen
    ctx->redraw_needed = false;                                   // repaint satisfied
}

// host window paint hook: refreshes buffers and repaints the existing tree
static void ui_window_paint_cb(void* user, int w, int h) {
    (void)w; (void)h;                                             // sizes read from the window itself
    UiContext* ctx = (UiContext*)user;
    if (!ctx || !ctx->window) return;                             // nothing to paint

    ctx->pixels = ui_window_pixels(ctx->window);                  // refresh pixel buffer
    ctx->win_w = ui_window_width(ctx->window);                    // refresh window size
    ctx->win_h = ui_window_height(ctx->window);

    redraw_only(ctx);                                             // repaint the current tree
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
        pump(ctx);                                                // drain native events
        if (ctx->redraw_needed) rebuild_and_draw(ctx);            // repaint if requested
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

// builtin: destroys the native window and marks the context as shut down
Value ui_impl_shutdown(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (ctx->window) {                                            // tear down the window if present
        ui_window_destroy(ctx->window);
        ctx->window = NULL;
    }
    ctx->quit_requested = true;                                   // mark the context as quit
    return MAKE_NONE();
}

// builtin: sets the native window title
Value ui_impl_set_title(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    if (!ctx->window) return MAKE_NONE();                         // no window: no-op
    if (argc >= 1 && IS_STRING(argv[0]))                          // only string titles are applied
        ui_window_set_title(ctx->window, AS_STRING(argv[0])->chars);
    return MAKE_NONE();
}

// builtin: returns the current window size as a {w, h} table
Value ui_impl_window_size(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;                                       // no arguments
    UiContext* ctx = ui_ensure(vm);                               // ensure the context exists
    Table* t = table_create(4);                                   // small result table
    Value k, v;
    k = intern(vm, "w"); v = MAKE_NUMBER(ctx->win_w);             // width field
    table_set(t, k, v); value_decref(k);
    k = intern(vm, "h"); v = MAKE_NUMBER(ctx->win_h);             // height field
    table_set(t, k, v); value_decref(k);
    return MAKE_TABLE(t);                                         // hand the table to the caller
}
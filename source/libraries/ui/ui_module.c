// source/libraries/ui/ui_module.c
// Implementation of UI builtin module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_module.h"
#include "apex_ui.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// recursion cap for the value bridge
#define UI_BRIDGE_MAX_DEPTH 64

// lazily creates the ui context on first use, attached to the vm
static struct UiContext* ui_ensure(VM* vm) {
    if (vm->ui) return vm->ui;                    // already created
    vm->ui = (struct UiContext*)apexui_context_create();
    return vm->ui;
}

// releases every ui-owned allocation; same entry the vm teardown already calls
void ui_context_destroy(struct UiContext* ctx) {
    apexui_context_destroy((ApexUiContext*)ctx);
}

// converts a vm value into a fresh apex ui value
static apexui_value v2u(Value v, int depth) {
    if (depth > UI_BRIDGE_MAX_DEPTH) return apexui_make_none();
    if (IS_STRING(v)) {
        StringObject* s = AS_STRING(v);
        return apexui_make_string(s->chars, s->length);
    }
    if (IS_TABLE(v)) {
        Table* t = AS_TABLE(v);
        apexui_value out = apexui_make_table(t->capacity > 8 ? t->capacity : 8);
        if (!apexui_is_table(out)) return out;    // allocation failed: none

        if (t->array_part) {                      // array part first, 1-based
            for (int i = 0; i < t->array_count; i++) {
                if (IS_NONE(t->array_part[i])) continue;
                apexui_value item = v2u(t->array_part[i], depth + 1);
                apexui_table_set(out, apexui_make_number((double)(i + 1)), item);
                apexui_value_release(item);
            }
        }
        if (t->entries) {                         // then the hash part
            for (int b = 0; b < t->capacity; b++) {
                for (TableEntry* e = t->entries[b]; e; e = e->next) {
                    apexui_value k = v2u(e->key, depth + 1);
                    apexui_value val = v2u(e->value, depth + 1);
                    apexui_table_set(out, k, val);
                    apexui_value_release(k);
                    apexui_value_release(val);
                }
            }
        }
        return out;
    }
    if (IS_BOOL(v))   return apexui_make_bool(AS_BOOL(v));
    if (IS_NUMBER(v)) return apexui_make_number(AS_NUMBER(v));
    return apexui_make_none();
}

// converts an apex ui value into a fresh vm value
static Value u2v(VM* vm, apexui_value v, int depth) {
    if (depth > UI_BRIDGE_MAX_DEPTH) return MAKE_NONE();
    if (apexui_is_string(v)) {
        StringObject* s = string_create(apexui_get_string_ptr(v),
                                        apexui_get_string_len(v));
        return s ? MAKE_STRING(s) : MAKE_NONE();
    }
    if (apexui_is_table(v)) {
        Table* t = table_create(8);
        if (!t) return MAKE_NONE();
        int n = 0;
        apexui_value* keys = apexui_table_keys(v, &n);
        for (int i = 0; i < n; i++) {
            apexui_value item;
            if (apexui_table_get(v, keys[i], &item)) {
                Value k = u2v(vm, keys[i], depth + 1);
                Value val = u2v(vm, item, depth + 1);
                table_set(t, k, val);
                value_decref(k);
                value_decref(val);
                apexui_value_release(item);
            }
            apexui_value_release(keys[i]);
        }
        free(keys);
        return MAKE_TABLE(t);
    }
    if (apexui_is_bool(v))   return MAKE_BOOL(apexui_get_bool(v));
    if (apexui_is_number(v)) return MAKE_NUMBER(apexui_get_number(v));
    return MAKE_NONE();
}

// interns a c string into the vm's intern table, returning a tagged string value
static Value intern(VM* vm, const char* s) {
    return MAKE_STRING(string_intern(&vm->intern_table, s, (int)strlen(s)));
}

// sets a string-keyed entry in a table, releasing the interned key reference
static void tset(VM* vm, Table* t, const char* k, Value v) {
    Value kk = intern(vm, k);                    // intern the key once
    table_set(t, kk, v);                         // table_set takes its own reference
    value_decref(kk);                            // drop the local key reference
}

// merges an options table's fields into a vm-side descriptor table
static void merge_opts(VM* vm, Table* t, Value opts) {
    (void)vm;                                    // kept for signature symmetry
    if (!IS_TABLE(opts)) return;
    Table* o = AS_TABLE(opts);
    int nk;
    Value* keys = table_keys(o, &nk);
    for (int i = 0; i < nk; i++) {
        Value vv;
        if (table_get(o, keys[i], &vv)) {
            table_set(t, keys[i], vv);
            value_decref(vv);
        }
        value_decref(keys[i]);
    }
    free(keys);
}

// builds a leaf widget descriptor table of the given kind
static Value make_leaf(VM* vm, const char* type, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, type));        // "__t" discriminates the widget kind
    if (argc >= 1) tset(vm, t, "content", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);              // merge options fields
    }
    return MAKE_TABLE(t);                        // caller owns the descriptor
}

// builds a container widget descriptor table of the given kind
Value ui_ctor_container(VM* vm, int argc, Value* argv, const char* type) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, type));
    if (argc >= 1 && IS_TABLE(argv[0])) tset(vm, t, "children", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a button widget descriptor
Value ui_ctor_button(VM* vm, int argc, Value* argv) {
    return make_leaf(vm, "button", argc, argv);
}

// builds a leaf descriptor under an explicit kind (ui.text / ui.heading / ...)
Value ui_ctor_text(VM* vm, int argc, Value* argv, const char* type) {
    return make_leaf(vm, type, argc, argv);
}

// builds a checkbox widget descriptor
static Value ctor_checkbox(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "checkbox"));
    if (argc >= 1) tset(vm, t, "content", argv[0]);  // checkbox label
    if (argc >= 2 && IS_BOOL(argv[1])) tset(vm, t, "checked", argv[1]);
    else if (argc >= 2 && IS_NUMBER(argv[1]))
        tset(vm, t, "checked", MAKE_BOOL(AS_NUMBER(argv[1]) != 0));
    if (argc >= 3 && IS_TABLE(argv[2])) {
        tset(vm, t, "opts", argv[2]);
        merge_opts(vm, t, argv[2]);
    }
    return MAKE_TABLE(t);
}

// builds a single-line text input widget descriptor
static Value ctor_input(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "input"));
    if (argc >= 1) tset(vm, t, "value_text", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a multi-line text area descriptor
static Value ctor_text_area(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "text_area"));
    if (argc >= 1) tset(vm, t, "value_text", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a dropdown widget descriptor
static Value ctor_dropdown(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "dropdown"));
    if (argc >= 1) tset(vm, t, "value", argv[0]);
    if (argc >= 2) tset(vm, t, "options", argv[1]);
    if (argc >= 3 && IS_TABLE(argv[2])) {
        tset(vm, t, "opts", argv[2]);
        merge_opts(vm, t, argv[2]);
    }
    return MAKE_TABLE(t);
}

// builds a number input widget descriptor
static Value ctor_number_input(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "number_input"));
    if (argc >= 1) tset(vm, t, "value", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a table widget descriptor from a column list and a row list
static Value ctor_table(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "table"));
    if (argc >= 1) tset(vm, t, "columns", argv[0]);
    if (argc >= 2) tset(vm, t, "rows", argv[1]);
    if (argc >= 3 && IS_TABLE(argv[2])) {
        tset(vm, t, "opts", argv[2]);
        merge_opts(vm, t, argv[2]);
    }
    return MAKE_TABLE(t);
}

// builds a tree widget descriptor from a node array and options
static Value ctor_tree(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "tree"));
    if (argc >= 1) tset(vm, t, "nodes", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a tabs widget descriptor from a tabs array and options
static Value ctor_tabs(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "tabs"));
    if (argc >= 1) tset(vm, t, "tabs", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds a statusbar widget descriptor
Value ui_ctor_statusbar(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "statusbar"));
    if (argc >= 1 && IS_TABLE(argv[0])) {
        tset(vm, t, "opts", argv[0]);
        merge_opts(vm, t, argv[0]);
    }
    return MAKE_TABLE(t);
}

// builds a flexible spacer widget descriptor
static Value ctor_spacer(VM* vm, int argc, Value* argv) {
    Table* t = table_create(4);
    tset(vm, t, "__t", intern(vm, "spacer"));
    if (argc >= 1 && IS_NUMBER(argv[0])) {       // numeric arg: fixed min size
        tset(vm, t, "min_width", argv[0]);
        tset(vm, t, "min_height", argv[0]);
    } else {                                     // default: flex spacer
        tset(vm, t, "weight", MAKE_NUMBER(1.0));
    }
    if (argc >= 2 && IS_TABLE(argv[1])) {
        tset(vm, t, "opts", argv[1]);
        merge_opts(vm, t, argv[1]);
    }
    return MAKE_TABLE(t);
}

// builds the ui.key table of named key / modifier constants from the public apex ui macros
static Value ctor_key_table(VM* vm) {
    Table* t = table_create(32);
    tset(vm, t, "esc",       MAKE_NUMBER((double)APEXUI_KEY_ESCAPE));
    tset(vm, t, "enter",     MAKE_NUMBER((double)APEXUI_KEY_ENTER));
    tset(vm, t, "tab",       MAKE_NUMBER((double)APEXUI_KEY_TAB));
    tset(vm, t, "backspace", MAKE_NUMBER((double)APEXUI_KEY_BACKSPACE));
    tset(vm, t, "delete",    MAKE_NUMBER((double)APEXUI_KEY_DELETE));
    tset(vm, t, "insert",    MAKE_NUMBER((double)APEXUI_KEY_INSERT));
    tset(vm, t, "home",      MAKE_NUMBER((double)APEXUI_KEY_HOME));
    tset(vm, t, "end",       MAKE_NUMBER((double)APEXUI_KEY_END));
    tset(vm, t, "page_up",   MAKE_NUMBER((double)APEXUI_KEY_PAGE_UP));
    tset(vm, t, "page_down", MAKE_NUMBER((double)APEXUI_KEY_PAGE_DOWN));
    tset(vm, t, "left",      MAKE_NUMBER((double)APEXUI_KEY_LEFT));
    tset(vm, t, "right",     MAKE_NUMBER((double)APEXUI_KEY_RIGHT));
    tset(vm, t, "up",        MAKE_NUMBER((double)APEXUI_KEY_UP));
    tset(vm, t, "down",      MAKE_NUMBER((double)APEXUI_KEY_DOWN));
    tset(vm, t, "space",     MAKE_NUMBER((double)APEXUI_KEY_SPACE));
    tset(vm, t, "f1",        MAKE_NUMBER((double)APEXUI_KEY_F1));
    tset(vm, t, "f2",        MAKE_NUMBER((double)APEXUI_KEY_F2));
    tset(vm, t, "f3",        MAKE_NUMBER((double)APEXUI_KEY_F3));
    tset(vm, t, "f4",        MAKE_NUMBER((double)APEXUI_KEY_F4));
    tset(vm, t, "f5",        MAKE_NUMBER((double)APEXUI_KEY_F5));
    tset(vm, t, "f6",        MAKE_NUMBER((double)APEXUI_KEY_F6));
    tset(vm, t, "f7",        MAKE_NUMBER((double)APEXUI_KEY_F7));
    tset(vm, t, "f8",        MAKE_NUMBER((double)APEXUI_KEY_F8));
    tset(vm, t, "f9",        MAKE_NUMBER((double)APEXUI_KEY_F9));
    tset(vm, t, "f10",       MAKE_NUMBER((double)APEXUI_KEY_F10));
    tset(vm, t, "f11",       MAKE_NUMBER((double)APEXUI_KEY_F11));
    tset(vm, t, "f12",       MAKE_NUMBER((double)APEXUI_KEY_F12));
    tset(vm, t, "shift",     MAKE_NUMBER((double)APEXUI_MOD_SHIFT));
    tset(vm, t, "ctrl",      MAKE_NUMBER((double)APEXUI_MOD_CTRL));
    tset(vm, t, "alt",       MAKE_NUMBER((double)APEXUI_MOD_ALT));
    return MAKE_TABLE(t);
}

// calls an apex function value synchronously by running it as a one-shot coroutine
static Value apex_call_function(VM* vm, Value func, int argc, Value* argv) {
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

// state passed from ui_impl_run into the event trampoline
typedef struct {
    VM*   vm;                                         // owning vm
    Value fn;                                         // the script callback
} RunBridge;

// trampoline: converts the apex ui event table into vm values
static apexui_value run_trampoline(void* user, apexui_value event) {
    RunBridge* rb = (RunBridge*)user;
    VM* vm = rb->vm;

    Value ev_tbl = u2v(vm, event, 0);                 // vm-owned event table
    Value result = apex_call_function(vm, rb->fn, 1, &ev_tbl);
    value_decref(ev_tbl);

    apexui_value tree = v2u(result, 0);               // bridge the staged tree
    value_decref(result);
    return tree;                                      // ownership -> apex ui
}

// builtin: ui.init(opts?) — creates the native window if not already open
Value ui_impl_init(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);            // ensure the context exists
    if (!ctx) return MAKE_BOOL(false);
    apexui_value opts = argc >= 1 ? v2u(argv[0], 0) : apexui_make_none();
    Value out = MAKE_BOOL(apexui_get_bool(apexui_init((ApexUiContext*)ctx, opts)));
    apexui_value_release(opts);
    return out;
}

// builtin: ui.render(tree?) — records the current widget tree for painting
Value ui_impl_render(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value tree = argc >= 1 ? v2u(argv[0], 0) : apexui_make_none();
    apexui_render_value((ApexUiContext*)ctx, tree);  // impl retains its own copy
    apexui_value_release(tree);                       // drop our bridge copy
    return MAKE_NONE();
}

// builtin: ui.next_event() — blocks until an event is available
Value ui_impl_next_event(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value ev = apexui_next_event((ApexUiContext*)ctx);
    if (apexui_is_none(ev)) return MAKE_NONE();
    Value out = u2v(vm, ev, 0);                       // convert for the script
    apexui_value_release(ev);
    return out;
}

// builtin: ui.run(callback) — drive the message loop with batched repaints
Value ui_impl_run(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    if (argc < 1 || !IS_FUNCTION(argv[0])) return MAKE_NONE();  // callback required

    RunBridge rb;
    rb.vm = vm;
    rb.fn = argv[0];
    value_incref(rb.fn);                              // keep alive across the loop

    apexui_run((ApexUiContext*)ctx, run_trampoline, &rb);

    value_decref(rb.fn);
    return MAKE_NONE();
}

// builtin: ui.shutdown() — destroys the native window
Value ui_impl_shutdown(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_shutdown((ApexUiContext*)ctx);
    return MAKE_NONE();
}

// builtin: ui.set_title(title) — sets the native window title
Value ui_impl_set_title(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    if (argc >= 1 && IS_STRING(argv[0]))
        apexui_set_title((ApexUiContext*)ctx, AS_STRING(argv[0])->chars);
    return MAKE_NONE();
}

// builtin: ui.window_size() — returns the current size as a {w, h} table
Value ui_impl_window_size(VM* vm, int argc, Value* argv) {
    (void)argc; (void)argv;
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value sz = apexui_window_size((ApexUiContext*)ctx);
    Value out = u2v(vm, sz, 0);
    apexui_value_release(sz);
    return out;
}

// parses the optional options table shared by the pickers into an apex ui options value
static apexui_value bridge_picker_opts(VM* vm, int argc, Value* argv) {
    (void)vm;                                    // the options table shape matches one to one
    if (argc < 1 || !IS_TABLE(argv[0])) return apexui_make_none();

    // the descriptor shape matches one to one: title / default / filters
    return v2u(argv[0], 0);
}

// builtin: ui.open_file(opts?) — native "open file" dialog
Value ui_impl_open_file(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value opts = bridge_picker_opts(vm, argc, argv);
    apexui_value picked = apexui_open_file((ApexUiContext*)ctx, opts);
    apexui_value_release(opts);
    if (apexui_is_none(picked)) return MAKE_NONE();
    StringObject* s = string_create(apexui_get_string_ptr(picked),
                                    apexui_get_string_len(picked));
    apexui_value_release(picked);
    return s ? MAKE_STRING(s) : MAKE_NONE();
}

// builtin: ui.open_folder(opts?) — native "open folder" dialog
Value ui_impl_open_folder(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value opts = bridge_picker_opts(vm, argc, argv);
    apexui_value picked = apexui_open_folder((ApexUiContext*)ctx, opts);
    apexui_value_release(opts);
    if (apexui_is_none(picked)) return MAKE_NONE();
    StringObject* s = string_create(apexui_get_string_ptr(picked),
                                    apexui_get_string_len(picked));
    apexui_value_release(picked);
    return s ? MAKE_STRING(s) : MAKE_NONE();
}

// builtin: ui.save_as(content, opts?) — native "save file" dialog + write
Value ui_impl_save_as(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    if (argc < 1 || !IS_STRING(argv[0])) return MAKE_NONE();
    StringObject* content = AS_STRING(argv[0]);

    apexui_value opts = argc >= 2 ? bridge_picker_opts(vm, 1, argv + 1)
                                  : apexui_make_none();
    apexui_value picked = apexui_save_as((ApexUiContext*)ctx, content->chars,
                                         content->length, opts);
    apexui_value_release(opts);
    if (apexui_is_none(picked)) return MAKE_NONE();
    StringObject* s = string_create(apexui_get_string_ptr(picked),
                                    apexui_get_string_len(picked));
    apexui_value_release(picked);
    return s ? MAKE_STRING(s) : MAKE_NONE();
}

// builtin: ui.fullscreen(opts?) — toggle or set the window's fullscreen state
Value ui_impl_fullscreen(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_BOOL(false);
    bool state = (argc >= 1 && IS_BOOL(argv[0]))
                     ? apexui_set_fullscreen((ApexUiContext*)ctx, AS_BOOL(argv[0]))
                     : apexui_toggle_fullscreen((ApexUiContext*)ctx);
    return MAKE_BOOL(state);
}

// builtin: ui.dialog(table) — modal dialog with user-defined content
static Value ctor_dialog(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value descr = argc >= 1 ? v2u(argv[0], 0) : apexui_make_none();
    apexui_dialog((ApexUiContext*)ctx, descr);
    apexui_value_release(descr);
    return MAKE_NONE();
}

// ui.notification(table) — show a desktop notification popup
static Value ctor_notification(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value descr = argc >= 1 ? v2u(argv[0], 0) : apexui_make_none();
    apexui_notification((ApexUiContext*)ctx, descr);
    apexui_value_release(descr);
    return MAKE_NONE();
}

// ui.menubar(table) — set the window menu bar; non-table or missing clears it
static Value ctor_menubar(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    apexui_value descr = (argc >= 1 && IS_TABLE(argv[0]))
                             ? v2u(argv[0], 0)
                             : apexui_make_none();
    apexui_menubar((ApexUiContext*)ctx, descr);
    apexui_value_release(descr);
    return MAKE_NONE();
}

// ui.context_menu(items?, x?, y?) — open at cursor / at (x, y); none closes
static Value ctor_context_menu(VM* vm, int argc, Value* argv) {
    struct UiContext* ctx = ui_ensure(vm);
    if (!ctx) return MAKE_NONE();
    ApexUiContext* ui = (ApexUiContext*)ctx;
    if (argc < 1 || !IS_TABLE(argv[0])) {
        apexui_context_menu_close(ui);                // none or non-table: close
        return MAKE_NONE();
    }
    apexui_value items = v2u(argv[0], 0);             // bridge the item descriptors
    if (argc >= 3 && IS_NUMBER(argv[1]) && IS_NUMBER(argv[2])) {
        apexui_context_menu_open_at(ui, items,       // explicit position wins
                                    (int)AS_NUMBER(argv[1]), (int)AS_NUMBER(argv[2]));
    } else {
        apexui_context_menu_open(ui, items);          // default: cursor position
    }
    apexui_value_release(items);
    return MAKE_NONE();
}

// dispatches ui.* builtin calls from the vm into the apex ui library
bool ui_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {
    int argc = arg_count;
    Value* argv = args;

    if (strcmp(name, "ui.key") == 0)          { *result = ctor_key_table(vm); return true; }
    if (strcmp(name, "ui.init") == 0)          { *result = ui_impl_init(vm, argc, argv); return true; }
    if (strcmp(name, "ui.shutdown") == 0)      { *result = ui_impl_shutdown(vm, argc, argv); return true; }
    if (strcmp(name, "ui.render") == 0)        { *result = ui_impl_render(vm, argc, argv); return true; }
    if (strcmp(name, "ui.next_event") == 0)    { *result = ui_impl_next_event(vm, argc, argv); return true; }
    if (strcmp(name, "ui.run") == 0)           { *result = ui_impl_run(vm, argc, argv); return true; }
    if (strcmp(name, "ui.set_title") == 0)     { *result = ui_impl_set_title(vm, argc, argv); return true; }
    if (strcmp(name, "ui.window_size") == 0)   { *result = ui_impl_window_size(vm, argc, argv); return true; }
    if (strcmp(name, "ui.open_file") == 0)     { *result = ui_impl_open_file(vm, argc, argv); return true; }
    if (strcmp(name, "ui.open_folder") == 0)   { *result = ui_impl_open_folder(vm, argc, argv); return true; }
    if (strcmp(name, "ui.save_as") == 0)       { *result = ui_impl_save_as(vm, argc, argv); return true; }
    if (strcmp(name, "ui.fullscreen") == 0)    { *result = ui_impl_fullscreen(vm, argc, argv); return true; }
    if (strcmp(name, "ui.dialog") == 0)        { *result = ctor_dialog(vm, argc, argv); return true; }
    if (strcmp(name, "ui.notification") == 0)  { *result = ctor_notification(vm, argc, argv); return true; }
    if (strcmp(name, "ui.menubar") == 0)       { *result = ctor_menubar(vm, argc, argv); return true; }
    if (strcmp(name, "ui.context_menu") == 0)  { *result = ctor_context_menu(vm, argc, argv); return true; }
    if (strcmp(name, "ui.statusbar") == 0)     { *result = ui_ctor_statusbar(vm, argc, argv); return true; }

    if (strcmp(name, "ui.text") == 0)     { *result = ui_ctor_text(vm, argc, argv, "text"); return true; }
    if (strcmp(name, "ui.heading") == 0)  { *result = ui_ctor_text(vm, argc, argv, "heading"); return true; }
    if (strcmp(name, "ui.separator") == 0){ *result = ui_ctor_text(vm, argc, argv, "separator"); return true; }

    if (strcmp(name, "ui.button") == 0)   { *result = ui_ctor_button(vm, argc, argv); return true; }
    if (strcmp(name, "ui.checkbox") == 0) { *result = ctor_checkbox(vm, argc, argv); return true; }
    if (strcmp(name, "ui.input") == 0)    { *result = ctor_input(vm, argc, argv); return true; }
    if (strcmp(name, "ui.number_input") == 0) { *result = ctor_number_input(vm, argc, argv); return true; }
    if (strcmp(name, "ui.text_area") == 0){ *result = ctor_text_area(vm, argc, argv); return true; }
    if (strcmp(name, "ui.dropdown") == 0) { *result = ctor_dropdown(vm, argc, argv); return true; }
    if (strcmp(name, "ui.table") == 0)    { *result = ctor_table(vm, argc, argv); return true; }
    if (strcmp(name, "ui.tree") == 0)     { *result = ctor_tree(vm, argc, argv); return true; }
    if (strcmp(name, "ui.tabs") == 0)     { *result = ctor_tabs(vm, argc, argv); return true; }
    if (strcmp(name, "ui.spacer") == 0)   { *result = ctor_spacer(vm, argc, argv); return true; }

    if (strcmp(name, "ui.vbox") == 0)    { *result = ui_ctor_container(vm, argc, argv, "vbox"); return true; }
    if (strcmp(name, "ui.hbox") == 0)    { *result = ui_ctor_container(vm, argc, argv, "hbox"); return true; }
    if (strcmp(name, "ui.panel") == 0)   { *result = ui_ctor_container(vm, argc, argv, "panel"); return true; }
    if (strcmp(name, "ui.vscroll") == 0) { *result = ui_ctor_container(vm, argc, argv, "vscroll"); return true; }
    if (strcmp(name, "ui.hscroll") == 0) { *result = ui_ctor_container(vm, argc, argv, "hscroll"); return true; }

    return false;                                // not a ui builtin, let other modules try
}
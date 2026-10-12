// source/libraries/ui/ui_module.c
// Implementation of UI builtin module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_module.h"
#include "ui_internal.h"
#include <string.h>
#include <stdlib.h>

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

// builds a leaf widget descriptor table of the given kind with content and options
static Value make_leaf(VM* vm, const char* type, int argc, Value* argv) {
    Table* t = table_create(8);                  // descriptor table for the leaf
    tset(vm, t, "__t", intern(vm, type));        // "__t" discriminates the widget kind
    if (argc >= 1) tset(vm, t, "content", argv[0]);  // first arg is the label/content
    if (argc >= 2 && IS_TABLE(argv[1])) tset(vm, t, "opts", argv[1]);  // keep options table too
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields into the descriptor
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);  // snapshot option keys
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) {
                table_set(t, keys[i], vv);       // copy each option into the descriptor
                value_decref(vv);
            }
            value_decref(keys[i]);               // release each key from the snapshot
        }
        free(keys);
    }
    return MAKE_TABLE(t);                        // caller owns the descriptor
}

// builds a container widget descriptor of the given kind with children and options
Value ui_ctor_container(VM* vm, int argc, Value* argv, const char* type) {
    Table* t = table_create(8);                  // descriptor table for the container
    tset(vm, t, "__t", intern(vm, type));        // "__t" discriminates the widget kind
    if (argc >= 1 && IS_TABLE(argv[0])) tset(vm, t, "children", argv[0]);  // children table
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields into the descriptor
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) {
                table_set(t, keys[i], vv);
                value_decref(vv);
            }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);                        // caller owns the descriptor
}

// builds a button widget descriptor
static Value ctor_button(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "button"));
    if (argc >= 1) tset(vm, t, "content", argv[0]);  // button label
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a checkbox widget descriptor
static Value ctor_checkbox(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "checkbox"));
    if (argc >= 1) tset(vm, t, "content", argv[0]);  // checkbox label
    if (argc >= 2 && IS_BOOL(argv[1])) tset(vm, t, "checked", argv[1]);  // explicit bool state
    else if (argc >= 2 && IS_NUMBER(argv[1])) tset(vm, t, "checked", MAKE_BOOL(AS_NUMBER(argv[1]) != 0));  // numeric truthiness
    if (argc >= 3 && IS_TABLE(argv[2])) {        // merge options fields
        Table* opts = AS_TABLE(argv[2]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a radio button widget descriptor
static Value ctor_radio(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "radio"));
    if (argc >= 1) tset(vm, t, "content", argv[0]);  // radio label
    if (argc >= 2) tset(vm, t, "value", argv[1]);    // value this radio represents
    if (argc >= 3) tset(vm, t, "group", argv[2]);    // group name for exclusivity
    if (argc >= 4 && IS_TABLE(argv[3])) {        // merge options fields
        Table* opts = AS_TABLE(argv[3]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a slider widget descriptor
static Value ctor_slider(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "slider"));
    if (argc >= 1) tset(vm, t, "min", argv[0]);      // range minimum
    if (argc >= 2) tset(vm, t, "max", argv[1]);      // range maximum
    if (argc >= 3) tset(vm, t, "value", argv[2]);    // starting value
    if (argc >= 4 && IS_TABLE(argv[3])) {        // merge options fields
        Table* opts = AS_TABLE(argv[3]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a single-line text input widget descriptor
static Value ctor_input(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "input"));
    if (argc >= 1) tset(vm, t, "value_text", argv[0]);  // initial editable text
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a combobox widget descriptor
static Value ctor_combobox(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "combobox"));
    if (argc >= 1) tset(vm, t, "value", argv[0]);
    if (argc >= 2) tset(vm, t, "options", argv[1]);
    if (argc >= 3 && IS_TABLE(argv[2])) {
        Table* opts = AS_TABLE(argv[2]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// builds a progress bar widget descriptor
static Value ctor_progress(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "progress"));
    if (argc >= 1) tset(vm, t, "value", argv[0]);    // current progress value
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
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
    } else if (argc >= 1) {                      // non-numeric arg: flex spacer
        tset(vm, t, "weight", MAKE_NUMBER(1.0));
    } else {                                     // no arg: flex spacer by default
        tset(vm, t, "weight", MAKE_NUMBER(1.0));
    }
    if (argc >= 2 && IS_TABLE(argv[1])) {        // merge options fields
        Table* opts = AS_TABLE(argv[1]);
        int nk; Value* keys = table_keys(opts, &nk);
        for (int i = 0; i < nk; i++) {
            Value vv;
            if (table_get(opts, keys[i], &vv)) { table_set(t, keys[i], vv); value_decref(vv); }
            value_decref(keys[i]);
        }
        free(keys);
    }
    return MAKE_TABLE(t);
}

// named key constants exposed to scripts as ui.key_*()
static const struct { const char* name; int value; } UI_KEY_TABLE[] = {
    {"ui.key_escape",    UI_KEY_ESCAPE},
    {"ui.key_enter",     UI_KEY_ENTER},
    {"ui.key_tab",       UI_KEY_TAB},
    {"ui.key_backspace", UI_KEY_BACKSPACE},
    {"ui.key_delete",    UI_KEY_DELETE},
    {"ui.key_insert",    UI_KEY_INSERT},
    {"ui.key_home",      UI_KEY_HOME},
    {"ui.key_end",       UI_KEY_END},
    {"ui.key_page_up",   UI_KEY_PAGE_UP},
    {"ui.key_page_down", UI_KEY_PAGE_DOWN},
    {"ui.key_left",      UI_KEY_LEFT},
    {"ui.key_right",     UI_KEY_RIGHT},
    {"ui.key_up",        UI_KEY_UP},
    {"ui.key_down",      UI_KEY_DOWN},
    {"ui.key_space",     UI_KEY_SPACE},
    {"ui.key_f1",        UI_KEY_F1},
    {"ui.key_f2",        UI_KEY_F2},
    {"ui.key_f3",        UI_KEY_F3},
    {"ui.key_f4",        UI_KEY_F4},
    {"ui.key_f5",        UI_KEY_F5},
    {"ui.key_f6",        UI_KEY_F6},
    {"ui.key_f7",        UI_KEY_F7},
    {"ui.key_f8",        UI_KEY_F8},
    {"ui.key_f9",        UI_KEY_F9},
    {"ui.key_f10",       UI_KEY_F10},
    {"ui.key_f11",       UI_KEY_F11},
    {"ui.key_f12",       UI_KEY_F12},
    {"ui.mod_shift",     UI_MOD_SHIFT},
    {"ui.mod_ctrl",      UI_MOD_CTRL},
    {"ui.mod_alt",       UI_MOD_ALT},
};

// looks up a ui.key_* / ui.mod_* constant; writes the value and returns true on hit
static bool ui_key_lookup(const char* name, Value* out) {
    for (size_t i = 0; i < sizeof(UI_KEY_TABLE) / sizeof(UI_KEY_TABLE[0]); i++) {
        if (strcmp(name, UI_KEY_TABLE[i].name) == 0) {
            *out = MAKE_NUMBER((double)UI_KEY_TABLE[i].value);
            return true;
        }
    }
    return false;
}

// dispatches ui.* builtin calls from the vm into the ui library
bool ui_call_builtin(VM* vm, const char* name, int argc, Value* argv, Value* result) {
    if (ui_key_lookup(name, result)) return true;   // ui.key_* / ui.mod_* constants
    if (strcmp(name, "ui.init") == 0)          { *result = ui_impl_init(vm, argc, argv); return true; }         // open the native window
    if (strcmp(name, "ui.shutdown") == 0)      { *result = ui_impl_shutdown(vm, argc, argv); return true; }     // close the native window
    if (strcmp(name, "ui.render") == 0)        { *result = ui_impl_render(vm, argc, argv); return true; }       // set the current widget tree
    if (strcmp(name, "ui.next_event") == 0)    { *result = ui_impl_next_event(vm, argc, argv); return true; }   // block until the next event
    if (strcmp(name, "ui.run") == 0)           { *result = ui_impl_run(vm, argc, argv); return true; }          // batched message loop
    if (strcmp(name, "ui.set_title") == 0)     { *result = ui_impl_set_title(vm, argc, argv); return true; }    // change the window title
    if (strcmp(name, "ui.window_size") == 0)   { *result = ui_impl_window_size(vm, argc, argv); return true; }  // query the window size

    if (strcmp(name, "ui.text") == 0)     { *result = make_leaf(vm, "text", argc, argv); return true; }         // plain text block
    if (strcmp(name, "ui.heading") == 0)  { *result = make_leaf(vm, "heading", argc, argv); return true; }      // section heading
    if (strcmp(name, "ui.badge") == 0)    { *result = make_leaf(vm, "badge", argc, argv); return true; }        // small status pill
    if (strcmp(name, "ui.separator") == 0){ *result = make_leaf(vm, "separator", argc, argv); return true; }    // horizontal rule

    if (strcmp(name, "ui.button") == 0)   { *result = ctor_button(vm, argc, argv); return true; }               // push button
    if (strcmp(name, "ui.checkbox") == 0) { *result = ctor_checkbox(vm, argc, argv); return true; }             // boolean toggle
    if (strcmp(name, "ui.radio") == 0)    { *result = ctor_radio(vm, argc, argv); return true; }                // radio option
    if (strcmp(name, "ui.slider") == 0)   { *result = ctor_slider(vm, argc, argv); return true; }               // numeric slider
    if (strcmp(name, "ui.progress") == 0) { *result = ctor_progress(vm, argc, argv); return true; }             // progress bar
    if (strcmp(name, "ui.input") == 0){ *result = ctor_input(vm, argc, argv); return true; }                    // editable text field
    if (strcmp(name, "ui.combobox") == 0) { *result = ctor_combobox(vm, argc, argv); return true; }             // dropdown selector
    if (strcmp(name, "ui.spacer") == 0)   { *result = ctor_spacer(vm, argc, argv); return true; }               // flexible gap

    if (strcmp(name, "ui.vbox") == 0)   { *result = ui_ctor_container(vm, argc, argv, "vbox"); return true; }    // vertical stack
    if (strcmp(name, "ui.hbox") == 0)   { *result = ui_ctor_container(vm, argc, argv, "hbox"); return true; }    // horizontal row
    if (strcmp(name, "ui.panel") == 0)  { *result = ui_ctor_container(vm, argc, argv, "panel"); return true; }   // framed panel
    if (strcmp(name, "ui.scroll") == 0) { *result = ui_ctor_container(vm, argc, argv, "scroll"); return true; }  // vertical scroller
    if (strcmp(name, "ui.hscroll") == 0){ *result = ui_ctor_container(vm, argc, argv, "hscroll"); return true; } // horizontal scroller

    return false;                                // not a ui builtin, let other modules try
}
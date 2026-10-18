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

// builds an switch widget descriptor
static Value ctor_switch(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "switch"));
    if (argc >= 1) tset(vm, t, "content", argv[0]);                    // label, may be ""
    if (argc >= 2 && IS_BOOL(argv[1])) tset(vm, t, "checked", argv[1]); // explicit bool state
    else if (argc >= 2 && IS_NUMBER(argv[1]))
        tset(vm, t, "checked", MAKE_BOOL(AS_NUMBER(argv[1]) != 0));    // numeric truthiness
    if (argc >= 3 && IS_TABLE(argv[2])) {                              // merge options fields
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

// builds a multi-line text area descriptor
static Value ctor_text_area(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "text_area"));            // discriminator
    if (argc >= 1) tset(vm, t, "value_text", argv[0]);      // initial content
    if (argc >= 2 && IS_TABLE(argv[1])) {                   // merge options
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

// builtin: ui.dialog(table) — modal dialog with user-defined content
static Value ctor_dialog(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                                             // ensure the context exists
    if (argc < 1 || !IS_TABLE(argv[0])) return MAKE_NONE();                     // descriptor table required
    Table* t = AS_TABLE(argv[0]);                                               // unwrap the descriptor

    char* key   = table_get_str_dup(vm, t, "key");                              // script-visible id
    char* title = table_get_str_dup(vm, t, "title");                            // heading text
    char* msg   = table_get_str_dup(vm, t, "message");                          // body text
    dialog_begin(ctx, key ? key : "", title ? title : "", msg ? msg : "");      // stage a fresh dialog
    free(key); free(title); free(msg);                                          // dialog owns its own copies

    // fields
    Value fkey = intern(vm, "fields"), fv;                                      // cached key + value slot
    if (table_get(t, fkey, &fv)) {                                              // optional "fields" table
        if (IS_TABLE(fv)) {
            Table* ft = AS_TABLE(fv);                                           // the field list
            int n = ft->array_part ? ft->array_count : 0;                       // number of array entries
            for (int i = 0; i < n; i++) {
                Value item = ft->array_part[i];                                 // one field descriptor
                if (!IS_TABLE(item)) continue;                                  // skip non-tables
                Table* it = AS_TABLE(item);
                char* fk   = table_get_str_dup(vm, it, "key");                  // field key
                char* flbl = table_get_str_dup(vm, it, "label");                // field caption
                char* fph  = table_get_str_dup(vm, it, "placeholder");          // placeholder text
                char* fdef = table_get_str_dup(vm, it, "default");              // initial value
                dialog_add_field(ctx,
                                 fk   ? fk   : "",                              // fall back to empty
                                 flbl ? flbl : "",
                                 fph  ? fph  : "",
                                 fdef ? fdef : "");
                free(fk); free(flbl); free(fph); free(fdef);                    // dialog copied what it needs
            }
        }
        value_decref(fv);                                                       // release the fields table
    }
    value_decref(fkey);                                                         // release the interned key

    // buttons
    Value bkey = intern(vm, "buttons"), bv;                                     // cached key + value slot
    if (table_get(t, bkey, &bv)) {                                              // optional "buttons" table
        if (IS_TABLE(bv)) {
            Table* bt = AS_TABLE(bv);                                           // the button list
            int n = bt->array_part ? bt->array_count : 0;                       // number of array entries
            for (int i = 0; i < n; i++) {
                Value item = bt->array_part[i];                                 // one button descriptor
                if (!IS_TABLE(item)) continue;                                  // skip non-tables
                Table* it = AS_TABLE(item);
                char* bid  = table_get_str_dup(vm, it, "id");                   // action id
                char* blbl = table_get_str_dup(vm, it, "label");                // button caption
                bool bprim = false;                                             // default: ghost button
                Value pk = intern(vm, "primary"), pv;                           // cached key + value slot
                if (table_get(it, pk, &pv)) {
                    if (IS_BOOL(pv)) bprim = AS_BOOL(pv);                       // honour an explicit flag
                    value_decref(pv);                                           // release the flag value
                }
                value_decref(pk);                                               // release the interned key
                dialog_add_button(ctx, bid ? bid : "", blbl ? blbl : "", bprim); // append the button
                free(bid); free(blbl);                                          // dialog copied what it needs
            }
        }
        value_decref(bv);                                                       // release the buttons table
    }
    value_decref(bkey);                                                         // release the interned key

    dialog_commit(ctx);                                                         // drop if empty, else publish
    return MAKE_NONE();                                                         // dialogs return no value
}

// builds a table widget descriptor from a column list and a row list
static Value ctor_table(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "table"));
    if (argc >= 1) tset(vm, t, "columns", argv[0]);      // column descriptors
    if (argc >= 2) tset(vm, t, "rows", argv[1]);         // row cell arrays
    if (argc >= 3 && IS_TABLE(argv[2])) {                // merge options fields
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

// builds a tree widget descriptor from a node array and options
static Value ctor_tree(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "tree"));
    if (argc >= 1) tset(vm, t, "nodes", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
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

// builds a tabs widget descriptor from a tabs array and options
static Value ctor_tabs(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);                                  // descriptor table for the tabs widget
    tset(vm, t, "__t", intern(vm, "tabs"));                      // "__t" discriminates the widget kind
    if (argc >= 1) tset(vm, t, "tabs", argv[0]);                 // array of tab descriptors
    if (argc >= 2 && IS_TABLE(argv[1])) {                        // merge options fields
        Table* opts = AS_TABLE(argv[1]);                         // the options table
        int nk; Value* keys = table_keys(opts, &nk);             // snapshot option keys
        for (int i = 0; i < nk; i++) {                           // one pass per option key
            Value vv;                                            // value slot
            if (table_get(opts, keys[i], &vv)) {                 // read the option value
                table_set(t, keys[i], vv);                       // copy it into the descriptor
                value_decref(vv);                                // release our local reference
            }
            value_decref(keys[i]);                               // release each key from the snapshot
        }
        free(keys);                                              // free the key snapshot array
    }
    return MAKE_TABLE(t);                                        // caller owns the descriptor
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

// builds a statusbar widget descriptor
Value ui_ctor_statusbar(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "statusbar"));
    if (argc >= 1 && IS_TABLE(argv[0])) {
        Table* opts = AS_TABLE(argv[0]);
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

// ui.notification(table) — show a desktop notification popup
static Value ctor_notification(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);
    if (argc < 1 || !IS_TABLE(argv[0])) return MAKE_NONE();

    Table* t = AS_TABLE(argv[0]);
    char* key     = table_get_str_dup(vm, t, "key");
    char* title   = table_get_str_dup(vm, t, "title");
    char* message = table_get_str_dup(vm, t, "message");
    double timeout = table_get_num(vm, t, "timeout", 4.0);
    bool show_close = table_get_bool(vm, t, "close", true);

    ui_notification_show(ctx,
                         key     ? key     : "",
                         title   ? title   : "",
                         message ? message : "",
                         timeout, show_close);

    free(key); free(title); free(message);
    return MAKE_NONE();
}

// ui.menubar(table) — set the window menu bar; non-table or missing clears it
static Value ctor_menubar(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                             // ensure the context exists
    if (argc >= 1 && IS_TABLE(argv[0])) {
        ui_menubar_set(ctx, argv[0]);                           // install the new bar
    } else {
        ui_menubar_free(ctx);                                   // clear any existing bar
        ctx->redraw_needed = true;
        ctx->full_repaint  = true;
    }
    return MAKE_NONE();                                         // builders return no value
}

// ui.context_menu(items?, x?, y?) — open at cursor / at (x, y); none or no arg closes
static Value ctor_context_menu(VM* vm, int argc, Value* argv) {
    UiContext* ctx = ui_ensure(vm);                                         // ensure the context exists
    if (argc < 1 || !IS_TABLE(argv[0])) {
        ui_context_menu_close(ctx);                                         // none or non-table: close
        return MAKE_NONE();
    }
    int px = ctx->input.x;                                                  // default: cursor position
    int py = ctx->input.y;
    if (argc >= 3 && IS_NUMBER(argv[1]) && IS_NUMBER(argv[2])) {
        px = (int)AS_NUMBER(argv[1]);                                       // explicit position wins
        py = (int)AS_NUMBER(argv[2]);
    }
    ui_context_menu_set(ctx, argv[0], px, py);                              // build and open
    return MAKE_NONE();                                                     // builders return no value
}

// ui.list(items, opts) — a plain vertical list of clickable text rows
static Value ctor_list(VM* vm, int argc, Value* argv) {
    if (argc < 1 || !IS_TABLE(argv[0])) return MAKE_NONE();   // items array required
    Table* items = AS_TABLE(argv[0]);

    // optional selected label from opts
    char* sel = NULL;
    if (argc >= 2 && IS_TABLE(argv[1])) {
        sel = table_get_str_dup(vm, AS_TABLE(argv[1]), "selected");
    }

    // vbox holding every row, zero gap so the rows sit flush
    Table* vbox_t = table_create(4);
    tset(vm, vbox_t, "__t", intern(vm, "vbox"));
    tset(vm, vbox_t, "gap", MAKE_NUMBER(0.0));
    tset(vm, vbox_t, "padding", MAKE_NUMBER(0.0));

    Table* vbox_children = table_create(4);
    int n_arr = items->array_part ? items->array_count : 0;
    for (int i = 0; i < n_arr; i++) {
        Value it = items->array_part[i];
        if (IS_NONE(it)) break;
        if (!IS_STRING(it)) continue;                         // strings only

        Table* row_t = table_create(4);
        tset(vm, row_t, "__t", intern(vm, "list_item"));
        tset(vm, row_t, "content", it);                       // label
        tset(vm, row_t, "key", it);                           // the label doubles as the key

        if (sel) {                                            // mark the chosen row
            StringObject* s = AS_STRING(it);
            int n = s->length;
            if ((int)strlen(sel) == n && memcmp(s->chars, sel, (size_t)n) == 0) {
                tset(vm, row_t, "checked", MAKE_BOOL(true));
            }
        }

        Value row_v = MAKE_TABLE(row_t);
        Value kidx  = MAKE_NUMBER((double)(i + 1));           // 1-based array slot
        table_set(vbox_children, kidx, row_v);
        value_decref(kidx);
        value_decref(row_v);
    }
    { Value cv = MAKE_TABLE(vbox_children);
      tset(vm, vbox_t, "children", cv); value_decref(cv); }

    // panel wrapping the vbox, giving the rounded outer border
    Table* panel_t = table_create(4);
    tset(vm, panel_t, "__t", intern(vm, "panel"));
    tset(vm, panel_t, "padding", MAKE_NUMBER(1.0));           // keep the 1px border visible
    tset(vm, panel_t, "justify", intern(vm, "center"));       // split the panel's extra
                                                              // frame evenly top and bottom
    { Value cv = MAKE_TABLE(vbox_t);
      tset(vm, panel_t, "children", cv); value_decref(cv); }
      
    free(sel);                                                // release the optional selection copy
    return MAKE_TABLE(panel_t);
}

// builds a number input widget descriptor
static Value ctor_number_input(VM* vm, int argc, Value* argv) {
    Table* t = table_create(8);
    tset(vm, t, "__t", intern(vm, "number_input"));
    if (argc >= 1) tset(vm, t, "value", argv[0]);
    if (argc >= 2 && IS_TABLE(argv[1])) {
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

// builds the ui.key table of named key / modifier constants
static Value ctor_key_table(VM* vm) {
    Table* t = table_create(32);                                        // room for every entry
    tset(vm, t, "esc",       MAKE_NUMBER((double)UI_KEY_ESCAPE));       // escape
    tset(vm, t, "enter",     MAKE_NUMBER((double)UI_KEY_ENTER));        // return / enter
    tset(vm, t, "tab",       MAKE_NUMBER((double)UI_KEY_TAB));          // tab
    tset(vm, t, "backspace", MAKE_NUMBER((double)UI_KEY_BACKSPACE));    // backspace
    tset(vm, t, "delete",    MAKE_NUMBER((double)UI_KEY_DELETE));       // forward delete
    tset(vm, t, "insert",    MAKE_NUMBER((double)UI_KEY_INSERT));       // insert
    tset(vm, t, "home",      MAKE_NUMBER((double)UI_KEY_HOME));         // home
    tset(vm, t, "end",       MAKE_NUMBER((double)UI_KEY_END));          // end
    tset(vm, t, "page_up",   MAKE_NUMBER((double)UI_KEY_PAGE_UP));      // page up
    tset(vm, t, "page_down", MAKE_NUMBER((double)UI_KEY_PAGE_DOWN));    // page down
    tset(vm, t, "left",      MAKE_NUMBER((double)UI_KEY_LEFT));         // left arrow
    tset(vm, t, "right",     MAKE_NUMBER((double)UI_KEY_RIGHT));        // right arrow
    tset(vm, t, "up",        MAKE_NUMBER((double)UI_KEY_UP));           // up arrow
    tset(vm, t, "down",      MAKE_NUMBER((double)UI_KEY_DOWN));         // down arrow
    tset(vm, t, "space",     MAKE_NUMBER((double)UI_KEY_SPACE));        // space
    tset(vm, t, "f1",        MAKE_NUMBER((double)UI_KEY_F1));           // function keys
    tset(vm, t, "f2",        MAKE_NUMBER((double)UI_KEY_F2));
    tset(vm, t, "f3",        MAKE_NUMBER((double)UI_KEY_F3));
    tset(vm, t, "f4",        MAKE_NUMBER((double)UI_KEY_F4));
    tset(vm, t, "f5",        MAKE_NUMBER((double)UI_KEY_F5));
    tset(vm, t, "f6",        MAKE_NUMBER((double)UI_KEY_F6));
    tset(vm, t, "f7",        MAKE_NUMBER((double)UI_KEY_F7));
    tset(vm, t, "f8",        MAKE_NUMBER((double)UI_KEY_F8));
    tset(vm, t, "f9",        MAKE_NUMBER((double)UI_KEY_F9));
    tset(vm, t, "f10",       MAKE_NUMBER((double)UI_KEY_F10));
    tset(vm, t, "f11",       MAKE_NUMBER((double)UI_KEY_F11));
    tset(vm, t, "f12",       MAKE_NUMBER((double)UI_KEY_F12));
    tset(vm, t, "shift",     MAKE_NUMBER((double)UI_MOD_SHIFT));        // modifier bitmasks
    tset(vm, t, "ctrl",      MAKE_NUMBER((double)UI_MOD_CTRL));
    tset(vm, t, "alt",       MAKE_NUMBER((double)UI_MOD_ALT));
    return MAKE_TABLE(t);                                               // caller owns the table
}

// dispatches ui.* builtin calls from the vm into the ui library
bool ui_call_builtin(VM* vm, const char* name, int argc, Value* argv, Value* result) {
    if (strcmp(name, "ui.key") == 0) { *result = ctor_key_table(vm); return true; }  // named key / mod table
    if (strcmp(name, "ui.init") == 0)          { *result = ui_impl_init(vm, argc, argv); return true; }         // open the native window
    if (strcmp(name, "ui.shutdown") == 0)      { *result = ui_impl_shutdown(vm, argc, argv); return true; }     // close the native window
    if (strcmp(name, "ui.render") == 0)        { *result = ui_impl_render(vm, argc, argv); return true; }       // set the current widget tree
    if (strcmp(name, "ui.next_event") == 0)    { *result = ui_impl_next_event(vm, argc, argv); return true; }   // block until the next event
    if (strcmp(name, "ui.run") == 0)           { *result = ui_impl_run(vm, argc, argv); return true; }          // batched message loop
    if (strcmp(name, "ui.set_title") == 0)     { *result = ui_impl_set_title(vm, argc, argv); return true; }    // change the window title
    if (strcmp(name, "ui.window_size") == 0)   { *result = ui_impl_window_size(vm, argc, argv); return true; }  // query the window size
    if (strcmp(name, "ui.open_file") == 0)     { *result = ui_impl_open_file(vm, argc, argv); return true; }    // native file picker
    if (strcmp(name, "ui.open_folder") == 0)   { *result = ui_impl_open_folder(vm, argc, argv); return true; }  // native folder picker
    if (strcmp(name, "ui.save_as") == 0)       { *result = ui_impl_save_as(vm, argc, argv); return true; }      // native save picker
    if (strcmp(name, "ui.fullscreen") == 0)    { *result = ui_impl_fullscreen(vm, argc, argv); return true; }   // toggle/set fullscreen
    if (strcmp(name, "ui.dialog") == 0)        { *result = ctor_dialog(vm, argc, argv); return true; }          // generic modal dialog
    if (strcmp(name, "ui.notification") == 0)  { *result = ctor_notification(vm, argc, argv); return true; }    // desktop notification
    if (strcmp(name, "ui.menubar") == 0)       { *result = ctor_menubar(vm, argc, argv); return true; }         // window menu bar
    if (strcmp(name, "ui.context_menu") == 0)  { *result = ctor_context_menu(vm, argc, argv); return true; }    // right-click menu
    if (strcmp(name, "ui.statusbar") == 0)     { *result = ui_ctor_statusbar(vm, argc, argv); return true; }    // bottom status bar
    if (strcmp(name, "ui.list") == 0)          { *result = ctor_list(vm, argc, argv); return true; }            // compositional list

    if (strcmp(name, "ui.text") == 0)     { *result = make_leaf(vm, "text", argc, argv); return true; }         // plain text block
    if (strcmp(name, "ui.heading") == 0)  { *result = make_leaf(vm, "heading", argc, argv); return true; }      // section heading
    if (strcmp(name, "ui.badge") == 0)    { *result = make_leaf(vm, "badge", argc, argv); return true; }        // small status pill
    if (strcmp(name, "ui.separator") == 0){ *result = make_leaf(vm, "separator", argc, argv); return true; }    // horizontal rule

    if (strcmp(name, "ui.button") == 0)   { *result = ctor_button(vm, argc, argv); return true; }               // push button
    if (strcmp(name, "ui.checkbox") == 0) { *result = ctor_checkbox(vm, argc, argv); return true; }             // boolean toggle
    if (strcmp(name, "ui.switch") == 0)   { *result = ctor_switch(vm, argc, argv); return true; }               // ios-style toggle
    if (strcmp(name, "ui.radio") == 0)    { *result = ctor_radio(vm, argc, argv); return true; }                // radio option
    if (strcmp(name, "ui.slider") == 0)   { *result = ctor_slider(vm, argc, argv); return true; }               // numeric slider
    if (strcmp(name, "ui.progress") == 0) { *result = ctor_progress(vm, argc, argv); return true; }             // progress bar
    if (strcmp(name, "ui.input") == 0){ *result = ctor_input(vm, argc, argv); return true; }                    // editable text field
    if (strcmp(name, "ui.number_input") == 0) { *result = ctor_number_input(vm, argc, argv); return true; }     // numeric input
    if (strcmp(name, "ui.text_area") == 0) { *result = ctor_text_area(vm, argc, argv); return true; }           // multi-line editor
    if (strcmp(name, "ui.combobox") == 0) { *result = ctor_combobox(vm, argc, argv); return true; }             // dropdown selector
    if (strcmp(name, "ui.table") == 0)    { *result = ctor_table(vm, argc, argv); return true; }                // columned table
    if (strcmp(name, "ui.tree") == 0)     { *result = ctor_tree(vm, argc, argv); return true; }                 // hierarchical tree
    if (strcmp(name, "ui.tabs") == 0)     { *result = ctor_tabs(vm, argc, argv); return true; }                 // tab strip
    if (strcmp(name, "ui.spacer") == 0)   { *result = ctor_spacer(vm, argc, argv); return true; }               // flexible gap

    if (strcmp(name, "ui.vbox") == 0)   { *result = ui_ctor_container(vm, argc, argv, "vbox"); return true; }    // vertical stack
    if (strcmp(name, "ui.hbox") == 0)   { *result = ui_ctor_container(vm, argc, argv, "hbox"); return true; }    // horizontal row
    if (strcmp(name, "ui.panel") == 0)  { *result = ui_ctor_container(vm, argc, argv, "panel"); return true; }   // framed panel
    if (strcmp(name, "ui.scroll") == 0) { *result = ui_ctor_container(vm, argc, argv, "scroll"); return true; }  // vertical scroller
    if (strcmp(name, "ui.hscroll") == 0){ *result = ui_ctor_container(vm, argc, argv, "hscroll"); return true; } // horizontal scroller

    return false;                                // not a ui builtin, let other modules try
}
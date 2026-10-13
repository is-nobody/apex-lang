// source/libraries/ui/core/ui_build.c
// Descriptor -> widget tree materialization for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_hash.h"
#include "ui_table.h"
#include "ui_widgets.h"
#include <string.h>

// length-exact comparison against a string literal, no allocation
static inline bool str_eq_n(const char* s, int n, const char* lit, int lit_n) {
    return n == lit_n && memcmp(s, lit, (size_t)n) == 0;
}
#define STR_EQ(s, n, lit) str_eq_n((s), (n), (lit), (int)(sizeof(lit) - 1))

// length-aware variant of kind_from_str
static WidgetKind kind_from_str_n(const char* s, int n) {
    if (STR_EQ(s,n,"text"))      return WK_TEXT;
    if (STR_EQ(s,n,"heading"))   return WK_HEADING;
    if (STR_EQ(s,n,"button"))    return WK_BUTTON;
    if (STR_EQ(s,n,"checkbox"))  return WK_CHECKBOX;
    if (STR_EQ(s,n,"radio"))     return WK_RADIO;
    if (STR_EQ(s,n,"slider"))    return WK_SLIDER;
    if (STR_EQ(s,n,"progress"))  return WK_PROGRESS;
    if (STR_EQ(s,n,"input"))     return WK_INPUT;
    if (STR_EQ(s,n,"badge"))     return WK_BADGE;
    if (STR_EQ(s,n,"combobox"))  return WK_COMBOBOX;
    if (STR_EQ(s,n,"separator")) return WK_SEPARATOR;
    if (STR_EQ(s,n,"spacer"))    return WK_SPACER;
    if (STR_EQ(s,n,"vbox"))      return WK_VBOX;
    if (STR_EQ(s,n,"hbox"))      return WK_HBOX;
    if (STR_EQ(s,n,"panel"))     return WK_PANEL;
    if (STR_EQ(s,n,"scroll"))    return WK_SCROLL;
    if (STR_EQ(s,n,"hscroll"))   return WK_HSCROLL;
    return WK_TEXT;
}

// length-aware variant of parse_align
static Align parse_align_n(const char* s, int n) {
    if (STR_EQ(s,n,"start"))   return AL_START;
    if (STR_EQ(s,n,"center"))  return AL_CENTER;
    if (STR_EQ(s,n,"end"))     return AL_END;
    if (STR_EQ(s,n,"stretch")) return AL_STRETCH;
    return AL_STRETCH;
}

// length-aware variant of parse_justify
static Justify parse_justify_n(const char* s, int n) {
    if (STR_EQ(s,n,"center"))        return JU_CENTER;
    if (STR_EQ(s,n,"end"))           return JU_END;
    if (STR_EQ(s,n,"space-between")) return JU_SPACE_BETWEEN;
    if (STR_EQ(s,n,"space-around"))  return JU_SPACE_AROUND;
    if (STR_EQ(s,n,"space-evenly"))  return JU_SPACE_EVENLY;
    return JU_START;
}

// copies n bytes of a c string into the context's per-frame arena
static Str arena_dup(UiContext* ctx, const char* s, int n) {
    char* p = (char*)ui_arena_alloc(ctx, (size_t)n + 1);    // payload plus null terminator
    memcpy(p, s, n);                                     // copy the payload
    p[n] = 0;                                            // null terminate
    Str r; r.ptr = p; r.len = n;                         // return by value
    return r;
}

// recursively builds a Widget subtree from an apex table descriptor
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx) {
    if (!IS_TABLE(descr)) return NULL;                   // descriptors must be tables
    Table* t = AS_TABLE(descr);

    StrView tv;
    if (!table_get_str_view(t, ctx->key_cache[UK_T], &tv)) return NULL;  // missing kind: skip

    Widget* w = ui_widget_alloc(ctx);                    // allocate the widget slot
    w->kind = kind_from_str_n(tv.ptr, tv.len);           // resolve the kind enum

    StrView kv;
    uint64_t parent_id = parent ? parent->id : 0;        // root widgets hash from 0
    if (table_get_str_view(t, ctx->key_cache[UK_KEY], &kv)) {
        w->id = ui_hash_id_view(parent_id, &kv, idx);    // derive stable id
        w->key_str = arena_dup(ctx, kv.ptr, kv.len).ptr; // arena copy of the key
    } else {
        w->id = ui_hash_id(parent_id, NULL, idx);        // no key: positional fallback
    }

    StrView sv;
    if (table_get_str_view(t, ctx->key_cache[UK_ALIGN], &sv))
        w->align = parse_align_n(sv.ptr, sv.len);
    if (table_get_str_view(t, ctx->key_cache[UK_JUSTIFY], &sv))
        w->justify = parse_justify_n(sv.ptr, sv.len);

    w->gap      = (float)table_get_num_key(t, ctx->key_cache[UK_GAP], 8);           // child spacing
    w->padding  = (float)table_get_num_key(t, ctx->key_cache[UK_PADDING], 0);       // inner padding
    w->weight   = (float)table_get_num_key(t, ctx->key_cache[UK_WEIGHT], 0);        // flex weight
    w->min_w    = (float)table_get_num_key(t, ctx->key_cache[UK_MIN_WIDTH], 0);     // minimum width
    w->max_w    = (float)table_get_num_key(t, ctx->key_cache[UK_MAX_WIDTH], 0);     // maximum width
    w->disabled = table_get_bool_key(t, ctx->key_cache[UK_DISABLED], false);        // disabled flag
    w->visible  = table_get_bool_key(t, ctx->key_cache[UK_VISIBLE], true);          // visibility flag
    w->on_top   = table_get_bool_key(t, ctx->key_cache[UK_ON_TOP], false);          // draw after siblings

    Value wv;
    if (table_get(t, ctx->key_cache[UK_WIDTH], &wv)) {   // explicit width override
        if (IS_NUMBER(wv)) w->req_w = (float)AS_NUMBER(wv);
        value_decref(wv);
    }
    if (table_get(t, ctx->key_cache[UK_HEIGHT], &wv)) {  // explicit height override
        if (IS_NUMBER(wv)) w->req_h = (float)AS_NUMBER(wv);
        value_decref(wv);
    }

    StrView txt;
    bool has_txt = table_get_str_view(t, ctx->key_cache[UK_CONTENT], &txt);  // "content" preferred
    if (!has_txt) has_txt = table_get_str_view(t, ctx->key_cache[UK_LABEL], &txt);  // "label" fallback
    if (has_txt) w->text = arena_dup(ctx, txt.ptr, txt.len);

    w->checked = table_get_bool_key(t, ctx->key_cache[UK_CHECKED], false);  // checkbox/radio state

    StrView grp;
    if (table_get_str_view(t, ctx->key_cache[UK_GROUP], &grp))              // radio group name
        w->group = arena_dup(ctx, grp.ptr, grp.len);

    w->num_value = table_get_num_key(t, ctx->key_cache[UK_VALUE], 0);       // slider/progress value
    w->num_min   = table_get_num_key(t, ctx->key_cache[UK_MIN], 0);         // slider/progress min
    w->num_max   = table_get_num_key(t, ctx->key_cache[UK_MAX], 100);       // slider/progress max

    WidgetState* st = ui_state_get(ctx, w->id);             // pull persistent state

    if (w->kind == WK_COMBOBOX) {                           // combobox reads a string value + option list
        StrView vval;
        bool has_vval = table_get_str_view(t, ctx->key_cache[UK_VALUE], &vval);
        if (has_vval) w->combo_value = arena_dup(ctx, vval.ptr, vval.len);

        Value optv;
        if (table_get(t, ctx->key_cache[UK_OPTIONS], &optv)) {
            if (IS_TABLE(optv)) {
                Table* ot = AS_TABLE(optv);

                int n_arr = ot->array_part ? ot->array_count : 0;
                int n_opts = 0;
                for (int i = 0; i < n_arr; i++) {           // array part first
                    if (IS_NONE(ot->array_part[i])) break;
                    n_opts++;
                }
                for (int i = n_arr + 1; ; i++) {            // then numeric hash entries
                    Value ov;
                    if (!table_get(ot, MAKE_NUMBER((double)i), &ov)) break;
                    value_decref(ov);
                    n_opts++;
                }

                w->combo_options = (Str*)ui_arena_alloc(ctx, sizeof(Str) * (size_t)(n_opts ? n_opts : 1));
                w->combo_option_count = 0;

                for (int i = 0; i < n_arr; i++) {           // copy each option into the arena
                    Value ov = ot->array_part[i];
                    if (IS_NONE(ov)) break;
                    if (IS_STRING(ov)) {
                        StringObject* s = AS_STRING(ov);
                        w->combo_options[w->combo_option_count++] = arena_dup(ctx, s->chars, s->length);
                    }
                }
                for (int i = n_arr + 1; ; i++) {
                    Value ov;
                    if (!table_get(ot, MAKE_NUMBER((double)i), &ov)) break;
                    if (IS_STRING(ov)) {
                        StringObject* s = AS_STRING(ov);
                        w->combo_options[w->combo_option_count++] = arena_dup(ctx, s->chars, s->length);
                    }
                    value_decref(ov);
                }
            }
            value_decref(optv);
        }

        w->combo_selected = -1;                             // resolve which option matches value
        if (has_vval) {
            for (int i = 0; i < w->combo_option_count; i++) {
                if (w->combo_options[i].len == vval.len &&
                    memcmp(w->combo_options[i].ptr, vval.ptr, (size_t)vval.len) == 0) {
                    w->combo_selected = i;
                    break;
                }
            }
        }
        w->combo_open = st->combo_open;                     // restore persisted open state
    }

    if (w->kind == WK_INPUT) {                              // input widgets own text in the state
        StrView ival;
        bool has_ival = table_get_str_view(t, ctx->key_cache[UK_VALUE_TEXT], &ival);
        if (!has_ival)
            has_ival = table_get_str_view(t, ctx->key_cache[UK_CONTENT], &ival);
        const char* init_s = has_ival ? ival.ptr : "";
        int         init_n = has_ival ? ival.len : 0;
        sync_input_buffer(w, st, init_s, init_n);
    }

    w->hovered = st->hovered;                               // restore hover flag
    w->pressed = st->pressed;                               // restore press flag
    w->focused = (ctx->focused_id == w->id);                // reflect current focus
    w->cursor  = st->cursor < 0 ? (int)w->input_value.len : st->cursor;
    w->sel_anchor = st->sel_anchor;                         // restore the selection anchor
    w->scroll_y = st->scroll_y;                             // restore scroll offset

    Value cv;
    if (table_get(t, ctx->key_cache[UK_CHILDREN], &cv)) {   // child descriptor present?
        if (IS_TABLE(cv)) {
            Table* ct = AS_TABLE(cv);

            // fast path: walk the array part directly
            int i = 1;
            int n_arr = ct->array_part ? ct->array_count : 0;
            for (; i <= n_arr; i++) {
                Value vv = ct->array_part[i - 1];
                if (IS_NONE(vv)) break;      // hole: fall through to the hash walk below
                Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                if (ch) widget_add_child(ctx, w, ch);
            }

            // slow path: indices past array_count, or holes inside it that
            // might still resolve to a numeric hash entry
            for (;; i++) {
                Value vv;
                if (!table_get(ct, MAKE_NUMBER((double)i), &vv)) break;
                Widget* ch = ui_build_tree(ctx, vv, w, i - 1);
                if (ch) widget_add_child(ctx, w, ch);
                value_decref(vv);
            }
        } else if (!IS_NONE(cv)) {                          // single child descriptor
            Widget* ch = ui_build_tree(ctx, cv, w, 0);
            if (ch) widget_add_child(ctx, w, ch);
        }
        value_decref(cv);
    }

    return w;                                               // return the built subtree
}

// mixes a descriptor Value into the running hash
static uint64_t hash_descr_value(UiContext* ctx, uint64_t h, Value v);

// recursive hash of a descriptor tree
static uint64_t hash_descr_table(UiContext* ctx, uint64_t h, Table* t) {
    // hash array-style numeric keys 1..n first
    int j = 1;
    int n_arr = t->array_part ? t->array_count : 0;
    for (; j <= n_arr; j++) {
        Value v = t->array_part[j - 1];
        if (IS_NONE(v)) break;
        uint64_t idx = (uint64_t)j;
        h = hash_bytes64(h, &idx, sizeof(idx));
        h = hash_descr_value(ctx, h, v);
    }
    for (;; j++) {
        Value v;
        if (!table_get(t, MAKE_NUMBER((double)j), &v)) break;
        uint64_t idx = (uint64_t)j;
        h = hash_bytes64(h, &idx, sizeof(idx));
        h = hash_descr_value(ctx, h, v);
        value_decref(v);
    }

    for (int i = 0; i < UK_COUNT; i++) {
        Value v;
        if (!table_get(t, ctx->key_cache[i], &v)) continue;
        h = hash_bytes64(h, &i, sizeof(i));
        if (i == UK_CHILDREN && IS_TABLE(v)) {
            Table* ct = AS_TABLE(v);
            int count = 0;

            // fast path: direct array walk, same trick as the other two callers.
            int jj = 1;
            int n_arr2 = ct->array_part ? ct->array_count : 0;
            for (; jj <= n_arr2; jj++) {
                Value cv = ct->array_part[jj - 1];
                if (IS_NONE(cv)) break;
                h = hash_descr_value(ctx, h, cv);
                count++;
            }
            for (;; jj++) {
                Value cv;
                if (!table_get(ct, MAKE_NUMBER((double)jj), &cv)) break;
                h = hash_descr_value(ctx, h, cv);
                value_decref(cv);
                count++;
            }

            if (count == 0) {
                // no iterable children
                h = hash_descr_value(ctx, h, v);
                count = 1;
            }
            h = hash_bytes64(h, &count, sizeof(count));
        } else {
            h = hash_descr_value(ctx, h, v);
        }
        value_decref(v);
    }
    return h;
}

static uint64_t hash_descr_value(UiContext* ctx, uint64_t h, Value v) {
    if (IS_TABLE(v))       return hash_descr_table(ctx, h, AS_TABLE(v));
    if (IS_STRING(v)) {
        StringObject* s = AS_STRING(v);
        return hash_str_sampled(h, s->chars, s->length);
    }
    if (IS_NUMBER(v)) { double d = AS_NUMBER(v); return hash_bytes64(h, &d, sizeof(d)); }
    if (IS_BOOL(v))   { bool b = AS_BOOL(v);    return hash_bytes64(h, &b, sizeof(b)); }
    return hash_bytes64(h, &v, sizeof(v));                  // none, function, etc.
}

// public wrapper: hash a whole descriptor Value
uint64_t hash_descr_recursive(UiContext* ctx, Value descr) {
    return hash_descr_value(ctx, 0xCBF29CE484222325ULL, descr);
}
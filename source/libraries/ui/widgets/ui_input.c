// source/libraries/ui/widgets/ui_input.c
// Input widget editing for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_hash.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>

// syncs a widget's input view against the descriptor's value_text
void sync_input_buffer(Widget* w, WidgetState* s, const char* sval, int slen) {
    uint64_t h = hash_bytes64(0xCBF29CE484222325ULL, sval, (size_t)slen);
    if (!s->input_buf || s->input_hash != h) {           // new widget, or script changed text
        free(s->input_buf);                              // release the previous buffer
        s->input_buf = (char*)malloc((size_t)slen + 1);
        memcpy(s->input_buf, sval, (size_t)slen);
        s->input_buf[slen] = 0;
        s->input_len  = slen;
        s->input_hash = h;
    }
    w->input_value.ptr = s->input_buf;                   // publish the view on the widget
    w->input_value.len = s->input_len;
}

// true when the state has a live non-empty selection
bool input_has_selection(WidgetState* s) {
    return s->sel_anchor >= 0 && s->cursor >= 0 && s->sel_anchor != s->cursor;
}

// returns the [min, max) byte range covered by the selection
void input_sel_range(WidgetState* s, int* out_min, int* out_max) {
    int a = s->sel_anchor >= 0 ? s->sel_anchor : 0;
    int b = s->cursor >= 0 ? s->cursor : 0;
    if (a > b) { int t = a; a = b; b = t; }
    *out_min = a;
    *out_max = b;
}

// maps a local x offset inside an input widget to a byte offset in its text
int input_hit_offset(UiContext* ctx, Widget* w, float local_x) {
    const char* s = w->input_value.ptr;
    int n = w->input_value.len;
    if (!ctx->font || n == 0) return 0;
    if (local_x <= 0) return 0;

    // walk the text one codepoint at a time, measuring each codepoint's
    float prev = 0;
    int i = 0;
    while (i < n) {
        int next = ui_utf8_next(s, i, n);
        float adv = ui_text_measure_cached(ctx, s + i, next - i, UI_FS_BODY);
        float w2 = prev + adv;
        float mid = (prev + w2) * 0.5f;
        if (local_x < mid) return i;
        i = next;
        prev = w2;
    }
    return n;
}

// finds the [start, end) byte range of the word containing pos
void input_word_range(Widget* w, int pos, int* out_start, int* out_end) {
    const char* s = w->input_value.ptr;
    int n = w->input_value.len;
    int a = pos;
    while (a > 0) {
        int prev = ui_utf8_prev(s, a);
        unsigned char c = (unsigned char)s[prev];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        a = prev;
    }
    int b = pos;
    while (b < n) {
        unsigned char c = (unsigned char)s[b];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') break;
        b = ui_utf8_next(s, b, n);
    }
    *out_start = a;
    *out_end = b;
}

// replaces the input buffer with a new heap copy and frees the old one
void input_set_buffer(UiContext* ctx, Widget* w, char* buf, int len) {
    WidgetState* s = ui_state_get(ctx, w->id);
    free(s->input_buf);                                     // release the previous buffer
    s->input_buf = buf;
    s->input_len = len;
    w->input_value.ptr = buf;                               // refresh the widget view
    w->input_value.len = len;
}

// pushes a snapshot of the current buffer onto the undo stack
#define INPUT_UNDO_MAX 128
void input_push_undo(WidgetState* s, const char* text, int len, int cursor) {
    if (s->undo_count >= INPUT_UNDO_MAX) {                 // drop the oldest snapshot
        free(s->undo_stack[0].text);
        memmove(&s->undo_stack[0], &s->undo_stack[1],
                sizeof(UndoEntry) * (s->undo_count - 1));
        s->undo_count--;
    }
    if (s->undo_count >= s->undo_cap) {                    // grow the undo stack
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (UndoEntry*)realloc(s->undo_stack,
                                            sizeof(UndoEntry) * s->undo_cap);
    }
    char* copy = (char*)malloc((size_t)len + 1);           // snapshot the text
    memcpy(copy, text, len);
    copy[len] = 0;
    s->undo_stack[s->undo_count].text   = copy;
    s->undo_stack[s->undo_count].len    = len;
    s->undo_stack[s->undo_count].cursor = cursor;
    s->undo_count++;

    for (int i = 0; i < s->redo_count; i++) free(s->redo_stack[i].text);  // invalidate redo
    s->redo_count = 0;
}

// steps the buffer back one snapshot, moving the current state onto redo
void input_undo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->undo_count == 0) return;                        // nothing to undo
    if (s->redo_count >= s->redo_cap) {                    // grow redo
        s->redo_cap = s->redo_cap ? s->redo_cap * 2 : 16;
        s->redo_stack = (UndoEntry*)realloc(s->redo_stack,
                                            sizeof(UndoEntry) * s->redo_cap);
    }
    char* cur = (char*)malloc((size_t)w->input_value.len + 1);  // snapshot current
    memcpy(cur, w->input_value.ptr, w->input_value.len);
    cur[w->input_value.len] = 0;
    s->redo_stack[s->redo_count].text   = cur;
    s->redo_stack[s->redo_count].len    = w->input_value.len;
    s->redo_stack[s->redo_count].cursor = s->cursor;
    s->redo_count++;

    UndoEntry e = s->undo_stack[--s->undo_count];          // pop the previous state
    free(s->input_buf);                                    // state owns the buffer
    s->input_buf = e.text;
    s->input_len = e.len;
    w->input_value.ptr = e.text;                           // refresh the widget view
    w->input_value.len = e.len;
    s->cursor = e.cursor;
    s->sel_anchor = e.cursor;
    push_input_change(ctx, w, e.text);
    ctx->redraw_needed = true;
}

// steps the buffer forward one snapshot, moving the current state back to undo
void input_redo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->redo_count == 0) return;                        // nothing to redo
    if (s->undo_count >= s->undo_cap) {                    // grow undo
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (UndoEntry*)realloc(s->undo_stack,
                                            sizeof(UndoEntry) * s->undo_cap);
    }
    char* cur = (char*)malloc((size_t)w->input_value.len + 1);  // snapshot current
    memcpy(cur, w->input_value.ptr, w->input_value.len);
    cur[w->input_value.len] = 0;
    s->undo_stack[s->undo_count].text   = cur;
    s->undo_stack[s->undo_count].len    = w->input_value.len;
    s->undo_stack[s->undo_count].cursor = s->cursor;
    s->undo_count++;

    UndoEntry e = s->redo_stack[--s->redo_count];          // pop the forward state
    free(s->input_buf);                                    // state owns the buffer
    s->input_buf = e.text;
    s->input_len = e.len;
    w->input_value.ptr = e.text;                           // refresh the widget view
    w->input_value.len = e.len;
    s->cursor = e.cursor;
    s->sel_anchor = e.cursor;
    push_input_change(ctx, w, e.text);
    ctx->redraw_needed = true;
}

// deletes [a, b) from the input buffer, moving the caret to a
void input_delete_range(UiContext* ctx, Widget* w, WidgetState* s, int a, int b) {
    int len = (int)w->input_value.len;
    if (a < 0) a = 0;
    if (b > len) b = len;
    if (b <= a) return;                                   // nothing to remove
    int new_len = len - (b - a);
    char* nb = (char*)malloc((size_t)new_len + 1);
    memcpy(nb, w->input_value.ptr, a);                    // prefix
    memcpy(nb + a, w->input_value.ptr + b, len - b);      // suffix
    nb[new_len] = 0;
    input_set_buffer(ctx, w, nb, new_len);
    s = ui_state_get(ctx, w->id); // re-fetch the valid pointer
    s->cursor = a;
    s->sel_anchor = a;
    push_input_change(ctx, w, nb);
    ctx->redraw_needed = true;
}

// emits a change event for an input widget, copying the new string
void push_input_change(UiContext* ctx, Widget* w, const char* str_val) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));                       // zero the event
    ev.kind = EV_CHANGE;                                          // change kind
    ev.widget_id = w->id;                                         // source widget id
    strncpy(ev.widget_type, "input", 15);                         // widget type name
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);         // user key if present
    ev.str_value = (char*)malloc(strlen(str_val) + 1);            // own a copy of the new text
    memcpy(ev.str_value, str_val, strlen(str_val) + 1);
    ui_event_push(ctx, &ev);                                      // enqueue the change
    ctx->geometry_dirty = true;                                   // input width may have changed
}
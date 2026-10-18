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

// rebuilds the line-start cache from scratch (O(N), runs once per edit)
void ta_line_cache_ensure(WidgetState* s, const char* buf, int len) {
    if (s->line_cache_valid) return;

    // count lines: at least one (empty buffer still has line 0)
    int count = 1;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') count++;
    }
    // grow capacity if needed (×2 headroom to amortize)
    if (count > s->line_cap) {
        int new_cap = s->line_cap > 0 ? s->line_cap * 2 : 64;
        while (new_cap < count) new_cap *= 2;
        int* nb = (int*)realloc(s->line_starts, (size_t)new_cap * sizeof(int));
        if (!nb) return;                    // OOM: cache stays invalid
        s->line_starts = nb;
        s->line_cap    = new_cap;
    }
    // populate: line 0 starts at byte 0; each '\n' starts a new line at i+1
    int idx = 0;
    s->line_starts[idx++] = 0;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') s->line_starts[idx++] = i + 1;
    }
    s->line_count       = idx;
    s->line_cache_valid = true;
}

// finds the largest line_starts[i] <= byte_pos (binary search)
int ta_line_cache_find(const WidgetState* s, int byte_pos) {
    if (s->line_count <= 1) return 0;
    if (byte_pos < 0) byte_pos = 0;
    int lo = 0, hi = s->line_count - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (s->line_starts[mid] <= byte_pos) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

// grows line_starts capacity if the new count would overflow it
static int line_cache_reserve(WidgetState* s, int want) {
    if (want <= s->line_cap) return 1;
    int new_cap = s->line_cap > 0 ? s->line_cap * 2 : 64;
    while (new_cap < want) new_cap *= 2;
    int* nb = (int*)realloc(s->line_starts, (size_t)new_cap * sizeof(int));
    if (!nb) return 0;
    s->line_starts = nb;
    s->line_cap    = new_cap;
    return 1;
}

// adjusts the line-start cache for [offset, offset+len) being replaced by the text at `text`
static void line_cache_splice(WidgetState* s, int offset,
                              const char* text, int len,
                              int removed, int removed_newlines) {
    if (!s->line_cache_valid) return;                      // cold cache: rebuild on demand

    int line_idx = ta_line_cache_find(s, offset);

    int newlines = 0;
    for (int i = 0; i < len; i++) if (text[i] == '\n') newlines++;

    if (newlines == 0 && removed_newlines == 0) {
        // no line boundaries touched: just shift the surviving tail
        int delta = len - removed;
        for (int i = line_idx + 1; i < s->line_count; i++)
            s->line_starts[i] += delta;
        return;
    }

    // drop the entries for any '\n' that was inside the removed range.
    // those are exactly the entries immediately after line_idx.
    if (removed_newlines > 0) {
        int lo = line_idx + 1;
        int to_remove = removed_newlines;
        if (lo + to_remove > s->line_count) to_remove = s->line_count - lo;
        if (to_remove > 0) {
            memmove(&s->line_starts[lo],
                    &s->line_starts[lo + to_remove],
                    (size_t)(s->line_count - lo - to_remove) * sizeof(int));
            s->line_count -= to_remove;
        }
    }

    // shift every surviving tail entry by the net byte delta
    int delta = len - removed;
    for (int i = line_idx + 1; i < s->line_count; i++)
        s->line_starts[i] += delta;

    // splice in the new entries for '\n's in the inserted text
    if (newlines > 0) {
        if (!line_cache_reserve(s, s->line_count + newlines)) {
            s->line_cache_valid = false;
            return;
        }
        int tail = s->line_count - (line_idx + 1);
        memmove(&s->line_starts[line_idx + 1 + newlines],
                &s->line_starts[line_idx + 1],
                (size_t)tail * sizeof(int));
        int insert_idx = line_idx + 1;
        int cur = offset;
        for (int i = 0; i < len; i++) {
            cur++;
            if (text[i] == '\n') s->line_starts[insert_idx++] = cur;
        }
        s->line_count += newlines;
    }
}

int ta_gutter_width(UiContext* ctx, Widget* w, int pad_x) {
    if (!w->gutter) return 0;
    WidgetState* s = ui_state_get(ctx, w->id);
    ta_line_cache_ensure(s, w->input_value.ptr, w->input_value.len);
    int line_count = s->line_count > 0 ? s->line_count : 1;
    int digits = 1;
    while (line_count >= 10) { digits++; line_count /= 10; }
    char buf[32];
    for (int i = 0; i < digits; i++) buf[i] = '0';
    buf[digits] = 0;
    float ww = ui_text_measure_cached(ctx, buf, digits, UI_FS_SMALL);
    return (int)(ww + 0.5f) + pad_x;
}

// syncs a widget's input view against the descriptor's value_text
void sync_input_buffer(Widget* w, WidgetState* s, const char* sval, int slen) {
    uint64_t h_desc = hash_str_sampled(0xCBF29CE484222325ULL, sval, slen);

    // descriptor matches the buffer: already in sync, remember the descriptor hash
    if (h_desc == s->input_hash) {
        s->desc_hash = h_desc;
        w->input_value.ptr = s->input_buf;
        w->input_value.len = s->input_len;
        return;
    }

    // descriptor has not changed since the last sync (user edit beat the
    // script to it): keep the buffer, do not copy the stale descriptor back
    if (h_desc == s->desc_hash && s->input_buf) {
        w->input_value.ptr = s->input_buf;
        w->input_value.len = s->input_len;
        return;
    }

    // descriptor changed programmatically: adopt it
    int cap = slen + 64;
    free(s->input_buf);
    s->input_buf = (char*)malloc((size_t)cap);
    memcpy(s->input_buf, sval, (size_t)slen);
    s->input_buf[slen] = 0;
    s->input_len  = slen;
    s->input_cap  = cap;
    s->input_hash = h_desc;
    s->desc_hash  = h_desc;
    s->line_cache_valid = false;
    w->input_value.ptr = s->input_buf;
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
    free(s->input_buf);
    s->input_buf = buf;
    s->input_len = len;
    s->input_cap = len + 1;                                 // buf was malloc'd at len + 1
    s->input_hash = hash_str_sampled(0xCBF29CE484222325ULL, buf, len);
    s->desc_hash  = s->input_hash;
    s->line_cache_valid = false;                            // buffer changed: rebuild lazily
    w->input_value.ptr = buf;
    w->input_value.len = len;
}

// cap on the number of diffs held in the undo stack
#define INPUT_UNDO_MAX 128

// frees one diff entry's owned buffers
static void diff_free(DiffEntry* e) {
    free(e->removed);
    free(e->inserted);
    e->removed = NULL;
    e->inserted = NULL;
}

// pushes a new diff onto the undo stack and clears the redo stack
static void record_diff(WidgetState* s, int offset,
                        const char* removed_ptr, int removed_len,
                        const char* inserted_ptr, int inserted_len) {
    while (s->undo_count >= INPUT_UNDO_MAX) {              // drop oldest
        diff_free(&s->undo_stack[0]);
        memmove(&s->undo_stack[0], &s->undo_stack[1],
                sizeof(DiffEntry) * (s->undo_count - 1));
        s->undo_count--;
    }
    if (s->undo_count >= s->undo_cap) {
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (DiffEntry*)realloc(s->undo_stack,
                                            sizeof(DiffEntry) * s->undo_cap);
    }
    DiffEntry* e = &s->undo_stack[s->undo_count++];
    e->offset        = offset;
    e->n_removed     = removed_len;
    e->n_inserted    = inserted_len;
    e->removed       = NULL;
    e->inserted      = NULL;
    if (removed_len > 0) {
        e->removed = (char*)malloc((size_t)removed_len);
        memcpy(e->removed, removed_ptr, (size_t)removed_len);
    }
    if (inserted_len > 0) {
        e->inserted = (char*)malloc((size_t)inserted_len);
        memcpy(e->inserted, inserted_ptr, (size_t)inserted_len);
    }
    e->cursor_before = s->cursor < 0 ? s->input_len : s->cursor;
    e->cursor_after  = offset + inserted_len;

    for (int i = 0; i < s->redo_count; i++) diff_free(&s->redo_stack[i]);
    s->redo_count = 0;
}

// buffer + hash + line-cache splice; no cursor, no event, no undo record
static void apply_splice(WidgetState* s, int a, int b,
                         const char* text, int len) {
    int old_len = s->input_len;
    int removed = b - a;
    if (removed < 0) removed = 0;
    int new_len = old_len - removed + len;
    if (new_len < 0) new_len = 0;

    // count '\n' inside the range being removed, BEFORE the buffer is mutated
    int removed_newlines = 0;
    for (int i = a; i < b; i++)
        if (s->input_buf[i] == '\n') removed_newlines++;

    if (s->input_cap < new_len + 1) {
        int new_cap = s->input_cap ? s->input_cap * 2 : 64;
        while (new_cap < new_len + 1) new_cap *= 2;
        s->input_buf = (char*)realloc(s->input_buf, (size_t)new_cap);
        s->input_cap = new_cap;
    }

    if (len != removed) {
        memmove(s->input_buf + a + len, s->input_buf + b,
                (size_t)(old_len - b + 1));
    }
    if (len > 0) {
        memcpy(s->input_buf + a, text, (size_t)len);
    }

    s->input_len  = new_len;
    s->input_hash = hash_str_sampled(0xCBF29CE484222325ULL, s->input_buf, new_len);
    // desc_hash is intentionally not updated here
    line_cache_splice(s, a, text ? text : "", len, removed, removed_newlines);
}

// steps the buffer back one diff, moving it onto redo
void input_undo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->undo_count == 0) return;
    DiffEntry e = s->undo_stack[--s->undo_count];

    // undo: replace the inserted region with the removed bytes
    apply_splice(s, e.offset, e.offset + e.n_inserted, e.removed, e.n_removed);

    if (s->redo_count >= s->redo_cap) {
        s->redo_cap = s->redo_cap ? s->redo_cap * 2 : 16;
        s->redo_stack = (DiffEntry*)realloc(s->redo_stack,
                                            sizeof(DiffEntry) * s->redo_cap);
    }
    s->redo_stack[s->redo_count++] = e;                    // ownership transfers to redo

    w->input_value.ptr = s->input_buf;
    w->input_value.len = s->input_len;
    s->cursor = e.cursor_before;
    if (s->cursor < 0 || s->cursor > s->input_len) s->cursor = s->input_len;
    s->sel_anchor = s->cursor;
    push_input_change_ex(ctx, w, true);
    ctx->redraw_needed = true;
}

// steps the buffer forward one diff, moving it back onto undo
void input_redo(UiContext* ctx, Widget* w, WidgetState* s) {
    if (s->redo_count == 0) return;
    DiffEntry e = s->redo_stack[--s->redo_count];

    // redo: replace the removed region with the inserted bytes
    apply_splice(s, e.offset, e.offset + e.n_removed, e.inserted, e.n_inserted);

    if (s->undo_count >= s->undo_cap) {
        s->undo_cap = s->undo_cap ? s->undo_cap * 2 : 16;
        s->undo_stack = (DiffEntry*)realloc(s->undo_stack,
                                            sizeof(DiffEntry) * s->undo_cap);
    }
    s->undo_stack[s->undo_count++] = e;                    // ownership transfers to undo

    w->input_value.ptr = s->input_buf;
    w->input_value.len = s->input_len;
    s->cursor = e.cursor_after;
    if (s->cursor < 0 || s->cursor > s->input_len) s->cursor = s->input_len;
    s->sel_anchor = s->cursor;
    push_input_change_ex(ctx, w, true);
    ctx->redraw_needed = true;
}

// deletes [a, b) from the input buffer via input_splice_inplace
void input_delete_range(UiContext* ctx, Widget* w, WidgetState* s, int a, int b) {
    (void)s;
    input_splice_inplace(ctx, w, a, b, NULL, 0);
}

// in-place replace: overwrites [a, b) with text[len] without copying the whole buffer
void input_splice_inplace(UiContext* ctx, Widget* w, int a, int b,
                          const char* text, int len) {
    WidgetState* s = ui_state_get(ctx, w->id);
    int old_len = s->input_len;
    if (a < 0) a = 0;
    if (b > old_len) b = old_len;
    if (b < a) b = a;
    int removed = b - a;
    if (removed == 0 && len == 0) return;                  // no-op edit

    // record what is about to change BEFORE mutating the buffer
    record_diff(s, a,
                s->input_buf + a, removed,
                text, len);

    apply_splice(s, a, b, text, len);

    w->input_value.ptr = s->input_buf;
    w->input_value.len = s->input_len;
    s->cursor    = a + len;
    s->sel_anchor = s->cursor;
    push_input_change_ex(ctx, w, false);
    ctx->redraw_needed = true;
}

// emits a change event for an input widget
#define PUSH_INPUT_SKIP_PAYLOAD_BYTES (4 * 1024 * 1024)        // skip copy above 4 MB

void push_input_change_ex(UiContext* ctx, Widget* w, bool undo_redo) {
    UiEvent ev; memset(&ev, 0, sizeof(ev));
    ev.kind = EV_CHANGE;
    ev.widget_id = w->id;
    strncpy(ev.widget_type, (w->kind == WK_TEXT_AREA) ? "text_area" : "input", 15);
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);
    ev.str_value = NULL;
    ev.num_value = (double)w->input_value.len;
    if (undo_redo && (w->kind == WK_TEXT_AREA) &&
        w->input_value.len > PUSH_INPUT_SKIP_PAYLOAD_BYTES) {
        ev.payload_skipped = true;
    }
    ui_event_push(ctx, &ev);
    ctx->geometry_dirty = true;
}

// maps (local_x, local_y) inside a WK_TEXT_AREA to a byte offset in its text
int text_area_hit_offset(UiContext* ctx, Widget* w, float local_x, float local_y) {
    const char* buf = w->input_value.ptr;
    int len = w->input_value.len;
    int lh  = UI_LINE_H;
    double scroll = w->scroll_y;

    int target_line = (int)((local_y + scroll) / lh);
    if (target_line < 0) target_line = 0;

    WidgetState* s = ui_state_get(ctx, w->id);
    ta_line_cache_ensure(s, buf, len);
    if (target_line >= s->line_count) return len;

    int line_start = s->line_starts[target_line];
    int line_end   = (target_line + 1 < s->line_count)
                     ? s->line_starts[target_line + 1] - 1
                     : len;
    if (line_end < line_start) line_end = line_start;

    float prev = 0;
    int j = line_start;
    while (j < line_end) {
        int next = ui_utf8_next(buf, j, line_end);
        float adv = ui_text_measure_cached(ctx, buf + j, next - j, UI_FS_BODY);
        float w2  = prev + adv;
        if (local_x < (prev + w2) * 0.5f) return j;
        j = next;
        prev = w2;
    }
    return line_end;
}

// ensures the focused text_area's cursor is visible
void text_area_ensure_visible(UiContext* ctx, Widget* w) {
    if (!w || w->kind != WK_TEXT_AREA) return;
    if (!w->focused) return;

    WidgetState* s = ui_state_get(ctx, w->id);
    const char* buf = w->input_value.ptr;
    int len = w->input_value.len;
    int cur = s->cursor < 0 ? len : s->cursor;
    if (cur > len) cur = len;

    ta_line_cache_ensure(s, buf, len);
    int cursor_line = ta_line_cache_find(s, cur);

    int lh     = UI_LINE_H;
    int pad_y  = 6;
    int inner_h = (int)w->h - pad_y * 2;
    if (inner_h < lh) inner_h = lh;

    int content_h  = s->line_count * lh;
    int max_scroll = content_h - inner_h;
    if (max_scroll < 0) max_scroll = 0;

    double cy     = (double)cursor_line * lh;
    double scroll = s->scroll_y;
    double target = scroll;
    if (cy < scroll)                             target = cy;
    else if (cy + lh > scroll + inner_h)         target = cy + lh - inner_h;
    if (target < 0)           target = 0;
    if (target > max_scroll)  target = max_scroll;

    if (target != scroll) {
        s->scroll_y      = target;
        s->scroll_target = target;
        w->scroll_y      = target;
        ctx->redraw_needed = true;
    }
}

// moves the cursor one line up (dir=-1) or down (dir=+1); extend = shift held
void text_area_move_vertical(UiContext* ctx, Widget* w, WidgetState* s, int dir, bool extend) {
    const char* buf = w->input_value.ptr;
    int len = w->input_value.len;
    int cur = s->cursor < 0 ? len : s->cursor;
    if (cur > len) cur = len;

    ta_line_cache_ensure(s, buf, len);
    int line_idx   = ta_line_cache_find(s, cur);
    int line_start = s->line_starts[line_idx];
    int offset_in_line = cur - line_start;
    float x_target = ui_text_measure_cached(ctx, buf + line_start, offset_in_line, UI_FS_BODY);

    int target_line;
    if (dir > 0) {
        if (line_idx + 1 >= s->line_count) return;
        target_line = line_idx + 1;
    } else {
        if (line_idx == 0) return;
        target_line = line_idx - 1;
    }

    int target_start = s->line_starts[target_line];
    int target_end   = (target_line + 1 < s->line_count)
                       ? s->line_starts[target_line + 1] - 1
                       : len;
    if (target_end < target_start) target_end = target_start;

    float prev = 0;
    int j = target_start;
    int new_cur = target_end;
    while (j < target_end) {
        int next = ui_utf8_next(buf, j, target_end);
        float adv = ui_text_measure_cached(ctx, buf + j, next - j, UI_FS_BODY);
        float w2  = prev + adv;
        if (x_target < (prev + w2) * 0.5f) { new_cur = j; break; }
        j = next;
        prev = w2;
    }

    s->cursor = new_cur;
    if (!extend) s->sel_anchor = new_cur;
}
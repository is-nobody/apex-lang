// source/libraries/ui/widgets/ui_dialog.c
// Modal dialog construction, geometry and input handling for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_text.h"
#include "ui_window.h"
#include <string.h>
#include <stdlib.h>

// releases every heap string owned by one dialog field and zeroes it
static void dialog_field_free(UiDialogField* f) {
    free(f->key);
    free(f->label);
    free(f->placeholder);
    free(f->input);
    memset(f, 0, sizeof(*f));
}

// releases every heap allocation owned by the dialog and resets it
void dialog_clear(UiDialog* d) {
    free(d->key);
    free(d->title);
    free(d->message);
    for (int i = 0; i < d->field_count; i++) dialog_field_free(&d->fields[i]);  // release each field's strings
    free(d->fields);                                                            // release the field array
    for (int i = 0; i < d->button_count; i++) {
        free(d->buttons[i].id);                                                 // release the button id
        free(d->buttons[i].label);                                              // release the button label
    }
    free(d->buttons);                                                           // release the button array
    memset(d, 0, sizeof(*d));                                                   // wipe every field
    d->hover_btn     = -1;                                                      // no hovered button
    d->active_btn    = -1;                                                      // no pressed button
    d->focused_field = -1;                                                      // no focused field
    d->pending_btn   = -1;                                                      // no pending action
}

// duplicates a c string into a fresh heap buffer, mapping null to ""
static char* dlg_dup(const char* s) {
    if (!s) s = "";                                                             // null guard: treat as empty
    size_t n = strlen(s);                                                       // payload length
    char* out = (char*)malloc(n + 1);                                           // payload plus terminator
    if (!out) return NULL;                                                      // oom: hand back null
    memcpy(out, s, n + 1);                                                      // copy bytes and null
    return out;                                                                 // caller owns the copy
}

// begins a dialog: clears any previous one and stages a fresh empty one
void dialog_begin(UiContext* ctx, const char* key, const char* title, const char* message) {
    dialog_clear(&ctx->dialog);                                                 // drop any previous dialog
    UiDialog* d = &ctx->dialog;                                                 // working pointer
    d->key     = dlg_dup(key);                                                  // copy the script key
    d->title   = dlg_dup(title);                                                // copy the heading
    d->message = dlg_dup(message);                                              // copy the body text
    d->active  = true;                                                          // mark the dialog live
    d->anim_t  = 0.0;                                                           // start fully transparent
    d->open_started_at = 0.0;                                                   // anchor on first tick
    d->closing = false;                                                         // not fading out yet
    d->pending_btn   = -1;                                                      // no pending action
    d->hover_btn     = -1;                                                      // no hovered button
    d->active_btn    = -1;                                                      // no pressed button
    d->focused_field = -1;                                                      // no focused field
    ctx->redraw_needed = true;                                                  // schedule a repaint
    ctx->full_repaint  = true;                                                  // full repaint next frame
}

// appends one editable text field to the dialog
void dialog_add_field(UiContext* ctx, const char* key, const char* label,
                      const char* placeholder, const char* def) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->field_count >= d->field_cap) {                                       // field array is full
        d->field_cap = d->field_cap ? d->field_cap * 2 : 4;                     // double capacity
        d->fields = (UiDialogField*)realloc(d->fields,
                        sizeof(UiDialogField) * (size_t)d->field_cap);          // grow the array
    }
    UiDialogField* f = &d->fields[d->field_count++];                            // take the next slot
    memset(f, 0, sizeof(*f));                                                   // clear every field
    f->key         = dlg_dup(key);                                              // copy the field key
    f->label       = dlg_dup(label);                                            // copy the caption
    f->placeholder = dlg_dup(placeholder);                                      // copy the placeholder
    const char* init = def ? def : "";                                          // default text
    size_t n = strlen(init);                                                    // default length
    int cap = (int)n + 32;                                                      // room for a few edits
    f->input = (char*)malloc((size_t)cap);                                      // allocate the buffer
    if (f->input) {                                                             // allocation succeeded
        memcpy(f->input, init, n + 1);                                          // seed with the default
        f->input_len = (int)n;                                                  // record its length
        f->input_cap = cap;                                                     // record its capacity
    }
    f->cursor = f->input_len;                                                   // caret after the text
    if (d->focused_field < 0) d->focused_field = d->field_count - 1;            // focus the first field
}

// appends one button to the dialog
void dialog_add_button(UiContext* ctx, const char* id, const char* label, bool primary) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->button_count >= d->button_cap) {                                     // button array is full
        d->button_cap = d->button_cap ? d->button_cap * 2 : 4;                  // double capacity
        d->buttons = (UiDialogButton*)realloc(d->buttons,
                        sizeof(UiDialogButton) * (size_t)d->button_cap);        // grow the array
    }
    UiDialogButton* b = &d->buttons[d->button_count++];                         // take the next slot
    memset(b, 0, sizeof(*b));                                                   // clear every field
    b->id      = dlg_dup(id);                                                   // copy the action id
    b->label   = dlg_dup(label);                                                // copy the caption
    b->primary = primary;                                                       // mark accent fill
}

// finalises the dialog; drops it if it has no buttons
void dialog_commit(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op
    if (d->button_count == 0) dialog_clear(d);                                  // no action: drop entirely
    else ctx->redraw_needed = true;                                             // otherwise schedule a paint
}

// builds the JSON payload string and pushes an EV_DIALOG event
static void dialog_emit_json(UiContext* ctx, int btn_idx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: no-op

    size_t cap = 256;                                                           // generous starting size
    for (int i = 0; i < d->field_count; i++) {                                  // reserve room for each field
        cap += (d->fields[i].key    ? strlen(d->fields[i].key)    : 0) + 8;     // key + quoting overhead
        cap += (d->fields[i].input  ? strlen(d->fields[i].input)  : 0) * 2 + 4; // worst-case escaped value
    }
    char* buf = (char*)malloc(cap);                                             // allocate the output buffer
    if (!buf) return;                                                           // oom: nothing to emit
    size_t p = 0;                                                               // write cursor
    buf[p++] = '{';                                                             // open the outer object

    // "action":"<id>"
    static const char kAction[] = "\"action\":\"";                              // field prefix
    memcpy(buf + p, kAction, sizeof(kAction) - 1);                              // copy the prefix
    p += sizeof(kAction) - 1;
    const char* act = "close";                                                  // fallback action id
    if (btn_idx >= 0 && btn_idx < d->button_count && d->buttons[btn_idx].id)
        act = d->buttons[btn_idx].id;                                           // use the chosen button's id
    for (const char* c = act; *c && p + 2 < cap; c++) {
        if (*c == '"' || *c == '\\') buf[p++] = '\\';                           // escape quotes and slashes
        buf[p++] = *c;                                                          // copy the character
    }
    buf[p++] = '"';                                                             // close the action string

    // "fields":{...}
    if (d->field_count > 0) {                                                   // only when there are fields
        buf[p++] = ',';                                                         // separate from action
        static const char kFields[] = "\"fields\":{";                           // fields prefix
        memcpy(buf + p, kFields, sizeof(kFields) - 1);
        p += sizeof(kFields) - 1;

        for (int i = 0; i < d->field_count; i++) {
            if (i) buf[p++] = ',';                                              // separate entries
            buf[p++] = '"';                                                     // open the key string
            const char* k = d->fields[i].key ? d->fields[i].key : "";           // field key, or empty
            for (const char* c = k; *c && p + 2 < cap; c++) {
                if (*c == '"' || *c == '\\') buf[p++] = '\\';                   // escape quotes and slashes
                buf[p++] = *c;
            }
            buf[p++] = '"'; buf[p++] = ':'; buf[p++] = '"';                     // "key":"
            const char* v = d->fields[i].input ? d->fields[i].input : "";       // field value, or empty
            for (const char* c = v; *c && p + 2 < cap; c++) {
                if (*c == '"' || *c == '\\') buf[p++] = '\\';                   // escape quotes and slashes
                else if (*c == '\n') { buf[p++] = '\\'; buf[p++] = 'n'; continue; }  // escape newline
                else if (*c == '\r') { buf[p++] = '\\'; buf[p++] = 'r'; continue; }  // escape carriage return
                else if (*c == '\t') { buf[p++] = '\\'; buf[p++] = 't'; continue; }  // escape tab
                else if ((unsigned char)*c < 0x20) continue;                    // drop other control bytes
                buf[p++] = *c;                                                  // copy the character
            }
            buf[p++] = '"';                                                     // close the value string
        }
        buf[p++] = '}';                                                         // close the fields object
    }
    buf[p++] = '}';                                                             // close the outer object
    buf[p] = 0;                                                                 // null terminate

    UiEvent ev; memset(&ev, 0, sizeof(ev));                                     // zero the event
    ev.kind = EV_DIALOG;                                                        // dialog event kind
    strncpy(ev.widget_type, "dialog", 15);                                      // widget type name
    if (d->key) strncpy(ev.key_name, d->key, 63);                               // script key, if any
    ev.str_value = buf;                                                         // hand off the payload
    ui_event_push(ctx, &ev);                                                    // enqueue the event
}

// called from ui_tick once anim_t hits zero
void dialog_finish_close(UiContext* ctx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // nothing to close
    dialog_emit_json(ctx, d->pending_btn);                                      // emit the result event
    dialog_clear(d);                                                            // release every allocation
    ctx->redraw_needed = true;                                                  // schedule a repaint
    ctx->full_repaint  = true;                                                  // the dialog dimmed the screen
}

// computes the dialog box and the per-button rectangles
void ui_dialog_geom(UiContext* ctx, UiDialogGeom* g) {
    memset(g, 0, sizeof(*g));                                                   // clear the output
    UiDialog* d = &ctx->dialog;
    if (!d->active) return;                                                     // no live dialog: nothing to do

    int win_w = ctx->win_w > 0 ? ctx->win_w : 640;                              // fall back to a sane width
    int win_h = ctx->win_h > 0 ? ctx->win_h : 480;                              // fall back to a sane height

    int bw = DLG_WIDTH;                                                         // desired dialog width
    if (bw > win_w - 40) bw = win_w - 40;                                       // keep it on screen
    if (bw < 280) bw = 280;                                                     // enforce a minimum

    int inner_w = bw - 2 * DLG_PAD;                                             // usable inner width

    // pass 1: count wrapped message lines
    int msg_lines = 0;
    if (d->message && *d->message) {
        const char* s = d->message;
        int len = (int)strlen(s);
        float max_w = (float)inner_w;                                           // wrap budget
        int lines = 0;                                                          // running total
        int i = 0;
        while (i < len) {
            int seg_end = i;                                                    // end of this hard-break segment
            while (seg_end < len && s[seg_end] != '\n' && s[seg_end] != '\r') seg_end++;  // scan to a break

            float cur = 0;                                                      // width of the current line
            int seg_lines = 0;                                                  // lines in this segment
            int j = i;
            while (j < seg_end) {
                while (j < seg_end && (s[j] == ' ' || s[j] == '\t')) j++;       // skip leading spaces
                if (j >= seg_end) break;                                        // no more words
                int ws = j;                                                     // first byte of this word
                while (j < seg_end && s[j] != ' ' && s[j] != '\t') j++;         // advance past the word
                float ww = ui_text_measure_cached(ctx, s + ws, j - ws, DLG_BODY_FS);  // word width
                float sp = cur > 0 ? ui_text_measure_cached(ctx, " ", 1, DLG_BODY_FS) : 0;  // space before it
                if (cur + sp + ww > max_w && cur > 0) { cur = ww; seg_lines++; }  // would overflow: wrap
                else                                  { cur += sp + ww; }       // append to the line
            }
            if (cur > 0 || seg_lines == 0) seg_lines++;                         // account for the last line
            lines += seg_lines;                                                 // accumulate

            i = seg_end;                                                        // advance past the segment
            if (i < len && s[i] == '\r' && i + 1 < len && s[i + 1] == '\n') i += 2;  // crlf: two bytes
            else if (i < len) i++;                                              // lone cr or lf: one byte
        }
        if (lines == 0) lines = 1;                                              // always at least one line
        msg_lines = lines;
    }

    // pass 2: vertical layout, y counted from the top edge of the box
    int y = 0;                                                                  // running vertical offset
    int title_off  = 0;                                                         // title baseline offset
    int msg_off    = 0;                                                         // first message baseline
    int field_off  = 0;                                                         // top of the field block
    int button_off = 0;                                                         // top of the button row

    // title baseline + descent
    if (d->title && *d->title) {
        y += DLG_PAD;                                                           // padding above the title
        title_off = y + (int)(DLG_TITLE_FS * 0.85f);                            // baseline inside its line box
        y = title_off + (int)(DLG_TITLE_FS * 0.35f);                            // leave room for the descent
    }

    // message
    if (msg_lines > 0) {
        y += (title_off > 0) ? 14 : DLG_PAD;                                    // gap above the message
        msg_off = y + DLG_LINE_H - 5;                                           // first baseline
        y = msg_off + (msg_lines - 1) * DLG_LINE_H + 5;                         // advance to the last line
    }

    // fields
    int field_row_h = (int)(DLG_LABEL_FS * 1.4f) + 6 + DLG_INPUT_H;             // caption + input height
    if (d->field_count > 0) {
        y += (title_off > 0 || msg_lines > 0) ? 14 : DLG_PAD;                   // gap above the fields
        field_off = y;                                                          // remember the block top
        y += d->field_count * field_row_h;                                      // one row per field
        if (d->field_count > 1) y += (d->field_count - 1) * 6;                  // gaps between rows
    }

    // button row
    if (title_off > 0 || msg_lines > 0 || d->field_count > 0) y += 22;          // gap above the buttons
    else                                                       y += DLG_PAD;     // no content: just padding
    button_off = y;                                                             // remember the row top
    y += DLG_BTN_H;                                                             // button height
    y += DLG_PAD;                                                               // bottom padding

    int bh = y;                                                                 // total box height
    int bx = (win_w - bw) / 2;                                                  // horizontally centered
    int by = (win_h - bh) / 2;                                                  // vertically centered
    if (by < 20) by = 20;                                                       // keep clear of the top edge

    g->bx = bx; g->by = by; g->bw = bw; g->bh = bh;                             // publish the box
    g->tx = bx + DLG_PAD;                                                       // title left edge
    g->ty = by + title_off;                                                     // title baseline
    g->mx = bx + DLG_PAD;                                                       // message left edge
    g->my = by + msg_off;                                                       // first message baseline
    g->field_y     = by + field_off;                                            // top of the field block
    g->field_row_h = field_row_h;                                               // height of one field row
    g->input_w     = inner_w;                                                   // width of the inputs
    g->button_y    = by + button_off;                                           // top of the button row
    g->button_h    = DLG_BTN_H;                                                 // button height

    // buttons: measure and right-align
    int n = d->button_count;
    if (n > 16) n = 16;                                                         // hard cap on button count
    int widths[16];                                                             // per-button widths
    int total_w = 0;                                                            // total row width
    for (int i = 0; i < n; i++) {
        const char* lbl = d->buttons[i].label ? d->buttons[i].label : "";       // caption, or empty
        int tw = (int)ui_text_measure_cached(ctx, lbl, (int)strlen(lbl), DLG_BODY_FS);  // caption width
        int w = tw + 2 * DLG_BTN_PAD;                                           // plus side padding
        if (w < 72) w = 72;                                                     // enforce a minimum
        widths[i] = w;
        total_w += w;
    }
    if (n > 1) total_w += (n - 1) * DLG_BTN_GAP;                                // gaps between buttons

    int right = bx + bw - DLG_PAD;                                              // right edge of the row
    int cur_x = right - total_w;                                                // start of the row
    for (int i = 0; i < n; i++) {
        g->btn_x[i] = cur_x;                                                    // this button's left edge
        g->btn_w[i] = widths[i];                                                // this button's width
        cur_x += widths[i] + DLG_BTN_GAP;                                       // advance past the button
    }
    g->btn_count = n;                                                           // publish the count
}

// returns the button index under (x, y), or -1
static int dialog_button_at(UiContext* ctx, int x, int y) {
    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);                                                    // resolve the row layout
    for (int i = 0; i < g.btn_count; i++) {
        if (x >= g.btn_x[i] && x < g.btn_x[i] + g.btn_w[i] &&                   // inside on x
            y >= g.button_y && y < g.button_y + g.button_h)                     // and on y
            return i;                                                           // hit: hand back the index
    }
    return -1;                                                                  // miss
}

// returns the field index under (x, y), or -1
static int dialog_field_at(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    UiDialogGeom g;
    ui_dialog_geom(ctx, &g);                                                    // resolve the field layout
    int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;                               // caption line height + gap
    for (int i = 0; i < d->field_count; i++) {
        int fy = g.field_y + i * (g.field_row_h + 6);                           // top of this row
        int iy = fy + label_h;                                                  // top of its input box
        if (x >= g.bx + DLG_PAD && x < g.bx + DLG_PAD + g.input_w &&            // inside on x
            y >= iy && y < iy + DLG_INPUT_H)                                    // and on y
            return i;                                                           // hit: hand back the index
    }
    return -1;                                                                  // miss
}

// asks for a graceful close; the actual emit happens when the fade finishes
static void dialog_request_close(UiContext* ctx, int btn_idx) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // already closing or gone
    d->closing     = true;                                                      // start the fade out
    d->pending_btn = btn_idx;                                                   // remember the choice
    ctx->redraw_needed = true;                                                  // schedule a repaint
}

// updates the hovered button on pointer move
void dialog_mouse_move(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing
    int h = dialog_button_at(ctx, x, y);                                        // hit test the row
    if (h != d->hover_btn) { d->hover_btn = h; ctx->redraw_needed = true; }     // repaint on change
}

// handles a pointer press inside the dialog
void dialog_mouse_down(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing

    int b = dialog_button_at(ctx, x, y);                                        // button hit test first
    if (b >= 0) { d->active_btn = b; ctx->redraw_needed = true; return; }       // press the button

    int f = dialog_field_at(ctx, x, y);                                         // field hit test next
    if (f >= 0) {
        d->focused_field = f;                                                   // focus this field
        UiDialogGeom g;
        ui_dialog_geom(ctx, &g);                                                // resolve the geometry
        UiDialogField* fld = &d->fields[f];
        int label_h = (int)(DLG_LABEL_FS * 1.4f) + 6;                           // caption height
        int iy = g.field_y + f * (g.field_row_h + 6) + label_h;                 // top of this input box
        float local = (float)(x - (g.bx + DLG_PAD) - 10);                       // into text coordinates
        if (local < 0) local = 0;                                               // clamp at the left edge
        int off = fld->input_len;                                               // default: past the end
        if (ctx->font && fld->input && fld->input_len > 0) {
            float prev = 0;
            int i = 0;
            off = fld->input_len;
            while (i < fld->input_len) {
                int next = ui_utf8_next(fld->input, i, fld->input_len);         // advance one codepoint
                float adv = ui_text_measure_cached(ctx, fld->input + i, next - i, DLG_BODY_FS);  // its width
                float w2 = prev + adv;                                          // width up to the next glyph
                if (local < (prev + w2) * 0.5f) { off = i; break; }             // landed on this glyph
                i = next;
                prev = w2;
            }
        }
        fld->cursor = off;                                                      // place the caret
        ctx->redraw_needed = true;                                              // schedule a repaint
        (void)iy;
    }
}

// handles a pointer release inside the dialog
void dialog_mouse_up(UiContext* ctx, int x, int y) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing
    int was = d->active_btn;                                                    // remember the pressed button
    d->active_btn = -1;                                                         // clear the press state
    if (was < 0) return;                                                        // nothing was pressed
    int hit = dialog_button_at(ctx, x, y);                                      // hit test on release
    if (hit == was) dialog_request_close(ctx, was);                             // same button: accept
    else ctx->redraw_needed = true;                                             // moved off: just repaint
}

// routes a key press to the dialog
void dialog_key_down(UiContext* ctx, const UiWinEvent* e) {
    UiDialog* d = &ctx->dialog;
    if (!d->active || d->closing) return;                                       // ignore while closing

    if (e->key == UI_KEY_ESCAPE) {                                              // esc: pick the first non-primary
        int cancel_idx = -1;
        for (int i = 0; i < d->button_count; i++) {
            if (!d->buttons[i].primary) { cancel_idx = i; break; }              // first ghost button
        }
        dialog_request_close(ctx, cancel_idx);                                  // close with that id
        return;
    }
    if (e->key == UI_KEY_ENTER) {                                               // enter: pick the primary button
        int ok_idx = -1;
        for (int i = 0; i < d->button_count; i++)
            if (d->buttons[i].primary) { ok_idx = i; break; }                   // first accent button
        if (ok_idx < 0 && d->button_count > 0) ok_idx = d->button_count - 1;    // fallback: last button
        dialog_request_close(ctx, ok_idx);                                      // close with that id
        return;
    }
    if (e->key == UI_KEY_TAB) {                                                 // tab: cycle focused field
        if (d->field_count > 0) {
            int dir = (e->mods & 1) ? -1 : 1;                                   // shift-tab goes back
            int cur = d->focused_field;
            if (cur < 0) cur = (dir > 0) ? 0 : d->field_count - 1;              // no focus: pick an edge
            else         cur = (cur + dir + d->field_count) % d->field_count;   // step and wrap
            d->focused_field = cur;
            ctx->redraw_needed = true;
        }
        return;
    }

    int fi = d->focused_field;                                                  // active field
    if (fi < 0 || fi >= d->field_count) return;                                 // none focused: nothing to edit
    UiDialogField* f = &d->fields[fi];

    if (e->utf8_len > 0) {                                                      // printable text
        if (f->input_len + e->utf8_len + 1 > f->input_cap) {                    // buffer too small
            int nc = f->input_cap ? f->input_cap : 32;
            while (nc < f->input_len + e->utf8_len + 1) nc *= 2;                // grow to fit
            char* nb = (char*)realloc(f->input, (size_t)nc);
            if (!nb) return;                                                    // oom: drop the key
            f->input = nb;
            f->input_cap = nc;
        }
        memmove(f->input + f->cursor + e->utf8_len, f->input + f->cursor,
                (size_t)(f->input_len - f->cursor + 1));                        // make room
        memcpy(f->input + f->cursor, e->utf8, (size_t)e->utf8_len);             // insert the bytes
        f->input_len += e->utf8_len;                                            // grow the length
        f->cursor    += e->utf8_len;                                            // advance the caret
        ctx->redraw_needed = true;
        return;
    }
    if (e->key == UI_KEY_BACKSPACE) {                                           // backspace
        if (f->cursor > 0) {
            int prev = ui_utf8_prev(f->input, f->cursor);                       // start of the previous glyph
            memmove(f->input + prev, f->input + f->cursor,
                    (size_t)(f->input_len - f->cursor + 1));                    // splice it out
            f->input_len -= f->cursor - prev;                                   // shrink the length
            f->cursor    = prev;                                                // move the caret
            ctx->redraw_needed = true;
        }
        return;
    }
    if (e->key == UI_KEY_DELETE) {                                              // delete (forward)
        if (f->cursor < f->input_len) {
            int next = ui_utf8_next(f->input, f->cursor, f->input_len);         // end of the next glyph
            memmove(f->input + f->cursor, f->input + next,
                    (size_t)(f->input_len - next + 1));                         // splice it out
            f->input_len -= next - f->cursor;                                   // shrink the length
            ctx->redraw_needed = true;
        }
        return;
    }
    if (e->key == UI_KEY_LEFT)  { f->cursor = ui_utf8_prev(f->input, f->cursor); ctx->redraw_needed = true; return; }  // move caret left
    if (e->key == UI_KEY_RIGHT) { f->cursor = ui_utf8_next(f->input, f->cursor, f->input_len); ctx->redraw_needed = true; return; }  // move caret right
    if (e->key == UI_KEY_HOME)  { f->cursor = 0;            ctx->redraw_needed = true; return; }  // jump to start
    if (e->key == UI_KEY_END)   { f->cursor = f->input_len; ctx->redraw_needed = true; return; }  // jump to end
}
// source/libraries/ui/widgets/ui_widgets.h
// Widget-specific declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "ui_internal.h"
#include "ui_window.h"

// maps a local x offset inside an input widget to a byte offset in its text
int  input_hit_offset(UiContext* ctx, Widget* w, float local_x);

// finds the [start, end) byte range of the word containing pos
void input_word_range(Widget* w, int pos, int* out_start, int* out_end);

// true when the state has a live non-empty selection
bool input_has_selection(WidgetState* s);

// returns the [min, max) byte range covered by the selection
void input_sel_range(WidgetState* s, int* out_min, int* out_max);

// pushes a snapshot of the current buffer onto the undo stack
void input_push_undo(WidgetState* s, const char* text, int len, int cursor);

// deletes [a, b) from the input buffer, moving the caret to a
void input_delete_range(UiContext* ctx, Widget* w, WidgetState* s, int a, int b);

// replaces the input buffer with a new heap copy and frees the old one
void input_set_buffer(UiContext* ctx, Widget* w, char* buf, int len);

// steps the buffer back one snapshot, moving the current state onto redo
void input_undo(UiContext* ctx, Widget* w, WidgetState* s);

// steps the buffer forward one snapshot, moving the current state back to undo
void input_redo(UiContext* ctx, Widget* w, WidgetState* s);

// emits a change event for an input widget, copying the new string
void push_input_change(UiContext* ctx, Widget* w, const char* str_val);

// syncs a widget's input view against the descriptor's value_text
void sync_input_buffer(Widget* w, WidgetState* s, const char* sval, int slen);

// called from ui_tick once anim_t hits zero
void dialog_finish_close(UiContext* ctx);

// updates the hovered button on pointer move
void dialog_mouse_move(UiContext* ctx, int x, int y);

// handles a pointer press inside the dialog
void dialog_mouse_down(UiContext* ctx, int x, int y);

// handles a pointer release inside the dialog
void dialog_mouse_up(UiContext* ctx, int x, int y);

// routes a key press to the dialog
void dialog_key_down(UiContext* ctx, const UiWinEvent* e);

// fades any 0xAARRGGBB color by a 0..1 factor (dialog variant)
uint32_t dlg_fade(uint32_t c, double t);

// fades any 0xAARRGGBB color by a 0..1 factor
uint32_t dlg_fade_static(uint32_t c, double t);

// releases every owned allocation and zeroes the notification
void notif_clear(UiNotification* n);

// places every live notification stacked from the bottom-right corner
void notif_place_all(UiContext* ctx);

// emits a notification close event to the script queue
void notif_emit_close(UiContext* ctx, UiNotification* n);

// computes the natural (content-hugging) width of every column into out[]
void table_natural_widths(UiContext* ctx, Widget* w, float* out);

// resolves the final per-column widths so the table fits into avail pixels
void table_resolve_widths(UiContext* ctx, Widget* w, float avail, float* out);

// flattens a wk_tree descriptor's "nodes" array into w->tree_nodes
void tree_build_nodes(UiContext* ctx, Widget* w, Table* arr);

// draws the wk_tree widget into its own rectangle
void tree_render(UiContext* ctx, Widget* w, int x, int y, int ww, int hh);

// emits the appropriate click or change event for a press on a wk_tree
void tree_handle_press(UiContext* ctx, Widget* w);

// flattens a wk_tabs descriptor's "tabs" array into w->tabs
void tabs_build_items(UiContext* ctx, Widget* w, Table* arr);

// content-driven preferred width of a wk_tabs widget (tabs + gaps + "+" button)
float tabs_natural_width(UiContext* ctx, Widget* w);

// fills out_x/out_w with the current layout target for each tab
void tabs_target_positions(UiContext* ctx, Widget* w,
                           float* out_x, float* out_w);

// draws the wk_tabs strip into its own rectangle
void tabs_render(UiContext* ctx, Widget* w, int x, int y, int ww, int hh);

// records the pressed tab / "+" button and the pointer x at press time
void tabs_handle_mousedown(UiContext* ctx, Widget* w, int x, int y);

// refreshes hover + drag state; returns true if anything changed
bool tabs_handle_mousemove(UiContext* ctx, Widget* w, int x, int y);

// finishes a drag and emits tab_move; returns true when it consumed the mouseup
bool tabs_handle_mouseup(UiContext* ctx, Widget* w, int x, int y);

// emits the select / close / add event matching the recorded press
void tabs_handle_click(UiContext* ctx, Widget* w);

#endif
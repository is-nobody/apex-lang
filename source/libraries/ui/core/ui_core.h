// source/libraries/ui/core/ui_core.h
// Core context / state / build / runtime declarations for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_CORE_H
#define UI_CORE_H

#include "ui_internal.h"

// context lifecycle
UiContext* ui_ensure(VM* vm);
void       ui_context_destroy(UiContext* ctx);

// widget pool + tree construction helpers
Widget* ui_widget_alloc(UiContext* ctx);
void    ui_widgets_reset(UiContext* ctx);
void    widget_add_child(UiContext* ctx, Widget* p, Widget* c);
bool    widget_focusable(WidgetKind k);
void    collect_focusable(Widget* w, Widget** arr, int* n, int cap);
const char* widget_kind_name(WidgetKind k);

// descriptor -> tree
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx);

// text measurement memo
float ui_text_measure_cached(UiContext* ctx, const char* s, int len, float size);

// descriptor hash of a whole tree
uint64_t hash_descr_recursive(UiContext* ctx, Value descr);

// state accessor + gc
WidgetState* ui_state_get(UiContext* ctx, uint64_t id);
void         state_gc_mark_root(UiContext* ctx, Widget* root);
void         state_gc_sweep(UiContext* ctx);

// event queue + table conversion + apex callback
void  ui_event_push(UiContext* ctx, const UiEvent* ev);
Value ui_event_to_table(VM* vm, const UiEvent* ev);
Value apex_call_function(VM* vm, Value func, int argc, Value* argv);
void  fire_click(UiContext* ctx, Widget* w);

// animation
void ui_tick(UiContext* ctx, double dt);
bool ui_animating(UiContext* ctx);

// message loop
bool pump(UiContext* ctx);

// builtin entry points
Value ui_impl_init(VM* vm, int argc, Value* argv);
Value ui_impl_render(VM* vm, int argc, Value* argv);
Value ui_impl_next_event(VM* vm, int argc, Value* argv);
Value ui_impl_run(VM* vm, int argc, Value* argv);
Value ui_impl_shutdown(VM* vm, int argc, Value* argv);
Value ui_impl_set_title(VM* vm, int argc, Value* argv);
Value ui_impl_window_size(VM* vm, int argc, Value* argv);
Value ui_impl_open_file(VM* vm, int argc, Value* argv);
Value ui_impl_open_folder(VM* vm, int argc, Value* argv);

// host paint hook
void ui_window_paint_cb(void* user, int w, int h);

// rebuild/redraw helpers
void rebuild_and_draw(UiContext* ctx);
void redraw_only(UiContext* ctx);

#endif
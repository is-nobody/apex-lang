// source/libraries/ui/ui_module.h
// Public declarations for Apex ui module
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_MODULE_H
#define UI_MODULE_H

#include "vm.h"

// dispatcher entry from the vm
bool ui_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result);

// create the native window
Value ui_impl_init(VM* vm, int argc, Value* argv);

// record the widget tree for painting
Value ui_impl_render(VM* vm, int argc, Value* argv);

// block until the next ui event
Value ui_impl_next_event(VM* vm, int argc, Value* argv);

// tear down the native window
Value ui_impl_shutdown(VM* vm, int argc, Value* argv);

// set the window title
Value ui_impl_set_title(VM* vm, int argc, Value* argv);

// query the current window size
Value ui_impl_window_size(VM* vm, int argc, Value* argv);

// release every ui-owned allocation
void  ui_context_destroy(struct UiContext* ctx);

// build a leaf descriptor
Value ui_ctor_text(VM* vm, int argc, Value* argv, const char* type);

// build a button descriptor
Value ui_ctor_button(VM* vm, int argc, Value* argv);

// build a container descriptor
Value ui_ctor_container(VM* vm, int argc, Value* argv, const char* type);

#endif
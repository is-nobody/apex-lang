// source/libraries/ui/core/ui_context.c
// Context lifecycle for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_core.h"
#include "ui_hash.h"
#include "ui_table.h"
#include "ui_widgets.h"
#include "ui_interaction.h"
#include "ui_render.h"
#include "ui_window.h"
#include "ui_text.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// lazily creates the ui context on first use, attached to the vm
UiContext* ui_ensure(VM* vm) {
    if (vm->ui) return vm->ui;                                    // already created
    UiContext* ctx = (UiContext*)calloc(1, sizeof(UiContext));    // fresh zeroed context
    ctx->vm = vm;                                                 // back-pointer to the vm
    ctx->focused_id = 0;                                          // nothing focused
    ctx->last_tree = MAKE_NONE();                                 // no tree rendered yet
    ctx->title_end_x = 14;                                        // left_pad

    ctx->font = ui_font_load_system();                            // try to load a default font
    if (!ctx->font) {
        fprintf(stderr, "[ui] no system font available; text will not render\n");
    }

    // pre-intern every descriptor field name once; saves a hash + lookup per field per widget
    static const char* const kKeyNames[UK_COUNT] = {
        "__t", "key", "align", "justify", "gap", "padding", "weight",
        "min_width", "max_width", "disabled", "visible", "on_top", "bare",
        "width", "height", "content", "label", "checked", "group",
        "value", "min", "max", "value_text", "children", "options",
        "columns", "rows", "stripe", "header",
        "nodes", "selected", "indent", "expanded",
        "tabs", "closeable", "add_button", "reorder",
        "tooltip", "tooltip_delay", "left", "right", "text", "title", "collapsed", "gutter"
    };
    for (int i = 0; i < UK_COUNT; i++)
        ctx->key_cache[i] = ui_intern(vm, kKeyNames[i]);

    vm->ui = ctx;                                                 // publish on the vm
    return ctx;
}

// releases every heap allocation owned by one persistent state entry
static void free_state_entry(WidgetState* s) {
    for (int j = 0; j < s->undo_count; j++) {
        free(s->undo_stack[j].removed);
        free(s->undo_stack[j].inserted);
    }
    for (int j = 0; j < s->redo_count; j++) {
        free(s->redo_stack[j].removed);
        free(s->redo_stack[j].inserted);
    }
    free(s->undo_stack);
    free(s->redo_stack);
    free(s->input_buf);
    free(s->tab_anim_x);
    free(s->tab_anim_w);
    free(s->line_starts);
}

// tears down the ui context and clears the vm's back-pointer
void ui_context_destroy(UiContext* ctx) {
    if (!ctx) return;                                             // null guard
    VM* saved_vm = ctx->vm;                                       // remember vm for back-pointer clear
    ui_widgets_reset(ctx);                                        // free all per-widget allocations
    ui_context_menu_free(ctx);                                    // release any open context menu
    ui_menubar_free(ctx);                                         // release the menu bar

    for (int i = 0; i < ctx->state.count; i++)                    // release per-state history
        free_state_entry(&ctx->state.entries[i]);

    ui_notification_destroy(ctx);                                 // tear down notification popups
    free(ctx->pool);                                              // free the widget pool array
    free(ctx->state.entries);                                     // free the persistent state table
    free(ctx->text_cache);                                        // free the text measurement cache
    free(ctx->state.index);                                       // free the state lookup index
    dialog_clear(&ctx->dialog);                                   // release any open dialog strings
    ui_wrap_cache_free(ctx);                                      // free the wrap cache and its line arrays
    for (int i = 0; i < ctx->queue.count; i++) free(ctx->queue.events[i].str_value);  // release queued event strings
    free(ctx->queue.events);                                      // free the event queue array
    if (ctx->font)   ui_font_free(ctx->font);                     // release the font
    if (ctx->window) ui_window_destroy(ctx->window);              // release the host window
    ctx->last_tree = MAKE_NONE();                                 // clear the cached tree
    ctx->has_tree = false;
    hit_grid_free(ctx);                                           // release hit-test grid
    ui_painter_shutdown();                                        // release the corner atlas
    ui_damage_free(ctx);                                          // free ui damage
    free(ctx);                                                    // free the context itself
    if (saved_vm) saved_vm->ui = NULL;                            // drop the vm's back-pointer
}
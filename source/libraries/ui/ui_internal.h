// source/libraries/ui/ui_internal.h
// Internal declarations for Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "vm.h"
#include "ui_theme.h"
#include <stdint.h>
#include <stdbool.h>

// opaque host window handle
typedef struct UiWindow UiWindow;

// opaque loaded font handle
typedef struct UiFont UiFont;

// length-tagged heap string (may contain nulls)
typedef struct { char* ptr; int len; } Str;

typedef enum {
    WK_TEXT, WK_HEADING, WK_BUTTON, WK_LINK, WK_CHECKBOX, WK_RADIO,
    WK_SLIDER, WK_PROGRESS, WK_INPUT, WK_BADGE, WK_IMAGE,
    WK_SEPARATOR, WK_SPACER,
    WK_VBOX, WK_HBOX, WK_PANEL, WK_FRAME, WK_SCROLL, WK_HSCROLL, WK_GRID
} WidgetKind;  // leaf widget kinds then container kinds

typedef enum { AL_START, AL_CENTER, AL_END, AL_STRETCH } Align;  // cross-axis child placement
typedef enum { JU_START, JU_CENTER, JU_END,
               JU_SPACE_BETWEEN, JU_SPACE_AROUND, JU_SPACE_EVENLY } Justify;  // main-axis packing

typedef struct Widget Widget;            // forward declaration for self-reference
struct Widget {
    WidgetKind kind;                     // discriminator for rendering and behavior
    uint64_t   id;                       // stable identity derived from parent + key/index
    Widget*    parent;                   // null for the root
    Widget**   children;                 // heap array of child pointers
    int        child_count;              // number of live children
    int        child_cap;                // allocated capacity of the children array

    float x, y, w, h;                    // computed geometry after layout
    float meas_w, meas_h;                // intrinsic measured size from content
    float req_w, req_h;                  // explicit size request (-1 = unset)
    float min_w, max_w;                  // width clamps, 0 = no constraint

    Align   align;                       // cross-axis alignment
    Justify justify;                     // main-axis justification
    float   weight;                      // flex weight in the parent's layout
    float   gap, padding;                // child spacing and inner padding
    bool    visible, disabled;           // visibility and interaction flags

    char*      key_str;                  // user-supplied key string, or null
    Str     text;                        // label/content text

    bool   hovered, pressed, focused;    // per-frame interaction flags
    bool   checked;                      // checkbox / radio state
    double num_value, num_min, num_max;  // slider / progress numeric state
    Str    group;                        // radio group name

    Str    input_value;                  // owned text buffer for input widgets
    int    cursor;                       // cursor byte offset into input_value

    double scroll_y, content_h;          // vertical scroll offset and content height
};

// one snapshot in an input widget's undo/redo history
typedef struct {
    char* text;    // owned copy of the input buffer
    int   len;     // length of that copy
    int   cursor;  // caret position at snapshot time
} UndoEntry;

typedef struct {
    uint64_t id;                // widget identity this state belongs to
    bool     hovered, pressed;  // persistent interaction flags
    int      cursor;            // saved cursor position (-1 = unset)
    int      sel_anchor;        // byte offset of the selection anchor (-1 = no selection)
    double   scroll_y;          // saved scroll offset
    double   blink_phase;       // cursor blink animation phase

    // per-input undo/redo history; null until the first edit
    UndoEntry* undo_stack;      // snapshots that undo can walk back through
    int        undo_count;      // live entries in the undo stack
    int        undo_cap;        // allocated capacity of the undo stack
    UndoEntry* redo_stack;      // snapshots that redo can walk forward through
    int        redo_count;      // live entries in the redo stack
    int        redo_cap;        // allocated capacity of the redo stack
} WidgetState;                  // per-widget persistent state across frames

typedef struct {
    WidgetState* entries;     // flat array of widget states
    int          count, cap;  // live count and allocated capacity
} StateMap;                   // id-indexed state store for the ui context

typedef struct {
    int  x, y;  // last known pointer position
    bool lmb;   // left mouse button held
    int  mods;  // modifier bitmask (shift/ctrl/alt)
} InputState;   // aggregated pointer and modifier state

typedef enum {
    EV_NONE, EV_CLOSE, EV_CLICK, EV_CHANGE, EV_KEY, EV_SCROLL,
    EV_HOVER, EV_RESIZE
} EventKind;  // event kind discriminator

typedef struct {
    EventKind kind;              // which kind of event this is
    uint64_t  widget_id;         // source widget id (0 if not applicable)
    char      widget_type[16];   // widget kind name, truncated to fit
    char      key_name[64];      // user key string, truncated to fit
    double    num_value;         // numeric payload (slider, scroll)
    bool      bool_value;        // boolean payload (checkbox)
    char*     str_value;         // heap-owned string payload, or null
    int       key_code;          // platform key code for key events
    int       mods;              // modifier bitmask for key events
    int       mouse_x, mouse_y;  // pointer position for pointer events
    double    scroll_dy;         // wheel delta for scroll events
} UiEvent;                       // a single queued ui event

typedef struct {
    UiEvent* events;      // ring/linear queue of pending events
    int      count, cap;  // live count and allocated capacity
} EventQueue;             // fifo of events awaiting script consumption

typedef struct UiContext {
    VM*       vm;               // owner vm, used for interning and refcounts
    UiWindow* window;           // native window handle, null until init
    uint32_t* pixels;           // framebuffer pointer for the painter
    int       win_w, win_h;     // current window size in pixels

    Widget* pool;               // bump-allocated widget storage for this frame
    int     pool_cap;           // allocated capacity of the widget pool
    int     pool_used;          // number of widgets allocated this frame

    Value last_tree;            // last widget descriptor passed to render()
    bool  has_tree;             // true when last_tree holds a live reference

    Widget* root;               // root of the current widget tree

    InputState input;           // current pointer and modifier state
    uint64_t   focused_id;      // widget id with keyboard focus, 0 for none
    uint64_t   drag_widget_id;  // widget currently being dragged (slider or input)

    // double-click detection
    double     last_click_time; // wall clock of the previous click
    uint64_t   last_click_id;   // widget id of the previous click

    StateMap   state;           // persistent per-widget state across frames
    EventQueue queue;           // pending events awaiting the script

    UiFont*    font;            // loaded font used for text rendering
    int        font_size;       // font size in pixels

    bool       redraw_needed;   // set when the tree must be re-laid out and painted
    bool       quit_requested;  // set when the ui should shut down
} UiContext;                    // per-vm ui session state

// lazily create the ui context on the vm
UiContext* ui_ensure(VM* vm);

// release every ui-owned allocation
void ui_context_destroy(UiContext* ctx);

// allocate a zeroed widget from the pool
Widget* ui_widget_alloc(UiContext* ctx);

// free per-widget heap allocations and rewind the pool
void ui_widgets_reset(UiContext* ctx);

// recursively materialize a descriptor table
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx);

// compute geometry for the whole tree
void ui_layout(UiContext* ctx);

// draw the tree into the current framebuffer
void ui_render_frame(UiContext* ctx);

// advance animated state by dt seconds
void ui_tick(UiContext* ctx, double dt);

// fetch or create per-widget persistent state
WidgetState* ui_state_get(UiContext* ctx, uint64_t id);

// enqueue an event
void ui_event_push(UiContext* ctx, const UiEvent* ev);

// convert an event into an apex table
Value ui_event_to_table(VM* vm, const UiEvent* ev);

// derive a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx);

// bind the painter to a framebuffer
void ui_painter_init(uint32_t* pixels, int w, int h);

// filled axis-aligned rectangle
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c);

// filled rounded rectangle
void ui_painter_fill_round(int x, int y, int w, int h, int r, uint32_t c);

// outlined rectangle
void ui_painter_stroke_rect(int x, int y, int w, int h, uint32_t c);

// outlined rounded rectangle
void ui_painter_stroke_round(int x, int y, int w, int h, int r, uint32_t c);

// push a scissor rectangle
void ui_painter_push_clip(int x, int y, int w, int h);

// pop the most recent scissor
void ui_painter_pop_clip(void);

// clear the whole clip stack
void ui_painter_reset_clip(void);

#endif
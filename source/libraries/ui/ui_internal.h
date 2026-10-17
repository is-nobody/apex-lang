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

// opaque menu bar state, defined in ui_menubar.c
typedef struct UiMenuBar UiMenuBar;

// length-tagged heap string (may contain nulls)
typedef struct { char* ptr; int len; } Str;

typedef enum {
    WK_TEXT, WK_HEADING, WK_BUTTON, WK_CHECKBOX, WK_RADIO,
    WK_SLIDER, WK_PROGRESS, WK_INPUT, WK_BADGE, WK_COMBOBOX,
    WK_SEPARATOR, WK_SPACER, WK_TABLE, WK_LIST_ITEM, WK_SWITCH, WK_TREE,
    WK_TABS, WK_VBOX, WK_HBOX, WK_PANEL, WK_SCROLL, WK_HSCROLL,
    WK_NUMBER_INPUT
} WidgetKind;  // leaf widget kinds then container kinds

typedef enum { AL_START, AL_CENTER, AL_END, AL_STRETCH } Align;  // cross-axis child placement
typedef enum { JU_START, JU_CENTER, JU_END,
               JU_SPACE_BETWEEN, JU_SPACE_AROUND, JU_SPACE_EVENLY } Justify;  // main-axis packing

// one column of a wk_table widget
typedef struct {
    Str   label;    // header caption, may be empty
    Align align;    // text alignment within the column
    float width;    // fixed width in pixels, 0 = auto-fit to content
} TableColumn;

// one visible row of a wk_tree widget, in depth-first order
typedef struct {
    Str  label;         // display text
    Str  key;           // event key for the row
    int  depth;         // indentation depth, 0 for root nodes
    bool has_children;  // true when this node carries a non-empty nodes array
    bool expanded;      // true when children are visible
} TreeNode;

// one tab in a wk_tabs strip
typedef struct {
    Str  label;         // display text
    Str  key;           // event key for this tab
    bool closeable;     // true when the tab shows a close button
} TabItem;

typedef struct Widget Widget;            // forward declaration for self-reference
struct Widget {
    WidgetKind kind;                     // discriminator for rendering and behavior
    uint64_t   id;                       // stable identity derived from parent + key/index
    Widget*    parent;                   // null for the root
    Widget**   children;                 // child pointer array, lives in the per-frame arena
    int        child_count;              // number of live children
    int        child_cap;                // allocated capacity of the children array

    float x, y, w, h;                    // computed geometry after layout
    float meas_w, meas_h;                // intrinsic measured size from content
    float req_w, req_h;                  // explicit size request (-1 = unset)
    float min_w, max_w;                  // width clamps, 0 = no constraint

    // aggregated screen-space bbox of the subtree, computed after layout
    int bbox_x0, bbox_y0, bbox_x1, bbox_y1;

    Align   align;                       // cross-axis alignment
    Justify justify;                     // main-axis justification
    float   weight;                      // flex weight in the parent's layout
    float   gap, padding;                // child spacing and inner padding
    bool    visible, disabled;           // visibility and interaction flags
    bool    on_top;                      // if true, draw subtree after siblings

    char*      key_str;                  // user-supplied key string, or null
    Str     text;                        // label/content text

    // visual-hash memo. the hash is a pure function of the widget's fields
    uint64_t visual_hash_cache;          // cached full digest, 0 = unset
    uint64_t visual_hash_fp;             // fingerprint the cache was computed for

    // cached wrapped-line breakdown for wk_text / wk_heading
    int*    line_starts;                 // byte offset of the first byte on each line
    int*    line_ends;                   // byte offset just past the last byte on each line
    int     line_count;                  // number of cached lines
    int     line_cap;                    // allocated capacity of the line arrays

    // cached wrap-cache key for the (text, size, avail_w) triple this widget was last measured against
    uint64_t wrap_key;
    float    wrap_avail_w;               // avail_w the cached key was computed for
    float    wrap_fs;                    // font size the cached key was computed for

    bool   hovered, pressed, focused;    // per-frame interaction flags
    int    hover_row;                    // wk_tree: hovered row index, -1 = none
    bool   checked;                      // checkbox / radio state
    double num_value, num_min, num_max;  // slider / progress numeric state
    Str    group;                        // radio group name

    // wk_combobox state
    Str*   combo_options;                // arena-owned array of option labels
    int    combo_option_count;           // number of options in the list
    int    combo_selected;               // index of the selected option, -1 = none
    Str    combo_value;                  // display text (usually one of the options)
    bool   combo_open;                   // dropdown expanded this frame

    Str    input_value;                  // non-owning view into WidgetState.input_buf for WK_INPUT
    int    cursor;                       // cursor byte offset into input_value
    int    sel_anchor;                   // selection anchor byte offset, -1 = none

    // wk_table state
    TableColumn* table_cols;             // arena-owned array of column metadata
    int          table_col_count;        // number of columns
    Str*         table_cells;            // arena-owned flat row-major cell array
    int          table_row_count;        // number of body rows
    bool         table_stripe;           // alternate row background bands
    bool         table_header;           // draw the header row

    // wk_tree state
    TreeNode* tree_nodes;      // arena-owned flat list of visible nodes
    int       tree_node_count; // number of visible nodes
    Str       tree_selected;   // key of the selected node, empty for none
    int       tree_indent;     // pixels per indentation level

    // wk_tabs state
    TabItem* tabs;             // arena-owned array of tab items
    int      tab_count;        // number of tabs
    Str      tab_selected;     // key of the selected tab, empty for none
    int      tab_hover_idx;    // hovered tab index, -1 for none
    bool     tab_hover_close;  // hover is on the hovered tab's close button
    int      tab_press_idx;    // index of the tab that was pressed, -1 for none
    bool     tab_press_close;  // press was on that tab's close button
    int      tab_drag_idx;     // tab being dragged, -1 for none
    int      tab_drag_target;  // target slot during a drag
    int      tab_drag_start_x; // pointer x at the moment the press landed
    int      tab_drag_cur_x;   // most recent pointer x during a drag
    int      tab_drag_grab_dx; // pointer offset from the tab's left edge at press
    bool     tab_show_add;     // render the "+" button
    bool     tab_reorder;      // allow drag-to-reorder
    bool     tab_add_hover;    // "+" button hovered
    bool     tab_add_press;    // "+" button pressed
    bool     tab_default_closeable;  // per-widget default for closeable

    // wk_tabs animation pointers
    float*   tab_anim_x;
    float*   tab_anim_w;
    int      tab_anim_n;

    double scroll_y, content_h;          // vertical scroll offset and content height
    float  switch_knob;                  // wk_switch: 0 = off, 1 = on, mirrored from state
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
    int      hover_row;         // wk_tree: hovered row index, -1 = none
    bool     seen;              // marked during the current frame by the state gc
    int      cursor;            // saved cursor position (-1 = unset)
    int      sel_anchor;        // byte offset of the selection anchor (-1 = no selection)
    double   scroll_y;          // saved scroll offset, animated toward scroll_target
    double   scroll_target;     // offset the smooth scroll animation eases toward
    double   blink_phase;       // cursor blink animation phase
    bool     combo_open;        // dropdown expanded state persisted across frames

    // wk_switch animation
    float    switch_knob;       // 0 = off, 1 = on, eased toward checked
    float    switch_from;       // knob position at the start of the current animation
    float    switch_to;         // knob target of the current animation
    float    switch_anim_t;     // elapsed seconds in the current animation
    bool     switch_init;       // true once the initial knob position was seeded

    // owned text buffer for wk_input widgets
    char*      input_buf;       // malloc'd, null terminated, or null
    int        input_len;       // length of that buffer
    uint64_t   input_hash;      // hash of the last descriptor value_text synced from

    // per-input undo/redo history; null until the first edit
    UndoEntry* undo_stack;      // snapshots that undo can walk back through
    int        undo_count;      // live entries in the undo stack
    int        undo_cap;        // allocated capacity of the undo stack
    UndoEntry* redo_stack;      // snapshots that redo can walk forward through
    int        redo_count;      // live entries in the redo stack
    int        redo_cap;        // allocated capacity of the redo stack

    // wk_tabs drag animation
    float* tab_anim_x;
    float* tab_anim_w;
    int    tab_anim_n;
    bool   tab_anim_init;       // true while the animation is running
} WidgetState;                  // per-widget persistent state across frames

typedef struct {
    WidgetState* entries;     // flat array of widget states
    int          count, cap;  // live count and allocated capacity
    int*         index;       // open-addressed: id hash -> entry index, -1 empty
    int          index_cap;   // power of two, kept >= 2 * (count + 1)
} StateMap;                   // id-indexed state store for the ui context

typedef struct {
    int  x, y;  // last known pointer position
    bool lmb;   // left mouse button held
    int  mods;  // modifier bitmask (shift/ctrl/alt)
} InputState;   // aggregated pointer and modifier state

typedef enum {
    EV_NONE, EV_CLOSE, EV_CLICK, EV_CHANGE, EV_KEY, EV_SCROLL,
    EV_HOVER, EV_RESIZE, EV_DIALOG, EV_NOTIFICATION
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

// dirty rectangle in window coordinates; used for damage-tracked repaints
typedef struct {
    int x, y, w, h;
} DamageRect;

// one entry in the per-frame widget snapshot used for damage detection
typedef struct {
    uint64_t id;           // widget identity, stable across frames when keyed
    int      x, y, w, h;   // border_box, rounded to integer pixels
    uint64_t hash;         // digest of visual state (kind, text, flags, colors, ...)
} WidgetSnapshot;

typedef struct {
    WidgetSnapshot* entries;
    int             count;
    int             cap;
} SnapshotTable;

// uniform grid over the laid-out widget tree, rebuilt after every ui_layout()
typedef struct {
    int       cols, rows;     // grid dimensions in cells
    int       win_w, win_h;   // window size the grid was built for
    int*      cell_start;     // [cols*rows + 1] prefix-sum offsets into cell_items
    Widget**  cell_items;     // [cell_start[cols*rows]] widget pointers, draw order
} HitGrid;

// one memoized text measurement: content hash + size -> advance width
typedef struct TextMeasureEntry {
    uint64_t hash;        // 0 = empty slot; nonzero otherwise
    float    width;       // cached advance width in pixels
    uint32_t last_frame;  // frame counter when this entry was last touched
} TextMeasureEntry;

// one memoized wrapped-text breakdown
typedef struct {
    uint64_t key;         // hash of (size, max_w, len, text bytes)
    uint32_t last_frame;  // text_cache_frame value when the entry was last touched
    int      lines;       // number of wrapped lines
    float    max_width;   // widest wrapped line in pixels
    int*     starts;      // malloc'd [lines], byte offset of the first byte on each line
    int*     ends;        // malloc'd [lines], byte offset just past the last byte on each line
} WrapCacheEntry;

// non-owning view into a string value; ptr is valid while the owning table holds the entry
typedef struct { const char* ptr; int len; } StrView;

// cached interned key ids for hot descriptor-field lookups
typedef enum {
    UK_T, UK_KEY, UK_ALIGN, UK_JUSTIFY, UK_GAP, UK_PADDING, UK_WEIGHT,
    UK_MIN_WIDTH, UK_MAX_WIDTH, UK_DISABLED, UK_VISIBLE, UK_ON_TOP,
    UK_WIDTH, UK_HEIGHT, UK_CONTENT, UK_LABEL, UK_CHECKED, UK_GROUP,
    UK_VALUE, UK_MIN, UK_MAX, UK_VALUE_TEXT, UK_CHILDREN, UK_OPTIONS,
    UK_COLUMNS, UK_ROWS, UK_STRIPE, UK_HEADER, UK_NODES, UK_SELECTED,
    UK_INDENT, UK_EXPANDED, UK_TABS, UK_CLOSEABLE, UK_ADD_BUTTON, UK_REORDER,
    UK_COUNT
} UiKeyId;

// modal dialog
typedef struct {
    char*  key;            // field key (heap-owned)
    char*  label;          // caption above the input (heap-owned)
    char*  placeholder;    // placeholder text (heap-owned)
    char*  input;          // editable buffer (heap-owned)
    int    input_len;      // byte length
    int    input_cap;      // allocated capacity
    int    cursor;         // caret byte offset
} UiDialogField;

typedef struct {
    char*  id;             // action id emitted to the script (heap-owned)
    char*  label;          // button caption (heap-owned)
    bool   primary;        // render as the accent-filled button
} UiDialogButton;

typedef struct {
    bool            active;         // true while a dialog is on screen
    char*           key;            // script-visible id (heap-owned)
    char*           title;          // heading (heap-owned)
    char*           message;        // body text (heap-owned)

    UiDialogField*  fields;         // dynamic array of text fields
    int             field_count;
    int             field_cap;

    UiDialogButton* buttons;        // dynamic array of buttons
    int             button_count;
    int             button_cap;

    int             hover_btn;      // hovered button index, -1 = none
    int             active_btn;     // pressed button index, -1 = none
    int             focused_field;  // focused field index, -1 = none
    double          blink_phase;    // caret blink phase

    double          anim_t;         // 0 = hidden, 1 = fully visible
    bool            closing;        // fading out toward anim_t == 0
    int             pending_btn;    // button chosen at close time
    double          open_started_at;// wall-clock seconds when fade-in was anchored, 0 = unset
} UiDialog;

// dialog geometry, recomputed every frame
typedef struct {
    int bx, by, bw, bh;             // dialog box
    int tx, ty;                     // title baseline
    int mx, my;                     // message origin (first baseline)
    int field_y;                    // absolute y of the first field row
    int field_row_h;                // total height of one field row
    int input_w;                    // width of the input widgets
    int button_y;                   // y of the button row
    int button_h;                   // height of a button
    int btn_x[16];                  // per-button x
    int btn_w[16];                  // per-button width
    int btn_count;                  // number of live buttons
} UiDialogGeom;

// dialog box metrics, shared with ui_render.c
#define DLG_PAD        26
#define DLG_RADIUS      8
#define DLG_TITLE_FS   20
#define DLG_BODY_FS    13
#define DLG_LABEL_FS   11
#define DLG_BTN_H      32
#define DLG_BTN_PAD    16
#define DLG_BTN_GAP    10
#define DLG_INPUT_H    32
#define DLG_WIDTH     460
#define DLG_LINE_H     21
#define DLG_OPEN_SEC   0.10
#define DLG_CLOSE_SEC  0.10

// one live notification popup with its own native window
typedef struct UiNotification {
    bool      active;             // true while on screen
    char*     key;                // heap-owned script key, or null
    char*     title;              // heap-owned title, or null
    char*     message;            // heap-owned body, or null
    bool      show_close;         // draw the close button

    UiWindow* window;             // owned native popup window
    int       win_w, win_h;       // popup client size

    double    timeout;            // seconds until auto close, 0 = manual only
    double    elapsed;            // seconds since shown
    double    anim_t;             // 0..1 open/close phase
    bool      closing;            // fading out
    double    anim_started_at;    // wall-clock seconds the current fade phase began

    bool      hover_close;        // pointer over the close button
    bool      press_close;        // held on the close button

    int       slot;               // vertical stack slot, 0 = bottom-most
    int       work_x, work_y;     // cached monitor work area top-left
    int       work_w, work_h;     // cached monitor work area size
    int       last_x, last_y;     // last placed screen position, avoid server churn
    struct UiContext* ctx;        // back-pointer for the paint hook
} UiNotification;

// one block of the per-frame string arena
typedef struct UiArenaBlock {
    struct UiArenaBlock* next;   // older block, or null
    int  used;                   // bytes consumed in this block
    int  cap;                    // usable capacity of data[]
    char data[];                 // flexible array: payload follows
} UiArenaBlock;

typedef struct UiContext {
    VM*       vm;               // owner vm, used for interning and refcounts
    UiWindow* window;           // native window handle, null until init
    uint32_t* pixels;           // framebuffer pointer for the painter
    int       win_w, win_h;     // current window size in pixels
    char      title[128];       // mirror of the current window title
    bool      custom_chrome;    // true when we draw our own titlebar
    int       tb_hot;           // hovered titlebar button: 0 none, 1 min, 2 max, 3 close
    int       tb_active;        // pressed titlebar button, same encoding

    Widget* pool;               // bump-allocated widget storage for this frame
    int     pool_cap;           // allocated capacity of the widget pool
    int     pool_used;          // number of widgets allocated this frame

    UiArenaBlock* arena;        // current (newest) arena block, null when empty

    Value last_tree;            // last widget descriptor passed to render()
    bool  has_tree;             // true when last_tree holds a live reference
    Value key_cache[UK_COUNT];  // borrowed interned field names

    // hash-guard for skipping the full tree rebuild when the descriptor tree hasn't changed
    uint64_t last_descr_hash;   // recursive hash of the descriptor tree last built
    Value    hashed_tree;       // the Value that last_descr_hash was computed for
    bool     descr_hash_valid;  // false until the first successful rebuild

    Widget* root;               // root of the current widget tree

    InputState input;           // current pointer and modifier state
    uint64_t   focused_id;      // widget id with keyboard focus, 0 for none
    uint64_t   drag_widget_id;  // widget currently being dragged (slider or input)
    uint64_t   hovered_id;      // widget id currently hovered, 0 = none; invariant:

    // a titlebar press that landed while the window was fullscreen
    bool       pending_fullscreen_drag;
    int        pending_drag_x, pending_drag_y;  // pointer position at press

    HitGrid    hit_grid;        // uniform grid for hit testing, rebuilt by ui_layout
    bool       hit_grid_valid;  // false after pool reset; set true by hit_grid_build
    uint32_t   geom_gen;        // bumped by arrange whenever any widget box changes

    // double-click detection
    double     last_click_time; // wall clock of the previous click
    uint64_t   last_click_id;   // widget id of the previous click

    StateMap   state;           // persistent per-widget state across frames
    EventQueue queue;           // pending events awaiting the script

    UiDialog   dialog;          // active modal dialog, kind = ui_dlg_none when idle

    UiFont*    font;            // loaded font used for text rendering
    int        font_size;       // font size in pixels
    int        title_end_x;     // right edge of the rendered window title

    bool       redraw_needed;   // set when the tree must be re-laid out and painted
    bool       quit_requested;  // set when the ui should shut down
    bool       geometry_dirty;  // true when ui_layout must run before the next paint
    bool       full_repaint;    // next frame must repaint the whole window, ignoring damage diff

    SnapshotTable snap_cur;     // widgets laid out this frame
    SnapshotTable snap_prev;    // widgets laid out in the previous frame
    DamageRect*   damage;       // pending damage rects for this repaint
    int           damage_count;
    int           damage_cap;

    // reused id->index map for the damage diff, rebuilt every frame from snap_prev
    int*     snap_prev_index;     // hash slot -> index into snap_prev->entries
    int      snap_prev_index_cap; // power of two, or 0 before first use
    uint8_t* snap_prev_seen;      // parallel to snap_prev->entries
    int      snap_prev_seen_cap;

    // persistent text measurement cache
    TextMeasureEntry* text_cache;         // open-addressed (hash, width) slots
    int               text_cache_count;   // live entries
    int               text_cache_cap;     // power of two, 0 until first use
    uint32_t          text_cache_frame;   // monotonic frame counter, bumped by ui_widgets_reset
    uint32_t          text_cache_swept;   // frame when the last age sweep ran

    // persistent wrap cache: (size, wrap width, text) -> line ranges
    WrapCacheEntry*   wrap_cache;         // linear array of live entries
    int               wrap_cache_count;   // live entries
    int               wrap_cache_cap;     // allocated capacity of the array

    // live notification popups
    UiNotification**  notifs;             // array of owned notification pointers
    int               notif_count;        // live entries
    int               notif_cap;          // allocated capacity of the array

    UiMenuBar*        menubar;            // top-of-window menu bar, or null
} UiContext;                    // per-vm ui session state

// reads a string field from a table, returns a heap copy or null on miss
char* table_get_str_dup(VM* vm, Table* t, const char* key);

// reads a number field from a table, returns def on miss or wrong type
double table_get_num(VM* vm, Table* t, const char* key, double def);

// reads a boolean field from a table, returns def on miss or wrong type
bool table_get_bool(VM* vm, Table* t, const char* key, bool def);

// releases every heap allocation owned by the dialog and resets it
void dialog_clear(UiDialog* d);

// begin / append / commit pair used by ui_module.c to fill the dialog
void dialog_begin(UiContext* ctx, const char* key, const char* title, const char* message);
void dialog_add_field(UiContext* ctx, const char* field_key, const char* label,
                      const char* placeholder, const char* def);
void dialog_add_button(UiContext* ctx, const char* id, const char* label, bool primary);
void dialog_commit(UiContext* ctx);

// fills g with the current dialog's geometry; safe when !active
void ui_dialog_geom(UiContext* ctx, UiDialogGeom* g);

// lazily create the ui context on the vm
UiContext* ui_ensure(VM* vm);

// release every ui-owned allocation
void ui_context_destroy(UiContext* ctx);

// allocate a zeroed widget from the pool
Widget* ui_widget_alloc(UiContext* ctx);

// memoized wrapper around ui_text_measure; safe on null font (returns 0)
float ui_text_measure_cached(UiContext* ctx, const char* s, int len, float size);

// bump-allocate n bytes from the per-frame arena; valid until ui_widgets_reset
void* ui_arena_alloc(UiContext* ctx, size_t n);

// free per-widget heap allocations and rewind the pool
void ui_widgets_reset(UiContext* ctx);

// recursively materialize a descriptor table
Widget* ui_build_tree(UiContext* ctx, Value descr, Widget* parent, int idx);

// compute geometry for the whole tree
void ui_layout(UiContext* ctx);

// draw the custom titlebar into the current framebuffer
void ui_render_titlebar(UiContext* ctx);

// draw the tree into the current framebuffer
void ui_render_frame(UiContext* ctx);

// advance animated state by dt seconds
void ui_tick(UiContext* ctx, double dt);

// fetch or create per-widget persistent state
WidgetState* ui_state_get(UiContext* ctx, uint64_t id);

// enqueue an event
void ui_event_push(UiContext* ctx, const UiEvent* ev);

// digest of every visual field of a widget, used for damage detection
uint64_t ui_visual_hash(Widget* w);

// recursively capture the laid-out widget tree into the current snapshot
void ui_snapshot_capture(UiContext* ctx, Widget* root);

// rebuild the uniform hit-test grid from the laid-out tree. call after ui_layout().
void hit_grid_build(UiContext* ctx, Widget* root);

// release every hit-grid allocation owned by the context.
void hit_grid_free(UiContext* ctx);

// reset the pending damage list before a fresh diff
void ui_damage_begin(UiContext* ctx);

// add one rectangle to the pending damage list, clamped to the window
void ui_damage_add(UiContext* ctx, int x, int y, int w, int h);

// diff the previous snapshot against the current tree, filling the damage list
void ui_damage_diff(UiContext* ctx);

// collapse overlapping damage rectangles into their union
void ui_damage_finalize(UiContext* ctx);

// swap current and previous snapshots so the next frame diffs against this one
void ui_damage_swap(UiContext* ctx);

// release every damage-tracking allocation owned by the context
void ui_damage_free(UiContext* ctx);

// drop wrap cache entries whose last_frame is older than the cutoff
void ui_wrap_cache_sweep(UiContext* ctx, uint32_t cutoff);

// release every wrap cache entry owned by the context
void ui_wrap_cache_free(UiContext* ctx);

// damage-aware render
void ui_render_frame_damage(UiContext* ctx, const DamageRect* rects, int n);

// convert an event into an apex table
Value ui_event_to_table(VM* vm, const UiEvent* ev);

// derive a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx);

// bind the painter to a framebuffer
void ui_painter_init(uint32_t* pixels, int w, int h);

// filled axis-aligned rectangle
void ui_painter_fill_rect(int x, int y, int w, int h, uint32_t c);

// full-surface clear to an opaque color, using non-temporal stores
void ui_painter_clear(uint32_t c);

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

// clears the whole clip stack
void ui_painter_reset_clip(void);

// releases the corner-coverage cache owned by the painter
void ui_painter_shutdown(void);

// show a new notification popup; key/title/message may be null
void ui_notification_show(UiContext* ctx, const char* key, const char* title,
                          const char* message, double timeout, bool show_close);

// advance every notification's animation and auto-close timers
void ui_notification_tick(UiContext* ctx, double dt);

// close a notification by index, emitting its dialog-style event
void ui_notification_close(UiContext* ctx, int index, bool emit);

// release every live notification and its native popup
void ui_notification_destroy(UiContext* ctx);

// pump events coming from every notification popup window
void ui_notification_pump(UiContext* ctx);

// paint one notification into its own framebuffer and blit
void ui_render_notification(UiNotification* n);

// install or replace the menu bar from a descriptor table
void ui_menubar_set(UiContext* ctx, Value descr);

// release the menu bar and every owned string
void ui_menubar_free(UiContext* ctx);

// height reserved at the top of the client area; zero, the bar lives in the titlebar
int ui_menubar_height(UiContext* ctx);

// total horizontal space the bar would like, for reserving room in the titlebar
int ui_menubar_preferred_width(UiContext* ctx);

// true while a dropdown is expanded
bool ui_menubar_has_open(UiContext* ctx);

// paint the top bar inside the titlebar and any open dropdowns below it
void ui_menubar_render(UiContext* ctx);

// pointer moved: returns true when the bar absorbed the event
bool ui_menubar_handle_mousemove(UiContext* ctx, int x, int y);

// pointer pressed: returns true when the bar consumed the click
bool ui_menubar_handle_mousedown(UiContext* ctx, int x, int y);

// escape pressed: returns true when a dropdown was closed
bool ui_menubar_handle_escape(UiContext* ctx);

#endif
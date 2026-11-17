# Apex UI Library Reference (27.03)
The `ui` module lets you build native desktop applications in Apex: a real OS window, real widgets, real menus, dialogs, notifications and file pickers — no browser, no HTML, no external toolkit. Everything is drawn by Apex UI itself straight into a framebuffer, on top of a thin per-platform window backend.

This document assumes you already know Apex (tables, functions, `if`, `for`, `import`) but have never used a UI API before. It first teaches the model, then lists every function with its arguments, table keys, return values and events.

**Important:** like the rest of the standard library, most `ui` functions return `none` on error. The exceptions are called out explicitly (`ui.init` returns a boolean, `ui.fullscreen` returns a boolean, pickers return a path or `none`). A missing field on any options/event table reads as `none`, and every option has a sensible default — you only pass the keys you care about.

## Table of Contents
### Concepts
- [What the ui Module Is](#what-the-ui-module-is)
- [Hello World](#hello-world)
- [The Declarative Model](#the-declarative-model)
- [Rebuilding the UI](#rebuilding-the-ui)
- [The Event Loop](#the-event-loop)
- [Events](#events)
- [Widget Options](#widget-options)
- [Layout and Containers](#layout-and-containers)
- [State and Widget Identity](#state-and-widget-identity)

### Window
- [ui.init(opts?)](#uiinitopts)
- [ui.shutdown()](#uishutdown)
- [ui.set_title(title)](#uiset_titletitle)
- [ui.window_size()](#uiwindow_size)
- [ui.fullscreen(enable?)](#uifullscreenenable)

### Rendering and the Loop
- [ui.render(tree?)](#uirendertree)
- [ui.next_event()](#uinext_event)
- [ui.run(callback)](#uiruncallback)

### Widgets
- [ui.text(content, opts?)](#uitextcontent-opts)
- [ui.heading(content, opts?)](#uiheadingcontent-opts)
- [ui.badge(content, opts?)](#uibadgecontent-opts)
- [ui.button(label, opts?)](#uibuttonlabel-opts)
- [ui.checkbox(label, opts?)](#uicheckboxlabel-opts)
- [ui.input(placeholder?, opts?)](#uiinputplaceholder-opts)
- [ui.text_area(placeholder?, opts?)](#uitext_areaplaceholder-opts)
- [ui.number_input(opts?)](#uinumber_inputopts)
- [ui.slider(opts?)](#uislideropts)
- [ui.dropdown(items, opts?)](#uidropdownitems-opts)
- [ui.progress(value, opts?)](#uiprogressvalue-opts)
- [ui.separator(opts?)](#uiseparatoropts)
- [ui.spacer(opts?)](#uispaceropts)
- [ui.table(desc, opts?)](#uitabledesc-opts)
- [ui.tabs(tabs, opts?)](#uitabstabs-opts)
- [ui.statusbar(content, opts?)](#uistatusbarcontent-opts)

### Containers
- [ui.vbox(children, opts?)](#uivboxchildren-opts)
- [ui.hbox(children, opts?)](#uihboxchildren-opts)
- [ui.panel(children, opts?)](#uipanelchildren-opts)
- [ui.vscroll(children, opts?)](#uivscrollchildren-opts)
- [ui.hscroll(children, opts?)](#uihscrollchildren-opts)
- [ui.scroll(children, opts?)](#uiscrollchildren-opts)

### Menus
- [ui.menubar(items)](#uimenubaritems)
- [ui.context_menu(items, x?, y?)](#uicontext_menuitems-x-y)

### Dialogs and Notifications
- [ui.dialog(desc)](#uidialogdesc)
- [ui.notification(desc)](#uinotificationdesc)

### File Pickers
- [ui.open_file(opts?)](#uiopen_fileopts)
- [ui.open_folder(opts?)](#uiopen_folderopts)
- [ui.save_as(content, opts?)](#uisave_ascontent-opts)

### Keyboard
- [ui.key()](#uikey)
- [Key Constants](#key-constants)

### Appendix
- [A Complete Application](#a-complete-application)
- [Platform Notes](#platform-notes)
- [Performance Notes](#performance-notes)

# Concepts
## What the ui Module Is
Start with `import ui`. The module gives you three kinds of functions:

1. **Window control** — open/close the window, set its title, query its size, toggle fullscreen.
2. **Widget constructors** — `ui.button`, `ui.text`, `ui.vbox`, … Each one *returns a plain Apex table* (a *descriptor*) describing a widget. Constructors do not draw anything and do not open anything; they just build data.
3. **The loop** — `ui.render` hands a descriptor tree to the renderer, and `ui.run` / `ui.next_event` deliver user input back to you as event tables.

There is exactly one window per script. The flow is always the same:

```
ui.init()  →  build a tree of descriptors  →  ui.render(tree)  →  process events  →  rebuild + render again
```

Because descriptors are ordinary tables, you build UI with the same language features you use everywhere else: functions, tables, string interpolation, `for ... in`. There is no special UI syntax.

## Hello World
Create `hello.apex`:

```apex
import ui

function on_event(event)
    return none

ui.init()
ui.render(ui.text("Hello, World!"))
ui.run(on_event)
```

Run it:

```
apex hello.apex
```

A native window opens showing the text. Line by line:

- `ui.init()` opens the window with default options and returns `true` on success.
- `ui.text("Hello, World!")` builds a one-widget descriptor tree.
- `ui.render(...)` records that tree and paints it.
- `ui.run(on_event)` drives the OS message loop, calling `on_event` for every user event, and returns when the user closes the window. The callback returns `none` here because the UI never changes.

That is a complete, runnable Apex UI program. Everything below elaborates on these four calls.

## The Declarative Model
You never mutate widgets. There is no `button.set_label(...)` anywhere in the API. Instead:

- A **descriptor** is a table produced by a constructor. It says *what* the UI should look like right now.
- `ui.render(tree)` tells the library: "make the window match this tree."
- When your data changes, you build a **new tree** and render it again.

Think of it as `view = f(state)`. Your application state lives in your own variables (a table is a good place for it); the tree is a pure description derived from that state. This is why the API is so small: the only things you can do are *describe* and *react*.

```apex
import ui

state = []
state.count = 0

function opts(id)
    o = []
    o.id = id
    return o

function view()
    n = state.count
    return ui.vbox([
        ui.heading("Counter"),
        ui.text("Clicked {n} times"),
        ui.button("Add one", opts("inc"))
    ])

function on_event(ev)
    if ev.type == "click" and ev.id == "inc"
        state.count = state.count + 1
        return view()
    return none

ui.init()
ui.render(view())
ui.run(on_event)
```

Note the pattern: state is a table, so functions can mutate its fields without rebinding any variable. `view()` reads the state and returns a fresh tree; the event handler updates the state and returns the new tree. Returning a tree from the `ui.run` callback is equivalent to calling `ui.render` on it.

## Rebuilding the UI
Rebuilding the whole tree on every event sounds wasteful, but it is the intended workflow, for two reasons:

1. **Damage tracking.** After each render, Apex UI snapshots the widget tree and diffs it against the previous frame. Only the rectangles that actually changed are repainted. Rebuilding a tree where one label changed repaints that label, not the window.
2. **Persistent widget state.** Things the user is in the middle of — focus, caret position, input text, scroll offsets, animation clocks — are kept *per widget* inside the library, keyed by a stable widget identity (see [State and Widget Identity](#state-and-widget-identity)). A rebuilt widget with the same identity keeps its state. Your descriptors do not need to carry the text the user is typing.

So the rule of thumb is: **rebuild freely, keep `id`s stable.** Return a new tree (or call `ui.render`) whenever your state changes; return `none` when nothing visual changed (for example, while the user types into an input whose value you are merely recording).

`ui.render()` with no argument clears the tree — the window stays open but shows no widgets.

## The Event Loop
There are two ways to run the loop. Pick one.

**1. `ui.run(callback)` — recommended.** The library owns the loop. It pumps OS messages, batches events, calls your function once per event, and repaints at most once per batch. If your callback returns a widget tree, that tree takes over rendering; if it returns `none`, the current tree stays. `ui.run` returns when the window closes.

```apex
function on_event(ev)
    // react, optionally return a new tree
    return none

ui.run(on_event)
```

**2. `ui.next_event()` — manual.** You own the loop. Each call blocks until the next event and returns it as a table. Use a conditional `for` loop, and stop when the user closes the window:

```apex
import ui

ui.init()
ui.render(ui.text("Close me to exit"))

for true
    ev = ui.next_event()
    if ev == none
        break
    if ev.type == "close"
        break
    // handle ev here; call ui.render(...) again if the UI changed

ui.shutdown()
```

Use the manual loop when you need to interleave other work between events; use `ui.run` for everything else. Do not mix them.

## Events
Every event is a table. The fields below are the union across all event kinds — a given event only carries the fields relevant to it, and the rest read as `none`.

| Key         | Type   | Meaning                                                                                                       |
|-------------|--------|---------------------------------------------------------------------------------------------------------------|
| `type`      | string | Event kind: `"click"`, `"change"`, `"key"`, `"scroll"`, `"close"`, `"resize"`                                 |
| `id`        | string | The `id` option of the widget that produced the event (or the `key` of a menu/dialog item)                    |
| `value`     | any    | New value for `"change"` events — string for inputs, number for sliders/number inputs, boolean for checkboxes |
| `x`         | number | Mouse x position (click/scroll events)                                                                        |
| `y`         | number | Mouse y position (click/scroll events)                                                                        |
| `button`    | number | Mouse button number (click events)                                                                            |
| `key_code`  | number | Key code (key events) — compare against [ui.key()](#uikey) constants                                          |
| `mods`      | number | Modifier bitmask (key events): shift = 1, ctrl = 2, alt = 4                                                   |
| `text`      | string | UTF-8 text the key produced, if any (empty for non-printable keys)                                            |
| `scroll_dy` | number | Wheel delta (scroll events)                                                                                   |

The core kinds:

- **`click`** — a button was pressed, or a menu item was chosen. Identify the source by `id`.
- **`change`** — a widget's value was edited (input, text area, number input, slider, checkbox, dropdown). The new value is in `value`.
- **`key`** — keyboard input. Use `key_code` + `mods` for shortcuts, `text` for typed characters.
- **`scroll`** — mouse wheel. `scroll_dy` is the vertical delta.
- **`close`** — the user asked to close the window. In a manual loop, break and call `ui.shutdown()`. `ui.run` handles this for you.
- **`resize`** — the client area changed size. Query the new size with `ui.window_size()`.

Widgets without an `id` still emit events, but you cannot tell them apart — give every interactive widget an `id`.

## Widget Options
Every widget constructor takes an optional trailing **options table**. These keys are shared by all widgets:

| Key         | Type    | Default | Meaning                                                                                                    |
|-------------|---------|---------|------------------------------------------------------------------------------------------------------------|
| `id`        | string  | none    | Stable identity. Reported back as `ev.id` on events, and used to preserve per-widget state across rebuilds |
| `css_class` | string  | none    | A styling tag carried on the descriptor                                                                    |
| `width`     | number  | auto    | Explicit pixel width; `0` or omitted means "compute from content/layout"                                   |
| `height`    | number  | auto    | Explicit pixel height; `0` or omitted means auto                                                           |
| `disabled`  | boolean | `false` | Greyed out and ignores input                                                                               |
| `checked`   | boolean | `false` | Initial state (checkbox)                                                                                   |

Build options like any other table:

```apex
o = []
o.id = "save"
o.width = 120
o.disabled = false
btn = ui.button("Save", o)
```

Individual widgets accept a few extra keys in the same table (`min`/`max`/`value` for sliders, and so on) — documented in each widget's section below.

## Layout and Containers
Containers are widgets whose content is a **table of child descriptors**:

- `ui.vbox([...])` stacks children vertically.
- `ui.hbox([...])` lays children out horizontally.
- `ui.panel([...])` groups children visually.
- `ui.vscroll([...])` / `ui.hscroll([...])` add vertical / horizontal scrolling.
- `ui.spacer()` inserts flexible empty space — the standard trick to push siblings apart.

The layout engine sizes everything automatically: text widgets measure their text, containers distribute space, and explicit `width`/`height` options override the computed size. A typical "toolbar with a right-aligned button" is just:

```apex
ui.hbox([
    ui.text("Status: ready"),
    ui.spacer(),
    ui.button("Run", opts("run"))
])
```

Trees nest arbitrarily: a `vbox` of `hbox`es of panels is normal. Headings get a larger font and extra line height automatically.

## State and Widget Identity
Two kinds of state exist, and keeping them straight is the key to using this API well:

1. **Application state** — your variables. The library never touches these. You read them in `view()` and write them in event handlers.
2. **Widget state** — focus, caret, input buffers, scroll position, hover/animation clocks. The library owns these and persists them across rebuilds.

A widget's identity is derived from its position in the tree plus its `id` (or its index among siblings when no `id` is given). Consequences:

- Give interactive widgets an explicit, stable `id`. It survives rebuilds, so the user's caret/scroll/focus survive too.
- If you reorder children or change ids, treat the affected widgets as new — their internal state resets.
- You do not need to feed an input's current text back into its descriptor on every keystroke; the widget already holds it. Record `ev.value` into your state and return `none`.

# Window
## ui.init(opts?)
Opens the native window if it is not already open. Calling it again while the window is open is safe. Returns `true` on success, `false` on failure.

`opts` is an optional table:

| Key          | Type    | Default     | Meaning                                                                                 |
|--------------|---------|-------------|-----------------------------------------------------------------------------------------|
| `title`      | string  | `"Apex UI"` | Window title (also drawn in the custom titlebar)                                        |
| `width`      | number  | built-in    | Initial client width in pixels                                                          |
| `height`     | number  | built-in    | Initial client height in pixels                                                         |
| `min_width`  | number  | built-in    | Minimum width the user can resize to                                                    |
| `min_height` | number  | built-in    | Minimum height the user can resize to                                                   |
| `chrome`     | boolean | —           | Draw the custom Apex titlebar instead of the native frame. Always forced off on Android |

```apex
import ui

o = []
o.title = "My App"
o.width = 800
o.height = 600
ok = ui.init(o)
if ok == false
    // window could not be created
```

## ui.shutdown()
Closes the native window. The UI context stays alive, so you can call `ui.init()` again later to open a fresh window. Returns `none`. In a `ui.run` app the loop has already ended by the time you would call this; it is mainly for manual loops.

## ui.set_title(title)
Sets the window title (and the custom titlebar text) to the string `title`. Returns `none`.

```apex
ui.set_title("notes.txt — My App")
```

## ui.window_size()
Returns the current client area size as a table with numeric keys `w` and `h`, in pixels. Returns `none` if there is no window.

```apex
size = ui.window_size()
width = size.w
height = size.h
```

## ui.fullscreen(enable?)
Controls fullscreen. With a boolean argument, sets fullscreen to that state; with no argument, toggles it. Returns the **resulting** fullscreen state as a boolean.

```apex
state = ui.fullscreen()        // toggle
state = ui.fullscreen(true)    // force on
state = ui.fullscreen(false)   // force off
```

# Rendering and the Loop
## ui.render(tree?)
Records `tree` (a widget descriptor, usually a container) as the current widget tree and schedules a repaint. Returns `none`. With no argument (or a non-table), clears the tree. This is the "make the window look like this" call; see [Rebuilding the UI](#rebuilding-the-ui).

```apex
ui.render(ui.vbox([
    ui.heading("Settings"),
    ui.checkbox("Enable feature", opts("feature"))
]))
```

## ui.next_event()
Blocks until the next UI event is available and returns it as a table (see [Events](#events)). Returns `none` when there is no window. Use it inside your own loop; see [The Event Loop](#the-event-loop).

## ui.run(callback)
Drives the message loop with batched repaints until the window closes, then returns. `callback` is a function value taking one argument (the event table). Its return value controls rendering:

- return a **widget tree** → it takes over rendering (same as `ui.render(tree)`);
- return **`none`** → the current tree is kept.

```apex
function on_event(ev)
    if ev.type == "click" and ev.id == "quit"
        ui.shutdown()
    return none

ui.run(on_event)
```

# Widgets
All constructors return a descriptor table and accept the shared [options table](#widget-options) as their last argument. They never draw or block.

## ui.text(content, opts?)
A line (or wrapped block) of body text. `content` is a string; interpolation makes it the natural way to display state. Emits no events.

```apex
ui.text("Hello, {name}")
```

## ui.heading(content, opts?)
A line of heading text — larger font and extra vertical room compared to `ui.text`. Emits no events.

```apex
ui.heading("Dashboard")
```

## ui.badge(content, opts?)
A small badge/tag for short status labels ("NEW", "3", "BETA"). Emits no events.

```apex
ui.badge("BETA")
```

## ui.button(label, opts?)

A push button with the text `label`. Emits **`click`** with `id` set to the button's `id` option.

```apex
ui.button("Save", opts("save"))
```

## ui.checkbox(label, opts?)
A checkbox with a text label. Set the initial state with `opts.checked = true`. Emits **`change`**; `ev.value` is the new checked state (boolean).

```apex
o = opts("dark")
o.checked = true
ui.checkbox("Dark mode", o)
```

## ui.input(placeholder?, opts?)
A single-line text field. `placeholder` is the grey hint shown while empty. Emits **`change`** as the user edits; `ev.value` is the current string. The widget keeps its own buffer and caret across rebuilds — record the value, return `none`, and do not rebuild on every keystroke.

```apex
ui.input("Search...", opts("search"))
```

## ui.text_area(placeholder?, opts?)
A multi-line text editor. Same contract as `ui.input`: **`change`** events with the full text in `ev.value`; internal buffer, line cache and caret are widget state.

```apex
ui.text_area("Write something...", opts("editor"))
```

## ui.number_input(opts?)
A numeric entry control. Configure it through the options table (`value`, `min`, `max`). Emits **`change`** with a number in `ev.value`.

```apex
o = opts("port")
o.value = 8080
o.min = 1
o.max = 65535
ui.number_input(o)
```

## ui.slider(opts?)
A draggable range control. Options: `value` (current position), `min`, `max`. Emits **`change`** with the new number in `ev.value`.

```apex
o = opts("volume")
o.min = 0
o.max = 100
o.value = 30
ui.slider(o)
```

## ui.dropdown(items, opts?)
A drop-down selector. `items` is a table of option strings. Emits **`change`** when the selection changes; `ev.value` carries the selection.

```apex
ui.dropdown(["Small", "Medium", "Large"], opts("size"))
```

## ui.progress(value, opts?)
A progress bar. `value` is a number describing completion (a fraction from 0 to 1). Emits no events; rebuild with a new value to advance it.

```apex
ui.progress(0.5)
```

## ui.separator(opts?)
A visual dividing line between sections. Emits no events.

```apex
ui.vbox([ui.text("Above"), ui.separator(), ui.text("Below")])
```

## ui.spacer(opts?)
Flexible empty space. With no explicit size it expands to fill available room, pushing siblings apart; pin it with `width`/`height` for a fixed gap. Emits no events.

```apex
ui.hbox([ui.text("left"), ui.spacer(), ui.text("right")])
```

## ui.table(desc, opts?)
A data table widget. `desc` describes the column headers (`columns`, a table of strings) and the row data (`rows`, a table of row tables).

```apex
d = []
d.columns = ["Name", "Size"]
d.rows = [
    ["main.apex", 120],
    ["utils.apex", 80]
]
ui.table(d, opts("files"))
```

## ui.tabs(tabs, opts?)
A tabbed container. `tabs` is a table of tab descriptors; each pairs a `label` with the widget tree (`items`) shown while that tab is active.

```apex
general = []
general.label = "General"
general.items = [ui.text("General settings")]

advanced = []
advanced.label = "Advanced"
advanced.items = [ui.checkbox("Experimental mode", opts("exp"))]

ui.tabs([general, advanced])
```

## ui.statusbar(content, opts?)
A status bar strip, typically placed at the bottom of the root container. `content` is the text to display.

```apex
ui.vbox([
    ui.text("Document body"),
    ui.spacer(),
    ui.statusbar("Ready")
])
```

# Containers
Containers take a **table of child descriptors** plus the shared options table. Children are laid out automatically; see [Layout and Containers](#layout-and-containers).

## ui.vbox(children, opts?)
Vertical stack — children top to bottom.

```apex
ui.vbox([ui.heading("Title"), ui.text("Body"), ui.button("OK", opts("ok"))])
```

## ui.hbox(children, opts?)
Horizontal row — children left to right.

```apex
ui.hbox([ui.button("Back", opts("back")), ui.button("Next", opts("next"))])
```

## ui.panel(children, opts?)
A grouped/bordered container for visually sectioning content.

```apex
ui.panel([ui.heading("Account"), ui.input("Username", opts("user"))])
```

## ui.vscroll(children, opts?)
A vertically scrolling container. Scroll position is widget state and survives rebuilds with a stable `id`.

## ui.hscroll(children, opts?)
A horizontally scrolling container.

## ui.scroll(children, opts?)
The vertical scrolling container under its generic name — same behavior as `ui.vscroll`.

```apex
ui.vscroll([
    ui.text("line one"),
    ui.text("line two"),
    ui.text("line three")
], opts("log"))
```

# Menus
Menu items are plain tables with these keys:

| Key     | Type   | Meaning                                                              |
|---------|--------|----------------------------------------------------------------------|
| `label` | string | Caption shown to the user                                            |
| `key`   | string | Script-visible identity — arrives as `ev.id` when the item is chosen |
| `items` | table  | Optional submenu: a nested table of items (recursive)                |

A helper keeps menu construction readable:

```apex
function menu_item(label, key)
    it = []
    it.label = label
    it.key = key
    return it
```

## ui.menubar(items)
Installs the window menu bar. `items` is a table of top-level menus, each with a `label` and an `items` array. Choosing any item emits an event whose `id` is that item's `key`. Returns `none`.

```apex
file_menu = []
file_menu.label = "File"
file_menu.items = [
    menu_item("Open...", "open"),
    menu_item("Save", "save"),
    menu_item("Quit", "quit")
]

ui.menubar([file_menu])
```

Handle selections in your event callback by `id`:

```apex
function on_event(ev)
    if ev.id == "open"
        path = ui.open_file()
    if ev.id == "quit"
        ui.shutdown()
    return none
```

## ui.context_menu(items, x?, y?)
Opens a context (right-click style) menu. With only `items`, it opens at the current cursor position; with `x` and `y` (numbers), it opens at those window coordinates — for example the `ev.x` / `ev.y` of a click. Items use the same format as the menubar, including nested `items` submenus. Choosing an item emits an event with the item's `key` as `id`. Returns `none`.

```apex
ui.context_menu([
    menu_item("Cut", "cut"),
    menu_item("Copy", "copy"),
    menu_item("Paste", "paste")
])
```

# Dialogs and Notifications
## ui.dialog(desc)
Shows a modal dialog. `desc` keys:

| Key       | Type   | Meaning                                                                        |
|-----------|--------|--------------------------------------------------------------------------------|
| `key`     | string | Script-visible identity for the dialog, reported back through the event stream |
| `title`   | string | Heading text                                                                   |
| `message` | string | Body text                                                                      |

Returns `none` immediately; the dialog's outcome is reported as an event identified by its `key`.

```apex
d = []
d.key = "confirm_delete"
d.title = "Delete file?"
d.message = "This action cannot be undone."
ui.dialog(d)
```

## ui.notification(desc)
Shows a desktop notification popup. `desc` keys:

| Key       | Type    | Default | Meaning                                   |
|-----------|---------|---------|-------------------------------------------|
| `key`     | string  | none    | Script-visible identity                   |
| `title`   | string  | `""`    | Heading text                              |
| `message` | string  | `""`    | Body text                                 |
| `timeout` | number  | `4.0`   | Seconds before the popup dismisses itself |
| `close`   | boolean | `true`  | Show a close button on the popup          |

Returns `none`.

```apex
n = []
n.title = "Saved"
n.message = "Your document was written to disk."
n.timeout = 3
ui.notification(n)
```

# File Pickers
All three pickers are native: the common item dialog on Windows, `NSOpenPanel`/save panel on macOS, `zenity`/`kdialog` on Linux (one of them must be installed). They block until the user chooses or cancels. On Android they are not supported.

## ui.open_file(opts?)
Opens the native "open file" picker. `opts` may carry a `title` (string) for the dialog. Returns the chosen file path as a string, or `none` if the user cancelled.

```apex
path = ui.open_file()
if path != none
    ui.set_title(path)
```

## ui.open_folder(opts?)
Opens the native "open folder" picker. Same contract: optional `title` in `opts`, returns the chosen directory path or `none`.

```apex
folder = ui.open_folder()
```

## ui.save_as(content, opts?)
Opens the native "save file" picker and writes the string `content` to the chosen path. `opts` may carry a `title`. Returns `true` when the content was written; `none` on cancel or failure.

```apex
ok = ui.save_as("file contents here")
if ok == true
    ui.notification(...)
```

# Keyboard
## ui.key()
Returns a table of named key and modifier constants. Compare `ev.key_code` against these values instead of hardcoding numbers, and test `ev.mods` against the modifier constants.

```apex
k = ui.key()

function on_event(ev)
    if ev.type == "key"
        if ev.key_code == k.f5
            // refresh
        if ev.mods == k.ctrl and ev.text == "s"
            // save
    return none
```

Printable keys report their character codepoint as `key_code` (space is 32), and the produced UTF-8 text is in `ev.text` — for shortcuts use `key_code` + `mods`, for typed characters use `text`.

## Key Constants
Modifiers are a bitmask; combined modifiers add up (ctrl+shift = 3).

| Name         | Value                           |          |
|--------------|---------------------------------|----------|
| `shift`      | 1                               | modifier |
| `ctrl`       | 2                               | modifier |
| `alt`        | 4                               | modifier |
| `space`      | 32 (0x20)                       |          |
| `home`       | 65360 (0xFF50)                  |          |
| `left`       | 65361 (0xFF51)                  |          |
| `up`         | 65362 (0xFF52)                  |          |
| `right`      | 65363 (0xFF53)                  |          |
| `down`       | 65364 (0xFF54)                  |          |
| `page_up`    | 65365 (0xFF55)                  |          |
| `page_down`  | 65366 (0xFF56)                  |          |
| `end`        | 65367 (0xFF57)                  |          |
| `insert`     | 65379 (0xFF63)                  |          |
| `f1` … `f12` | 65470 … 65481 (0xFFBE … 0xFFC9) |          |

# Appendix
## A Complete Application
A small notes editor tying everything together: menubar, text area, statusbar, Ctrl+S shortcut, file pickers and a notification.

```apex
import ui

state = []
state.text = ""

function opts(id)
    o = []
    o.id = id
    return o

function menu_item(label, key)
    it = []
    it.label = label
    it.key = key
    return it

function install_menu()
    file_menu = []
    file_menu.label = "File"
    file_menu.items = [
        menu_item("Open...", "open"),
        menu_item("Save", "save"),
        menu_item("Quit", "quit")
    ]
    ui.menubar([file_menu])

function view()
    return ui.vbox([
        ui.heading("Notes"),
        ui.text_area("Start typing...", opts("editor")),
        ui.statusbar("Ready")
    ])

function save()
    ok = ui.save_as(state.text)
    if ok == true
        n = []
        n.title = "Saved"
        n.message = "Your notes were written to disk."
        ui.notification(n)

function on_event(ev)
    if ev.type == "change" and ev.id == "editor"
        state.text = ev.value
        return none
    if ev.type == "key"
        k = ui.key()
        if ev.mods == k.ctrl and ev.text == "s"
            save()
            return none
    if ev.id == "open"
        path = ui.open_file()
        if path != none
            ui.set_title(path)
        return none
    if ev.id == "save"
        save()
        return none
    if ev.id == "quit"
        ui.shutdown()
        return none
    return none

ui.init()
install_menu()
ui.render(view())
ui.run(on_event)
```

Bundle it into a standalone executable with `apex build notes.apex` — the UI library is linked in and the bytecode is embedded, so the result runs on its own.

## Platform Notes
- **Windows** — Win32 backend; native common-item-dialog file pickers.
- **macOS** — Cocoa backend; `NSOpenPanel` / save-panel pickers.
- **Linux** — X11 backend; file pickers shell out to `zenity` or `kdialog`, so one of them must be installed; fonts are discovered via fontconfig.
- **Android** — supported by the window backend; the custom titlebar (`chrome`) is always off, and the file/folder pickers are not available.

## Performance Notes
- **Damage tracking** — each frame's widget snapshot is diffed against the previous one; only changed rectangles are repainted. Rebuild the whole tree without worry.
- **SIMD painting** — the row blenders and rectangle fillers use AVX2 / SSE2 / NEON fast paths with a scalar fallback; text is rasterized by the built-in TrueType blitter.
- **No external runtime** — no FreeType, HarfBuzz, ICU, GTK, Qt or SDL in the dependency graph. The only platform libraries are the OS's own windowing APIs.

Happy coding in Apex!
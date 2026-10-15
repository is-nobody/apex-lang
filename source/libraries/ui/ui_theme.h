// source/libraries/ui/ui_theme.h
// Color and metric constants for the Apex ui theme
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_THEME_H
#define UI_THEME_H
#include <stdint.h>

#define UI_C_BG         0xFF1E1E1Eu    // window background
#define UI_C_BAR        0xFF181818u    // title/menu bar background
#define UI_C_PANEL      0xFF252526u    // panel surface
#define UI_C_PANEL_ALT  0xFF2A2D2Eu    // panel surface, hovered

#define UI_C_FG         0xFFD4D4D4u    // primary text color
#define UI_C_FG_DIM     0xFF969696u    // secondary text color
#define UI_C_FG_MUTED   0xFF6E6E6Eu    // muted text color

#define UI_C_ACCENT     0xFF3A3D41u    // accent fill
#define UI_C_ACCENT_H   0xFF4A4D51u    // accent fill, hovered
#define UI_C_ACCENT_A   0xFF2A2D2Eu    // accent fill, active/pressed
#define UI_C_ACCENT_T   0xFFE8E8E8u    // text drawn on top of an accent fill

#define UI_C_TEAL       0xFF4EC9B0u    // semantic teal
#define UI_C_YELLOW     0xFFDCDCAAu    // semantic yellow
#define UI_C_PURPLE     0xFFC586C0u    // semantic purple
#define UI_C_ORANGE     0xFFCE9178u    // semantic orange
#define UI_C_RED        0xFFF48771u    // semantic red

#define UI_C_BORDER     0xFF3C3C3Cu    // default border
#define UI_C_BORDER_S   0xFF454545u    // strong border
#define UI_C_FOCUS      0xFF6E6E6Eu    // keyboard focus ring

#define UI_C_DIS_BG     0xFF2A2D2Eu    // disabled background
#define UI_C_DIS_FG     0xFF6E6E6Eu    // disabled foreground
#define UI_C_INPUT_BG   0xFF252526u    // input field background
#define UI_C_SEL_BG     0xFF3E3E42u    // text selection background

#define UI_C_SWITCH_OFF    0xFF252526u    // switch track, off: input field grey
#define UI_C_SWITCH_OFF_H  0xFF2A2D2Eu    // switch track, off, hovered: panel alt
#define UI_C_SWITCH_ON     0xFF4A4D51u    // switch track, on: accent hover grey
#define UI_C_SWITCH_ON_H   0xFF5A5D61u    // switch track, on, hovered: one step brighter
#define UI_C_SWITCH_KNOB   0xFFD4D4D4u    // knob fill: theme foreground
#define UI_C_SWITCH_KNOB_D 0xFF6E6E6Eu    // knob fill, disabled: theme muted

#define UI_C_SCROLL_T   0x00000000u    // scrollbar track (transparent)
#define UI_C_SCROLL_H   0x66797979u    // scrollbar thumb
#define UI_C_SCROLL_A   0x80B4B4B4u    // scrollbar thumb, active

#define UI_FS_BODY      14             // body text font size
#define UI_FS_H1        18             // h1 font size
#define UI_FS_H2        16             // h2 font size
#define UI_FS_SMALL     12             // small font size
#define UI_LINE_H       21             // body line height in pixels

#define UI_PAD_SCREEN   14             // screen padding in pixels
#define UI_PAD_PANEL    12             // panel padding in pixels
#define UI_GAP          8              // default gap between siblings
#define UI_RADIUS       6              // default corner radius
#define UI_RADIUS_SM    4              // small corner radius
#define UI_BORDER       1              // default border width
#define UI_BTN_H        30             // button height in pixels
#define UI_INPUT_H      30             // input field height in pixels
#define UI_CB_SIZE      16             // checkbox/radio box size
#define UI_TABLE_ROW_H    28           // table row height in pixels
#define UI_TABLE_CELL_PAD 10           // horizontal padding inside a cell
#define UI_SCROLL_W     10             // scrollbar width in pixels
#define UI_FOCUS_W      2              // focus ring width in pixels

#define UI_LIST_ITEM_H    32           // list row height in pixels
#define UI_LIST_ITEM_PAD  12           // horizontal padding inside a list row

#define UI_SWITCH_W       40           // switch track width in pixels
#define UI_SWITCH_H       22           // switch track height in pixels
#define UI_SWITCH_PAD     3            // gap between the knob and the track edge
#define UI_SWITCH_ANIM_SEC 0.25         // knob ease duration in seconds

#define UI_TITLEBAR_H     35           // custom titlebar height in pixels
#define UI_TB_RESIZE      5            // resize border thickness in pixels

#define UI_TB_BTN_SZ      28           // titlebar button size in pixels
#define UI_TB_BTN_GAP     4            // gap between titlebar buttons
#define UI_TB_BTN_PAD     8            // right padding of the button group

#define UI_C_TB_BG        0xFF181818u  // titlebar background
#define UI_C_TB_FG        0xFFCCCCCCu  // titlebar title text
#define UI_C_TB_BTN       0xFF9D9D9Du  // titlebar button icon
#define UI_C_TB_BTN_HOV   0xFF3A3D41u  // titlebar button hover background
#define UI_C_TB_BTN_ACT   0xFF4A4D51u  // titlebar button active background

#define UI_MIN_W   150                 // default minimum window width in pixels
#define UI_MIN_H   34                  // default minimum window height in pixels

#define UI_KEY_ESCAPE     0xFF1B  // 65307
#define UI_KEY_ENTER      0xFF0D  // 65293
#define UI_KEY_TAB        0xFF09  // 65289
#define UI_KEY_BACKSPACE  0xFF08  // 65288
#define UI_KEY_DELETE     0xFFFF  // 65535
#define UI_KEY_INSERT     0xFF63  // 65379
#define UI_KEY_HOME       0xFF50  // 65360
#define UI_KEY_END        0xFF57  // 65367
#define UI_KEY_PAGE_UP    0xFF55  // 65365
#define UI_KEY_PAGE_DOWN  0xFF56  // 65366
#define UI_KEY_LEFT       0xFF51  // 65361
#define UI_KEY_RIGHT      0xFF53  // 65363
#define UI_KEY_UP         0xFF52  // 65362
#define UI_KEY_DOWN       0xFF54  // 65364
#define UI_KEY_SPACE      0x20    // 32
#define UI_KEY_F1         0xFFBE  // 65470
#define UI_KEY_F2         0xFFBF
#define UI_KEY_F3         0xFFC0
#define UI_KEY_F4         0xFFC1
#define UI_KEY_F5         0xFFC2
#define UI_KEY_F6         0xFFC3
#define UI_KEY_F7         0xFFC4
#define UI_KEY_F8         0xFFC5
#define UI_KEY_F9         0xFFC6
#define UI_KEY_F10        0xFFC7
#define UI_KEY_F11        0xFFC8
#define UI_KEY_F12        0xFFC9

#define UI_MOD_SHIFT      1       // shift held
#define UI_MOD_CTRL       2       // control held
#define UI_MOD_ALT        4       // alt / option held

// notification popup
#define UI_NOTIF_W          340   // default notification width
#define UI_NOTIF_PAD        14    // inner padding
#define UI_NOTIF_RADIUS     10    // corner radius
#define UI_NOTIF_TITLE_FS   14    // title font size
#define UI_NOTIF_BODY_FS    13    // body font size
#define UI_NOTIF_LINE_H     18    // body line height
#define UI_NOTIF_CLOSE_SZ   20    // close button hitbox size
#define UI_NOTIF_MARGIN     18    // distance from screen edge
#define UI_NOTIF_GAP        10    // gap between stacked notifications
#define UI_NOTIF_MAX        6     // maximum live notifications
#define UI_NOTIF_OPEN_SEC   0.11  // slide-in duration
#define UI_NOTIF_CLOSE_SEC  0.08  // slide-out duration
#define UI_NOTIF_SLIDE_MAX  60    // pixels the popup slides in from the right

#define UI_C_NOTIF_BG        0xFF2A2D2Eu
#define UI_C_NOTIF_BORDER    0xFF454545u
#define UI_C_NOTIF_TITLE     0xFFE8E8E8u
#define UI_C_NOTIF_BODY      0xFFB0B0B0u
#define UI_C_NOTIF_CLOSE     0xFF9D9D9Du
#define UI_C_NOTIF_CLOSE_HOV 0xFF4A4D51u

// menu bar metrics
#define UI_MENUBAR_H             UI_TITLEBAR_H  // bar shares the titlebar height
#define UI_MENU_BAR_PAD          8              // lead-in from the window's left edge
#define UI_MENU_BAR_ITEM_PAD     10             // horizontal padding inside a top label
#define UI_MENU_BAR_GAP          2              // gap between top labels
#define UI_MENU_BAR_TITLE_GAP    16             // gap between the title text and the bar
#define UI_MENU_ITEM_H           26             // dropdown item row height
#define UI_MENU_SEP_H            9              // separator row height
#define UI_MENU_DROP_PAD         6              // vertical padding of a dropdown panel
#define UI_MENU_ITEM_PAD         12             // horizontal padding inside a dropdown item
#define UI_MENU_SHORTCUT_GAP     24             // gap between a row label and its right-aligned shortcut
#define UI_MENU_DROP_W_MIN       140            // minimum dropdown width

// soft, modern dropdown metrics (menubar only)
#define UI_MENU_DROP_RADIUS      6              // outer panel corner radius
#define UI_MENU_ITEM_RADIUS      5              // top-bar chip corner radius
#define UI_MENU_ROW_RADIUS       5              // menubar dropdown item highlight radius
#define UI_MENU_ITEM_INSET_X     4              // menubar highlight inset from panel side edges
#define UI_MENU_ITEM_INSET_Y     1              // menubar highlight inset from row top/bottom

// menu bar colors
#define UI_C_MENUBAR_BG        UI_C_TB_BG       // matches the titlebar strip
#define UI_C_MENUBAR_FG        UI_C_TB_FG       // matches the titlebar text
#define UI_C_MENU_HOVER_BG     UI_C_TB_BTN_HOV  // top-bar chip hover
#define UI_C_MENU_DROP_BG      UI_C_TB_BG       // dropdown panel fill, matches the tray
#define UI_C_MENU_DROP_BORDER  UI_C_BORDER      // dropdown outline
#define UI_C_MENU_SEP          UI_C_BORDER      // separator line
#define UI_C_MENU_ITEM_FG      UI_C_TB_FG       // dropdown item text, matches the tray

// tree colors
#define UI_TREE_ROW_H      28   // tree row height in pixels
#define UI_TREE_INDENT     16   // default pixels per indentation level
#define UI_TREE_INNER_PAD   6   // left inset of the first indent slot
#define UI_TREE_DISC_W     14   // width of the disclosure triangle's click slot

// tab strip metrics
#define UI_TABS_H          35   // tab strip height
#define UI_TAB_MIN_W       40   // minimum tab width
#define UI_TAB_MAX_W      170   // maximum tab width (~20 characters of body text)
#define UI_TAB_PAD_X        8   // left inset before the label
#define UI_TAB_PAD_R        4   // right inset after the close button
#define UI_TAB_LABEL_GAP    6   // gap between the label and the close button
#define UI_TAB_GAP          2   // gap between tabs
#define UI_TAB_RADIUS       6   // tab corner radius
#define UI_TAB_CLOSE_SZ    16   // close glyph hitbox size
#define UI_TAB_ADD_SZ      28   // "+" button square size
#define UI_TAB_DRAG_THRESH  4   // pixels moved before a press becomes a drag

// tab strip colors
#define UI_C_TABS_BG       0xFF181818u  // tab bar strip background
#define UI_C_TAB_BG        0xFF1E1E1Eu  // inactive tab fill
#define UI_C_TAB_BG_HOV    0xFF2A2D2Eu  // hovered tab fill
#define UI_C_TAB_BG_ACT    0xFF2A2D2Eu  // active tab fill
#define UI_C_TAB_FG        0xFF969696u  // inactive label
#define UI_C_TAB_FG_HOV    0xFFCCCCCCu  // hovered label
#define UI_C_TAB_FG_ACT    0xFFFFFFFFu  // active label
#define UI_C_TAB_CLOSE     0xFF969696u  // close glyph
#define UI_C_TAB_CLOSE_HOV 0xFFFFFFFFu  // close glyph, hovered
#define UI_C_TAB_CLOSE_BG  0xFF3A3D41u  // close button hover chip
#define UI_C_TAB_DRAG_BG   0xFF3A3D41u  // tab being dragged

#endif
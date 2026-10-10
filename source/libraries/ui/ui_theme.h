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
#define UI_SCROLL_W     10             // scrollbar width in pixels
#define UI_FOCUS_W      2              // focus ring width in pixels

#endif
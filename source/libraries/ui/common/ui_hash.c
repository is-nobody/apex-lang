// source/libraries/ui/common/ui_hash.c
// Hash utilities for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_hash.h"
#include <string.h>

// fnv-1a over a byte range;
uint64_t hash_bytes64(uint64_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;                              // byte cursor over the range
    for (size_t i = 0; i < n; i++) {                                   // one step per byte
        h ^= (uint64_t)b[i];                                           // xor next byte into state
        h *= 0x100000001B3ULL;                                         // multiply by fnv-1a prime
    }
    return h;                                                          // running digest
}

// how many leading/trailing bytes of a string field are folded into the visual hash
#define VIS_HASH_EDGE 16

// hashes a string field by (length, first vis_hash_edge bytes, last vis_hash_edge bytes)
uint64_t hash_str_sampled(uint64_t h, const char* s, int len) {
    h = hash_bytes64(h, &len, sizeof(len));               // length: catches inserts/deletes outright
    if (!s || len <= 0) return h;                         // empty: nothing else to fold in

    int edge = VIS_HASH_EDGE;
    if (len <= edge * 2) {                                // short string: hash it whole
        return hash_bytes64(h, s, (size_t)len);
    }
    h = hash_bytes64(h, s, (size_t)edge);                 // leading window
    h = hash_bytes64(h, s + len - edge, (size_t)edge);    // trailing window
    return h;
}

// fnv-1a hash of parent id + key (or index) to produce a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx) {
    uint64_t h = 0xCBF29CE484222325ULL ^ parent;     // fold parent id into the seed
    if (key) {
        while (*key) { h ^= (unsigned char)*key++; h *= 0x100000001B3ULL; }  // hash each key byte
    } else {
        h ^= (uint64_t)(idx + 1);                    // no key: mix in 1-based positional index
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1;                                // never return 0, id 0 is reserved for "none"
}

// same hash as ui_hash_id but reads from a length-tagged view
uint64_t ui_hash_id_view(uint64_t parent, const StrView* kv, int idx) {
    if (!kv) return ui_hash_id(parent, NULL, idx);
    uint64_t h = 0xCBF29CE484222325ULL ^ parent;
    for (int i = 0; i < kv->len; i++) {
        h ^= (unsigned char)kv->ptr[i];
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1;
}

// hash of (size, length, content) used as the text measurement cache key
uint64_t text_measure_hash(const char* s, int len, float size) {
    uint64_t h = 0xCBF29CE484222325ULL;               // fnv-1a offset basis
    h = hash_bytes64(h, &size, sizeof(size));         // fold font size first
    h = hash_bytes64(h, &len,  sizeof(len));          // then the length
    if (len > 0) h = hash_bytes64(h, s, (size_t)len); // then the payload bytes
    return h ? h : 1;                                 // 0 is reserved for empty slots
}

// mixes a 64-bit widget id into a 32-bit slot hash
uint32_t state_hash(uint64_t id) {
    uint64_t h = id * 0x9E3779B97F4A7C15ULL;   // 64-bit golden-ratio step
    return (uint32_t)(h ^ (h >> 32));          // fold high half down
}

// digest of every field that affects how a widget renders
uint64_t ui_visual_hash(Widget* w) {
    uint64_t nv_bits, sy_bits, ch_bits;
    uint32_t sk_bits;
    memcpy(&nv_bits, &w->num_value, sizeof(nv_bits));   // bit-exact, no aliasing UB
    memcpy(&sy_bits, &w->scroll_y,  sizeof(sy_bits));
    memcpy(&ch_bits, &w->content_h, sizeof(ch_bits));
    memcpy(&sk_bits, &w->switch_knob, sizeof(sk_bits)); // wk_switch knob is animated live

    // fingerprint covers every field below that can change between rebuilds
    uint64_t fp = 0xCBF29CE484222325ULL;
    fp ^= (uint64_t)(uint32_t)w->hovered    <<  0;
    fp ^= (uint64_t)(uint32_t)(w->hover_row + 1) << 11;   // +1 so -1 doesn't collapse to 0
    fp ^= (uint64_t)(uint32_t)w->pressed    <<  1;
    fp ^= (uint64_t)(uint32_t)w->focused    <<  2;
    fp ^= (uint64_t)(uint32_t)w->cursor     <<  3;
    fp ^= (uint64_t)(uint32_t)w->sel_anchor << 23;
    fp ^= (uint64_t)(uint32_t)w->combo_open     <<  5;
    fp ^= (uint64_t)(uint32_t)w->combo_selected <<  6;
    fp ^= (uint64_t)(uint32_t)(w->tab_hover_idx + 1) <<  7;  // +1 so -1 doesn't collapse
    fp ^= (uint64_t)(uint32_t)w->tab_hover_close <<  8;
    fp ^= (uint64_t)(uint32_t)w->tab_add_hover   <<  9;
    fp ^= (uint64_t)(uint32_t)w->tab_add_press   << 10;
    fp ^= (uint64_t)(uint32_t)w->tab_press_close << 24;
    fp ^= (uint64_t)(uint32_t)(w->tab_drag_idx + 1)   << 25;  // +1 so -1 doesn't collapse
    fp ^= (uint64_t)(uint32_t)(w->tab_drag_target + 1) << 27;
    fp ^= nv_bits;
    fp ^= sy_bits;
    fp ^= ch_bits;
    fp ^= (uint64_t)sk_bits << 32;                      // knob bits, high half
    fp ^= (uint64_t)(uintptr_t)w->input_value.ptr;
    fp ^= (uint64_t)(uint32_t)w->input_value.len << 43;

    if (fp == w->visual_hash_fp && w->visual_hash_cache != 0)
        return w->visual_hash_cache;                                       // state unchanged

    uint64_t h = 0xCBF29CE484222325ULL;                                    // fnv-1a offset basis
    h = hash_bytes64(h, &w->kind,            sizeof(w->kind));             // widget kind
    h = hash_bytes64(h, &w->visible,         sizeof(w->visible));          // visibility flag
    h = hash_bytes64(h, &w->disabled,        sizeof(w->disabled));         // disabled flag
    h = hash_bytes64(h, &w->hovered,         sizeof(w->hovered));          // hover flag
    h = hash_bytes64(h, &w->hover_row,       sizeof(w->hover_row));        // tree hovered row
    h = hash_bytes64(h, &w->pressed,         sizeof(w->pressed));          // pressed flag
    h = hash_bytes64(h, &w->focused,         sizeof(w->focused));          // focus flag
    h = hash_bytes64(h, &w->checked,         sizeof(w->checked));          // checkbox/radio state
    h = hash_bytes64(h, &w->cursor,          sizeof(w->cursor));           // input caret position
    h = hash_bytes64(h, &w->sel_anchor,      sizeof(w->sel_anchor));       // input selection anchor
    h = hash_bytes64(h, &w->num_value,       sizeof(w->num_value));        // slider/progress value
    h = hash_bytes64(h, &w->scroll_y,        sizeof(w->scroll_y));         // scroll offset
    h = hash_bytes64(h, &w->content_h,       sizeof(w->content_h));        // scroll content height
    h = hash_bytes64(h, &w->switch_knob,     sizeof(w->switch_knob));      // wk_switch knob position
    h = hash_bytes64(h, &w->combo_open,      sizeof(w->combo_open));       // dropdown expanded
    h = hash_bytes64(h, &w->combo_selected,  sizeof(w->combo_selected));   // selected option index
    h = hash_bytes64(h, &w->tab_hover_idx,   sizeof(w->tab_hover_idx));    // hovered tab
    h = hash_bytes64(h, &w->tab_hover_close, sizeof(w->tab_hover_close));  // hover is on the hovered tab's close button
    h = hash_bytes64(h, &w->tab_add_hover,   sizeof(w->tab_add_hover));    // "+" button hovered
    h = hash_bytes64(h, &w->tab_add_press,   sizeof(w->tab_add_press));    // "+" button pressed
    h = hash_bytes64(h, &w->tab_press_close, sizeof(w->tab_press_close));  // press was on a tab's close button
    h = hash_bytes64(h, &w->tab_drag_idx,    sizeof(w->tab_drag_idx));     // tab being dragged, -1 for none
    h = hash_bytes64(h, &w->tab_drag_target, sizeof(w->tab_drag_target));  // target slot during a drag

    // str fields carry a fresh heap copy each rebuild
    h = hash_str_sampled(h, w->text.ptr,        w->text.len);          // label/content
    h = hash_str_sampled(h, w->input_value.ptr, w->input_value.len);   // input text
    h = hash_str_sampled(h, w->group.ptr,       w->group.len);         // radio group name
    h = hash_str_sampled(h, w->combo_value.ptr, w->combo_value.len);

    w->visual_hash_fp    = fp;                                         // publish the memo
    w->visual_hash_cache = h;
    return h;                                                          // final widget digest
}
// source/libraries/ui/common/ui_hash.h
// Hash utilities for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#ifndef UI_HASH_H
#define UI_HASH_H

#include "ui_internal.h"

// fnv-1a over a byte range; the running digest is passed in and returned
uint64_t hash_bytes64(uint64_t h, const void* p, size_t n);

// hashes a string field by (length, first vis_hash_edge bytes, last vis_hash_edge bytes)
uint64_t hash_str_sampled(uint64_t h, const char* s, int len);

// fnv-1a hash of parent id + key (or index) to produce a stable widget id
uint64_t ui_hash_id(uint64_t parent, const char* key, int idx);

// same hash as ui_hash_id but reads from a length-tagged view
uint64_t ui_hash_id_view(uint64_t parent, const StrView* kv, int idx);

// digest of every field that affects how a widget renders
uint64_t ui_visual_hash(Widget* w);

// hash of (size, length, content) used as the text measurement cache key
uint64_t text_measure_hash(const char* s, int len, float size);

// mixes a 64-bit widget id into a 32-bit slot hash
uint32_t state_hash(uint64_t id);

#endif
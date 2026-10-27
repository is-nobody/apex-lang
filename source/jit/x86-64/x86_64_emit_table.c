// source/jit/x86-64/x86_64_emit_table.c
// Table opcode emitters for the x86-64 JIT loop body
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include <stddef.h>

#define APEX_SO_HASH_COMPUTED ((int32_t)offsetof(StringObject, hash_computed))
#define APEX_SO_HASH          ((int32_t)offsetof(StringObject, hash))
#define APEX_TBL_CAPACITY     ((int32_t)offsetof(Table, capacity))
#define APEX_TBL_ENTRIES      ((int32_t)offsetof(Table, entries))
#define APEX_TE_HASH          ((int32_t)offsetof(TableEntry, hash))
#define APEX_TE_KEY           ((int32_t)offsetof(TableEntry, key))
#define APEX_TE_VALUE         ((int32_t)offsetof(TableEntry, value))
#define APEX_TE_NEXT          ((int32_t)offsetof(TableEntry, next))

// emit jcc rel32 with a placeholder; returns the offset of the rel32 field
static size_t emit_jcc32_placeholder(CodeBuf* cb, uint8_t op2) {
    emit_u8(cb, 0x0F); emit_u8(cb, op2);                             // jcc rel32 opcode
    size_t at = cb->len;                                             // offset of the rel32 field
    emit_i32(cb, 0);                                                 // placeholder, patched later
    return at;
}

// emit jmp rel32 with a placeholder; returns the offset of the rel32 field
static size_t emit_jmp32_placeholder(CodeBuf* cb) {
    emit_u8(cb, 0xE9);                                               // jmp rel32 opcode
    size_t at = cb->len;                                             // offset of the rel32 field
    emit_i32(cb, 0);                                                 // placeholder, patched later
    return at;
}

// patch a previously emitted rel32 placeholder to point at `target`
static void patch_rel32(CodeBuf* cb, size_t at, size_t target) {
    int32_t rel = (int32_t)target - (int32_t)(at + 4);               // rel32 distance
    memcpy(cb->buf + at, &rel, 4);                                   // write rel32
}

// OP_TABLE_GET_KEY_STR: d = tbl["prefix" .. num]
static void loop_table_get_key_str(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for TABLE_GET_KEY_STR (pc=%d)", st->pc);
    x86_cache_flush(st->cache, cb);                                  // spill before the helper call

    int table_reg  = st->a;                                          // table register slot
    int prefix_idx = (int)((uint32_t)st->b >> 16);                   // packed prefix constant index
    int num_reg    = st->b & 0xFFFF;                                 // packed number register

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save caller frame pointer
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));    // rax = regs[table_reg]
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
#if defined(_WIN32) || defined(_WIN64)
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);         // win64: rcx = table*
#else
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);         // sysv: rdi = table*
#endif
    uint64_t prefix_addr = (uint64_t)(uintptr_t)st->chunk->constants[prefix_idx].cached_str;
    x86_emit_movabs_rax(cb, prefix_addr);                            // rax = &prefix
#if defined(_WIN32) || defined(_WIN64)
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);         // win64: rdx = prefix
#else
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);         // sysv: rsi = prefix
#endif
    x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));              // xmm0 = num (sysv position)
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_sse66_rr(cb, 0x28, 2, 0);                               // win64: xmm2 = num
#endif
    uint64_t helper = (uint64_t)(uintptr_t)&jit_table_get_key_str;   // helper address
    x86_emit_movabs_rax(cb, helper);                                 // rax = helper
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(st->d));       // regs[d] = returned value
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // restore frame_reg
    x86_cache_clear(st->cache);                                      // callee clobbered all xmm
}

// OP_TABLE_SET_KEY_STR: tbl["prefix" .. num] = val
static void loop_table_set_key_str(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for TABLE_SET_KEY_STR (pc=%d)", st->pc);
    x86_cache_flush(st->cache, cb);                                  // spill before the helper call

    int table_reg  = st->d;                                          // table register slot
    int val_reg    = st->a;                                          // value register slot
    int prefix_idx = (int)((uint32_t)st->b >> 16);                   // packed prefix constant index
    int num_reg    = st->b & 0xFFFF;                                 // packed number register

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save caller frame pointer
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));    // rax = regs[table_reg]
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
#if defined(_WIN32) || defined(_WIN64)
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);         // win64: rcx = table*
#else
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);         // sysv: rdi = table*
#endif
    uint64_t prefix_addr = (uint64_t)(uintptr_t)st->chunk->constants[prefix_idx].cached_str;
    x86_emit_movabs_rax(cb, prefix_addr);                            // rax = &prefix
#if defined(_WIN32) || defined(_WIN64)
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);         // win64: rdx = prefix
#else
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);         // sysv: rsi = prefix
#endif
    x86_emit_movsd_load(cb, 0, x86_slot_disp(num_reg));              // xmm0 = num (sysv position)
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_sse66_rr(cb, 0x28, 2, 0);                               // win64: xmm2 = num
#endif
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(val_reg));      // rax = regs[val_reg]
#if defined(_WIN32) || defined(_WIN64)
    emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);         // win64: r9 = value
#else
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);         // sysv: rdx = value
#endif
    uint64_t helper = (uint64_t)(uintptr_t)&jit_table_set_key_str;   // helper address
    x86_emit_movabs_rax(cb, helper);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // restore frame_reg
    x86_cache_clear(st->cache);                                      // callee clobbered all xmm
}

// OP_TABLE_GET / OP_TABLE_GET_NUM
static void loop_table_get(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    JitLoopInfo* info = st->info;                                    // loop being emitted
    int table_reg = st->a;                                           // table register slot
    int key_reg   = st->b;                                           // key register slot

    int cg = -1;                                                     // cached array_part gpr
    for (int ci = 0; ci < info->n_cached_tables; ci++)               // direct cached-table hit?
        if (info->cached_table_slot[ci] == table_reg) {
            cg = info->cached_table_gpr[ci]; break;
        }
    if (cg < 0) {                                                    // derived-invariant table base?
        for (int ci = 0; ci < info->n_derived_caches; ci++)
            if (info->derived_slot[ci] == table_reg) {
                cg = info->derived_gpr[ci]; break;
            }
    }
    if (cg >= 0) {                                                   // cached-table fast path
        if (key_reg == info->for_var_reg && info->for_var_gpr >= 0) {
            int xd = x86_cache_dest_reg(cache, cb, st->d, -1, -1);   // use gpr counter directly
            if (xd < 0) JIT_FATAL("cache full: TABLE_GET(gpr-key) d=%d (pc=%d)", st->d, st->pc);
            x86_emit_movsd_load_idx8_bx(cb, xd, cg, info->for_var_gpr);
            x86_cache_put(cache, xd, st->d);
            return;
        }
        int xk = cache->slot_reg[key_reg];                           // key already in cache?
        if (xk < 0) {
            xk = x86_cache_load(cache, cb, key_reg);                 // load key from frame
            if (xk < 0) JIT_FATAL("cache full: TABLE_GET(cached) key=%d (pc=%d)", key_reg, st->pc);
        }
        x86_emit_cvttsd2si_eax(cb, xk);                              // eax = (int)key
        x86_emit_dec_eax(cb);                                        // 1-based -> 0-based
        int xd = x86_cache_dest_reg(cache, cb, st->d, xk, -1);       // prefer d's home
        if (xd < 0) JIT_FATAL("cache full: TABLE_GET(cached) d=%d (pc=%d)", st->d, st->pc);
        x86_emit_movsd_load_idx8_bx(cb, xd, cg, X86_RAX);            // xd = array_part[rax]
        x86_cache_put(cache, xd, st->d);
        return;
    }

    if (key_reg == info->for_var_reg && table_reg == info->table.slot) {  // counter-key + primary table
        if (info->for_var_gpr >= 0) {                                // counter lives in gpr, already 0-based
            int xd = x86_cache_dest_reg(cache, cb, st->d, -1, -1);
            if (xd < 0) JIT_FATAL("cache full: TABLE_GET(gpr) d=%d (pc=%d)", st->d, st->pc);
            x86_emit_movsd_load_idx8_bx(cb, xd, X86_RBX, info->for_var_gpr);
            x86_cache_put(cache, xd, st->d);
            return;
        }
        int xk = cache->slot_reg[key_reg];                           // key already in cache?
        if (xk < 0) {
            xk = x86_cache_load(cache, cb, key_reg);
            if (xk < 0) JIT_FATAL("cache full: TABLE_GET(counter) key=%d (pc=%d)", key_reg, st->pc);
        }
        x86_emit_cvttsd2si_eax(cb, xk);                              // eax = (int)key
        x86_emit_dec_eax(cb);                                        // 1-based -> 0-based
        int xd = x86_cache_dest_reg(cache, cb, st->d, xk, -1);
        if (xd < 0) JIT_FATAL("cache full: TABLE_GET(counter) d=%d (pc=%d)", st->d, st->pc);
        x86_emit_movsd_load_idx8(cb, xd, X86_RBX, X86_RAX);          // xd = array_part[rax]
        x86_cache_put(cache, xd, st->d);
        return;
    }

    int xk = cache->slot_reg[key_reg];                               // general path: key in cache?
    if (xk < 0) {
        xk = x86_cache_load(cache, cb, key_reg);
        if (xk < 0) JIT_FATAL("cache full: TABLE_GET key=%d (pc=%d)", key_reg, st->pc);
    }
    int xt = cache->slot_reg[table_reg];                             // table still in an xmm?
    if (xt >= 0) x86_emit_movq_rax_xmm(cb, xt);                      // rax = table bits
    else         x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
    x86_emit_load_r64_base(cb, X86_RAX, X86_RAX,
                            (int32_t)offsetof(Table, array_part));   // rax = array_part
    x86_emit_cvttsd2si_edx(cb, xk);                                  // edx = (int)key
    x86_emit_dec_edx(cb);                                            // 1-based -> 0-based
    int xd = x86_cache_dest_reg(cache, cb, st->d, xk, -1);           // prefer d's home
    if (xd < 0) JIT_FATAL("cache full: TABLE_GET d=%d (pc=%d)", st->d, st->pc);
    x86_emit_movsd_load_idx8(cb, xd, X86_RAX, X86_RDX);              // xd = array_part[rdx]
    x86_cache_put(cache, xd, st->d);
}

// OP_TABLE_GET_INT: constant integer index into array_part
static void loop_table_get_int(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xd = x86_cache_dest_reg(cache, cb, st->d, -1, -1);           // prefer d's home
    if (xd < 0) JIT_FATAL("cache full: TABLE_GET_INT d=%d (pc=%d)", st->d, st->pc);
    x86_emit_movsd_load_base(cb, X86_RBX, xd, (st->b - 1) * 8);      // xd = array_part[b-1]
    x86_cache_put(cache, xd, st->d);
}

// OP_TABLE_GET_CONST: d = tbl[const_str]; inline tier-0 walk + helper fallback
static void loop_table_get_const(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for TABLE_GET_CONST (pc=%d)", st->pc);
    if (cb->len + 2048 > cb->cap)                                    // buffer overflow guard
        JIT_FATAL("code buffer too small for TABLE_GET_CONST (pc=%d)", st->pc);

    int d = st->d;                                                   // dest slot
    int a = st->a;                                                   // table slot
    int b = st->b;                                                   // key constant index
    StringObject* key_str = (StringObject*)st->chunk->constants[b].cached_str;

    int xt = cache->slot_reg[a];                                     // table already cached?
    if (xt >= 0) x86_emit_movq_rax_xmm(cb, xt);                      // rax = raw table value
    else         x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(a));

    x86_emit_movabs_r11(cb, X86_TAG_MASK);                           // r11 = tag mask
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_RAX);                      // rdx = raw value
    emit_u8(cb, 0x4C); emit_u8(cb, 0x21); emit_u8(cb, 0xDA);         // and rdx, r11
    x86_emit_movabs_r11(cb, X86_TABLE_BITS);                         // r11 = table tag
    x86_emit_cmp_r64(cb, X86_RDX, X86_R11);                          // is it a table?
    size_t jne_not_table = emit_jcc32_placeholder(cb, 0x85);         // jne .not_table

    x86_emit_clear_high16_rax(cb);                                   // strip tag: rax = Table*
    x86_emit_movabs_r11(cb, (uint64_t)(uintptr_t)key_str);           // r11 = key pointer
    x86_emit_movzx_r32_m8(cb, X86_RDX, X86_R11, APEX_SO_HASH_COMPUTED);  // rdx = key->hash_computed
    x86_emit_test_r32(cb, X86_RDX, X86_RDX);                         // hash already computed?
    size_t jz_slow_hash = emit_jcc32_placeholder(cb, 0x84);          // jz .slow
    x86_emit_load_r32_base(cb, X86_R8, X86_R11, APEX_SO_HASH);       // r8d = key->hash
    x86_emit_load_r32_base(cb, X86_RDX, X86_RAX, APEX_TBL_CAPACITY); // edx = capacity
    emit_u8(cb, 0x8D); emit_u8(cb, 0x72); emit_u8(cb, 0xFF);         // lea esi, [rdx-1]
    x86_emit_test_r32(cb, X86_RSI, X86_RDX);                         // capacity power-of-two?
    size_t jnz_slow_cap = emit_jcc32_placeholder(cb, 0x85);          // jnz .slow
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_R8);                       // rdx = hash
    x86_emit_and_r32(cb, X86_RDX, X86_RSI);                          // bucket = hash & (cap-1)
    x86_emit_load_r64_base(cb, X86_R9, X86_RAX, APEX_TBL_ENTRIES);   // r9 = entries
    x86_emit_test_r64(cb, X86_R9, X86_R9);                           // entries non-null?
    size_t jz_slow_nohash = emit_jcc32_placeholder(cb, 0x84);        // jz .slow
    x86_emit_load_r64_idx8(cb, X86_R10, X86_R9, X86_RDX);            // r10 = entries[bucket]
    x86_emit_test_r64(cb, X86_R10, X86_R10);                         // bucket non-empty?
    size_t jz_slow_empty = emit_jcc32_placeholder(cb, 0x84);         // jz .slow

    size_t walk_off = cb->len;                                       // .walk label
    x86_emit_load_r32_base(cb, X86_RDX, X86_R10, APEX_TE_HASH);      // rdx = entry->hash
    x86_emit_cmp_r32(cb, X86_RDX, X86_R8);                           // hash match?
    size_t jne_next = emit_jcc32_placeholder(cb, 0x85);              // jne .next
    x86_emit_load_r64_base(cb, X86_RSI, X86_R10, APEX_TE_KEY);       // rsi = entry->key
    x86_emit_clear_high16_rsi(cb);                                   // strip tag
    x86_emit_cmp_r64(cb, X86_RSI, X86_R11);                          // pointer match?
    size_t je_hit = emit_jcc32_placeholder(cb, 0x84);                // je .hit

    patch_rel32(cb, jne_next, cb->len);                              // .next:
    x86_emit_load_r64_base(cb, X86_R10, X86_R10, APEX_TE_NEXT);      // r10 = entry->next
    x86_emit_test_r64(cb, X86_R10, X86_R10);                         // next non-null?
    emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                            // jnz .walk
    { int32_t rel = (int32_t)walk_off - (int32_t)(cb->len + 4);
      emit_i32(cb, rel); }

    size_t slow_off = cb->len;                                       // .slow label
    patch_rel32(cb, jz_slow_hash,   slow_off);
    patch_rel32(cb, jnz_slow_cap,   slow_off);
    patch_rel32(cb, jz_slow_nohash, slow_off);
    patch_rel32(cb, jz_slow_empty,  slow_off);

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64: rcx = table
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_R11);                      // win64: rdx = key
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv: rdi = table
    x86_emit_mov_r64_r64(cb, X86_RSI, X86_R11);                      // sysv: rsi = key
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_table_get_const);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
    size_t jmp_store_from_slow = emit_jmp32_placeholder(cb);         // jmp .store

    size_t not_table_off = cb->len;                                  // .not_table:
    patch_rel32(cb, jne_not_table, not_table_off);
    x86_emit_movabs_rax(cb, X86_NONE_BITS);                          // rax = none
    size_t jmp_store_from_none = emit_jmp32_placeholder(cb);         // jmp .store

    size_t hit_off = cb->len;                                        // .hit:
    patch_rel32(cb, je_hit, hit_off);
    x86_emit_load_r64_base(cb, X86_RAX, X86_R10, APEX_TE_VALUE);     // rax = entry->value
    // fall through to .store

    size_t store_off = cb->len;                                      // .store:
    patch_rel32(cb, jmp_store_from_slow, store_off);
    patch_rel32(cb, jmp_store_from_none, store_off);

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_lea_r64_base(cb, X86_RCX, X86_RBP, x86_slot_disp(d));   // win64: rcx = &dst
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_RAX);                      // win64: rdx = value
#else
    x86_emit_lea_r64_base(cb, X86_RDI, X86_RBP, x86_slot_disp(d));   // sysv: rdi = &dst
    x86_emit_mov_r64_r64(cb, X86_RSI, X86_RAX);                      // sysv: rsi = value
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));

    {                                                                // drop any stale xmm entry
        int dx = cache->slot_reg[d];
        if (dx >= 0) {
            cache->reg_slot[dx] = -1;
            cache->slot_reg[d]  = -1;
            cache->slot_dirty[d] = false;
        }
    }
}

// OP_TABLE_SET / OP_TABLE_SET_NUM
static void loop_table_set(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    JitLoopInfo* info = st->info;                                    // loop being emitted
    int table_reg = st->d;                                           // table register slot
    int key_reg   = st->a;                                           // key register slot
    int val_reg   = st->b;                                           // value register slot

    if (key_reg == info->for_var_reg && table_reg == info->table.slot) {  // counter-key + primary table
        if (info->for_var_gpr >= 0) {                                // counter lives in gpr, already 0-based
            int xv = x86_cache_load_excl(cache, cb, val_reg, -1, -1);
            if (xv < 0) JIT_FATAL("cache full: TABLE_SET(gpr) val=%d (pc=%d)", val_reg, st->pc);
            x86_emit_movsd_store_idx8_bx(cb, X86_RBX, info->for_var_gpr, xv);
            return;
        }
        int xk = cache->slot_reg[key_reg];                           // key already in cache?
        if (xk < 0) {
            xk = x86_cache_load(cache, cb, key_reg);
            if (xk < 0) JIT_FATAL("cache full: TABLE_SET(counter) key=%d (pc=%d)", key_reg, st->pc);
        }
        x86_emit_cvttsd2si_eax(cb, xk);                              // eax = (int)key
        x86_emit_dec_eax(cb);                                        // 1-based -> 0-based
        int xv = x86_cache_load_excl(cache, cb, val_reg, xk, -1);    // load value, keep key
        if (xv < 0) JIT_FATAL("cache full: TABLE_SET(counter) val=%d (pc=%d)", val_reg, st->pc);
        x86_emit_movsd_store_idx8(cb, X86_RBX, X86_RAX, xv);         // array_part[rax] = xv
        return;
    }

    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for TABLE_SET (pc=%d)", st->pc);
    int xk = cache->slot_reg[key_reg];                               // key already in cache?
    if (xk < 0) {
        xk = x86_cache_load(cache, cb, key_reg);
        if (xk < 0) JIT_FATAL("cache full: TABLE_SET key=%d (pc=%d)", key_reg, st->pc);
    }
    int xv = cache->slot_reg[val_reg];                               // value already in cache?
    if (xv < 0) {
        xv = x86_cache_load(cache, cb, val_reg);
        if (xv < 0) JIT_FATAL("cache full: TABLE_SET val=%d (pc=%d)", val_reg, st->pc);
    }

    x86_cache_flush(cache, cb);                                      // spill dirty slots before the call
    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg

#if defined(_WIN32) || defined(_WIN64)
    x86_emit_cvttsd2si_edx(cb, xk);                                  // edx = (int)key
    x86_emit_dec_edx(cb);                                            // 1-based -> 0-based
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));    // rax = regs[table_reg]
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC1);         // win64: rcx = table
    x86_emit_movq_rax_xmm(cb, xv);                                   // rax = value bits
    emit_u8(cb, 0x49); emit_u8(cb, 0x89); emit_u8(cb, 0xC0);         // win64: r8 = value
#else
    x86_emit_cvttsd2si_edx(cb, xk);                                  // edx = (int)key
    x86_emit_dec_edx(cb);                                            // 1-based -> 0-based
    emit_u8(cb, 0x89); emit_u8(cb, 0xD6);                            // sysv: esi = edx
    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(table_reg));    // rax = regs[table_reg]
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC7);         // sysv: rdi = table
    x86_emit_movq_rax_xmm(cb, xv);                                   // rax = value bits
    emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);         // sysv: rdx = value
#endif
    uint64_t addr = (uint64_t)(uintptr_t)&table_set_int;             // helper that grows array_part
    x86_emit_movabs_rax(cb, addr);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // restore frame_reg
    x86_cache_clear(cache);                                          // the call clobbers all xmm
}

// OP_TABLE_SET_INT: constant integer index into array_part
static void loop_table_set_int(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xv = x86_cache_load(cache, cb, st->b);                       // load value to store
    if (xv < 0) JIT_FATAL("cache full: TABLE_SET_INT val=%d (pc=%d)", st->b, st->pc);
    x86_emit_movsd_store_base(cb, X86_RBX, xv, (st->a - 1) * 8);     // array_part[a-1] = xv
}

// OP_TABLE_SET_CONST: tbl[const_str] = val; inline tier-0 walk + helper fallback
static void loop_table_set_const(X86LoopBodyState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    if (st->save_slot < 0)                                           // helper needs a spill slot
        JIT_FATAL("no save slot for TABLE_SET_CONST (pc=%d)", st->pc);
    if (cb->len + 2048 > cb->cap)                                    // buffer overflow guard
        JIT_FATAL("code buffer too small for TABLE_SET_CONST (pc=%d)", st->pc);

    int d = st->d;                                                   // table register slot
    int a = st->a;                                                   // key constant index
    int b = st->b;                                                   // value register slot
    StringObject* key_str = (StringObject*)st->chunk->constants[a].cached_str;

    int xt = cache->slot_reg[d];                                     // table already cached?
    if (xt >= 0) x86_emit_movq_rax_xmm(cb, xt);                      // rax = raw table value
    else         x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(d));

    x86_emit_movabs_r11(cb, X86_TAG_MASK);                           // r11 = tag mask
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_RAX);                      // rdx = raw value
    emit_u8(cb, 0x4C); emit_u8(cb, 0x21); emit_u8(cb, 0xDA);         // and rdx, r11
    x86_emit_movabs_r11(cb, X86_TABLE_BITS);                         // r11 = table tag
    x86_emit_cmp_r64(cb, X86_RDX, X86_R11);                          // is it a table?
    size_t jne_not_table = emit_jcc32_placeholder(cb, 0x85);         // jne .not_table

    x86_emit_clear_high16_rax(cb);                                   // strip tag: rax = Table*
    x86_emit_movabs_r11(cb, (uint64_t)(uintptr_t)key_str);           // r11 = key pointer
    x86_emit_movzx_r32_m8(cb, X86_RDX, X86_R11, APEX_SO_HASH_COMPUTED);  // rdx = key->hash_computed
    x86_emit_test_r32(cb, X86_RDX, X86_RDX);                         // hash already computed?
    size_t jz_slow_hash = emit_jcc32_placeholder(cb, 0x84);          // jz .slow
    x86_emit_load_r32_base(cb, X86_R8, X86_R11, APEX_SO_HASH);       // r8d = key->hash
    x86_emit_load_r32_base(cb, X86_RDX, X86_RAX, APEX_TBL_CAPACITY); // edx = capacity
    emit_u8(cb, 0x8D); emit_u8(cb, 0x72); emit_u8(cb, 0xFF);         // lea esi, [rdx-1]
    x86_emit_test_r32(cb, X86_RSI, X86_RDX);                         // capacity power-of-two?
    size_t jnz_slow_cap = emit_jcc32_placeholder(cb, 0x85);          // jnz .slow
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_R8);                       // rdx = hash
    x86_emit_and_r32(cb, X86_RDX, X86_RSI);                          // bucket = hash & (cap-1)
    x86_emit_load_r64_base(cb, X86_R9, X86_RAX, APEX_TBL_ENTRIES);   // r9 = entries
    x86_emit_test_r64(cb, X86_R9, X86_R9);                           // entries non-null?
    size_t jz_slow_nohash = emit_jcc32_placeholder(cb, 0x84);        // jz .slow
    x86_emit_load_r64_idx8(cb, X86_R10, X86_R9, X86_RDX);            // r10 = entries[bucket]
    x86_emit_test_r64(cb, X86_R10, X86_R10);                         // bucket non-empty?
    size_t jz_slow_empty = emit_jcc32_placeholder(cb, 0x84);         // jz .slow

    size_t walk_off = cb->len;                                       // .walk label
    x86_emit_load_r32_base(cb, X86_RDX, X86_R10, APEX_TE_HASH);      // rdx = entry->hash
    x86_emit_cmp_r32(cb, X86_RDX, X86_R8);                           // hash match?
    size_t jne_next = emit_jcc32_placeholder(cb, 0x85);              // jne .next
    x86_emit_load_r64_base(cb, X86_RSI, X86_R10, APEX_TE_KEY);       // rsi = entry->key
    x86_emit_clear_high16_rsi(cb);                                   // strip tag
    x86_emit_cmp_r64(cb, X86_RSI, X86_R11);                          // pointer match?
    size_t je_hit = emit_jcc32_placeholder(cb, 0x84);                // je .hit

    patch_rel32(cb, jne_next, cb->len);                              // .next:
    x86_emit_load_r64_base(cb, X86_R10, X86_R10, APEX_TE_NEXT);      // r10 = entry->next
    x86_emit_test_r64(cb, X86_R10, X86_R10);                         // next non-null?
    emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                            // jnz .walk
    { int32_t rel = (int32_t)walk_off - (int32_t)(cb->len + 4);
      emit_i32(cb, rel); }

    size_t slow_off = cb->len;                                       // .slow label
    patch_rel32(cb, jz_slow_hash,   slow_off);
    patch_rel32(cb, jnz_slow_cap,   slow_off);
    patch_rel32(cb, jz_slow_nohash, slow_off);
    patch_rel32(cb, jz_slow_empty,  slow_off);

    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));  // save frame_reg
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64: rcx = table
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_R11);                      // win64: rdx = key
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv: rdi = table
    x86_emit_mov_r64_r64(cb, X86_RSI, X86_R11);                      // sysv: rsi = key
#endif
    {
        int xv = cache->slot_reg[b];                                 // value in cache?
#if defined(_WIN32) || defined(_WIN64)
        if (xv >= 0) x86_emit_movq_gpr_xmm(cb, X86_R8, xv);          // win64: r8 = value
        else         x86_emit_load_r64_rbp(cb, X86_R8, x86_slot_disp(b));
#else
        if (xv >= 0) x86_emit_movq_gpr_xmm(cb, X86_RDX, xv);         // sysv: rdx = value
        else         x86_emit_load_r64_rbp(cb, X86_RDX, x86_slot_disp(b));
#endif
    }
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_table_set_const);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
    size_t jmp_done_from_slow = emit_jmp32_placeholder(cb);          // jmp .done

    size_t not_table_off = cb->len;                                  // .not_table:
    patch_rel32(cb, jne_not_table, not_table_off);
    size_t jmp_done_from_none = emit_jmp32_placeholder(cb);          // jmp .done

    size_t hit_off = cb->len;                                        // .hit:
    patch_rel32(cb, je_hit, hit_off);
    x86_emit_store_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_lea_r64_base(cb, X86_RCX, X86_R10, APEX_TE_VALUE);      // win64: rcx = &entry->value
#else
    x86_emit_lea_r64_base(cb, X86_RDI, X86_R10, APEX_TE_VALUE);      // sysv: rdi = &entry->value
#endif
    {
        int xv = cache->slot_reg[b];                                 // value in cache?
#if defined(_WIN32) || defined(_WIN64)
        if (xv >= 0) x86_emit_movq_gpr_xmm(cb, X86_RDX, xv);         // win64: rdx = value
        else         x86_emit_load_r64_rbp(cb, X86_RDX, x86_slot_disp(b));
#else
        if (xv >= 0) x86_emit_movq_gpr_xmm(cb, X86_RSI, xv);         // sysv: rsi = value
        else         x86_emit_load_r64_rbp(cb, X86_RSI, x86_slot_disp(b));
#endif
    }
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_emit_load_r64_rbp(cb, st->abi->frame_reg, x86_slot_disp(st->save_slot));
    size_t jmp_done_from_hit = emit_jmp32_placeholder(cb);           // jmp .done

    size_t done_off = cb->len;                                       // .done:
    patch_rel32(cb, jmp_done_from_slow, done_off);
    patch_rel32(cb, jmp_done_from_none, done_off);
    patch_rel32(cb, jmp_done_from_hit,  done_off);
}

// loop body dispatch for the table category
void x86_loop_emit_table(X86LoopBodyState* st) {
    switch (st->op) {
        case OP_TABLE_GET: case OP_TABLE_GET_NUM: loop_table_get(st);       break;
        case OP_TABLE_GET_INT:                    loop_table_get_int(st);   break;
        case OP_TABLE_GET_CONST:                  loop_table_get_const(st); break;
        case OP_TABLE_SET: case OP_TABLE_SET_NUM: loop_table_set(st);       break;
        case OP_TABLE_SET_INT:                    loop_table_set_int(st);   break;
        case OP_TABLE_SET_CONST:                  loop_table_set_const(st); break;
        case OP_TABLE_GET_KEY_STR:                loop_table_get_key_str(st); break;
        case OP_TABLE_SET_KEY_STR:                loop_table_set_key_str(st); break;
        default:
            JIT_FATAL("loop table category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
}
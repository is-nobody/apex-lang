// source/jit/x86-64/x86_64_emit_match.c
// Match and switch emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include <string.h>

// emits a match jump through the given jcc opcode, recording the fixup against st->d
static void emit_match_jcc(X86FuncEmitState* st, uint8_t jcc) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    emit_u8(cb, 0x0F); emit_u8(cb, jcc);                             // jcc rel32 opcode
    x86_func_jump_fixup(st, cb->len, st->d);                         // record placeholder offset
    emit_i32(cb, 0);                                                 // rel32 placeholder
}

// snapshots or flushes the cache before a match jump to st->d
static void prepare_match_target(X86FuncEmitState* st) {
    if (st->d >= st->start && st->d < st->end) {                     // in-range target
        if (st->needs_flush[st->d - st->start])                      // multi-pred / backward
            x86_cache_flush(st->cache, st->cb);
        else if (st->use_snap[st->d - st->start])                    // unique forward: snapshot
            x86_cache_snap(&st->jump_snap[st->d - st->start], st->cache);
    } else {
        x86_cache_flush(st->cache, st->cb);                          // out-of-range: safest
    }
}

// OP_JUMP_MATCH_NUM: subject bit-exact equality against a pool constant
static void match_num(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load subject
    if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_NUM subj=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    double c = st->chunk->constants[st->b].number_value;             // fetch constant
    uint64_t cbits; memcpy(&cbits, &c, 8);                           // reinterpret as u64
    x86_emit_movabs_rax(cb, cbits);                                  // rax = constant bits
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                          // scratch = constant
    x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);                        // ucomisd subj, const
    prepare_match_target(st);
    emit_u8(cb, 0x0F); emit_u8(cb, 0x8A);                            // jp skip (subj was NaN-tagged)
    size_t skip_patch = cb->len;                                     // skip placeholder offset
    emit_i32(cb, 0);
    emit_match_jcc(st, 0x84);                                        // je target
    int32_t skip_rel = (int32_t)cb->len - (int32_t)(skip_patch + 4);
    memcpy(cb->buf + skip_patch, &skip_rel, 4);                      // patch skip
}

// OP_JUMP_MATCH_STR: call helper, branch on non-zero result
static void match_str(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    x86_cache_flush(cache, cb);                                      // call clobbers xmm
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_load_r64_rbp(cb, X86_RCX, x86_slot_disp(st->a));        // win64: rcx = subject
#else
    x86_emit_load_r64_rbp(cb, X86_RDI, x86_slot_disp(st->a));        // sysv: rdi = subject
#endif
    uint64_t addr = (uint64_t)(uintptr_t)&st->chunk->constants[st->b].cached_str;
    x86_emit_movabs_rax(cb, addr);                                   // rax = &cached_str
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_load_r64_base(cb, X86_RDX, X86_RAX, 0);                 // win64: rdx = cached_str
#else
    x86_emit_load_r64_base(cb, X86_RSI, X86_RAX, 0);                 // sysv: rsi = cached_str
#endif
    uint64_t helper = (uint64_t)(uintptr_t)&jit_match_str;
    x86_emit_movabs_rax(cb, helper);                                 // rax = helper
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    emit_u8(cb, 0x85); emit_u8(cb, 0xC0);                            // test eax, eax
    x86_cache_clear(cache);                                          // callee clobbered xmm
    emit_match_jcc(st, 0x85);                                        // jne target (match)
}

// OP_JUMP_MATCH_BOOL: bit-exact compare against MAKE_BOOL(b)
static void match_bool(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load subject
    if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_BOOL subj=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    x86_emit_movq_rax_xmm(cb, xa);                                   // rax = subject bits
    uint64_t expect = X86_BOOL_BITS | (st->b ? 1ULL : 0ULL);         // expected bool bits
    x86_emit_movabs_r11(cb, expect);                                 // r11 = expected bits
    emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);         // cmp rax, r11
    prepare_match_target(st);
    emit_match_jcc(st, 0x84);                                        // je target
}

// OP_JUMP_MATCH_NONE: bit-exact compare against NONE bits
static void match_none(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    int xa = x86_cache_load(cache, cb, st->a);                       // load subject
    if (xa < 0) JIT_FATAL("cache full: JUMP_MATCH_NONE subj=%d (func=%d pc=%d)",
                          st->a, st->func_idx, st->pc);
    x86_emit_movq_rax_xmm(cb, xa);                                   // rax = subject bits
    x86_emit_movabs_r11(cb, X86_NONE_BITS);                          // r11 = NONE bits
    emit_u8(cb, 0x4C); emit_u8(cb, 0x39); emit_u8(cb, 0xD8);         // cmp rax, r11
    prepare_match_target(st);
    emit_match_jcc(st, 0x84);                                        // je target
}

// OP_JUMP_TABLE: dense integer dispatch via an inline absolute-address table
static void match_jump_table(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    BytecodeChunk* chunk = st->chunk;                                // bytecode being compiled
    Instruction* inst = st->inst;                                    // current instruction

    int subj_reg = inst->operands[0];                                // subject register
    int min_val  = inst->operands[1];                                // minimum match value
    int tbl_idx  = inst->operands[2];                                // constant pool index
    Constant* tbl = &chunk->constants[tbl_idx];                      // CONST_JUMP_TABLE entry
    if (tbl->type != CONST_JUMP_TABLE)                               // wrong constant type?
        JIT_FATAL("OP_JUMP_TABLE without CONST_JUMP_TABLE (func=%d pc=%d)",
                  st->func_idx, st->pc);
    int range = tbl->jump_table.count;                               // dense range width

    int xa = x86_cache_load(cache, cb, subj_reg);                    // load subject
    if (xa < 0) JIT_FATAL("cache full: JUMP_TABLE subj=%d (func=%d pc=%d)",
                          subj_reg, st->func_idx, st->pc);

    x86_cache_flush(cache, cb);                                      // spill dirty slots before branch

    x86_emit_cvttsd2si_eax(cb, xa);                                  // eax = (int)subj
    emit_u8(cb, 0xF2);                                               // cvtsi2sd xmm15, eax
    x86_rex_r(cb, XMM_SCRATCH);
    emit_u8(cb, 0x0F); emit_u8(cb, 0x2A);
    emit_u8(cb, 0xC0 | ((XMM_SCRATCH & 7) << 3));
    x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);                        // ucomisd subj, truncated
    emit_u8(cb, 0x0F); emit_u8(cb, 0x85);                            // jne miss
    size_t miss_patch = cb->len;                                     // miss jump placeholder offset
    emit_i32(cb, 0);

    if (min_val != 0) {                                              // sub eax, min
        if (min_val >= -128 && min_val <= 127) {
            emit_u8(cb, 0x83); emit_u8(cb, 0xE8);
            emit_u8(cb, (uint8_t)min_val);
        } else {
            emit_u8(cb, 0x2D); emit_u32(cb, (uint32_t)min_val);
        }
    }
    if (range <= 127) {                                              // cmp eax, range
        emit_u8(cb, 0x83); emit_u8(cb, 0xF8);
        emit_u8(cb, (uint8_t)range);
    } else {
        emit_u8(cb, 0x3D); emit_u32(cb, (uint32_t)range);
    }
    emit_u8(cb, 0x0F); emit_u8(cb, 0x83);                            // jae miss
    size_t bounds_patch = cb->len;                                   // bounds miss placeholder offset
    emit_i32(cb, 0);

    emit_u8(cb, 0x4C); emit_u8(cb, 0x8D);                            // lea r11, [rip + table_disp]
    emit_u8(cb, 0x1D);
    size_t table_disp_at = cb->len;                                  // disp32 field offset
    emit_i32(cb, 0);
    emit_u8(cb, 0x41); emit_u8(cb, 0xFF);                            // jmp qword [r11 + rax*8]
    emit_u8(cb, 0x24);
    emit_u8(cb, 0xC3);                                               // sib: scale=8, index=rax, base=r11

    size_t table_start = cb->len;                                    // inline absolute-address table
    for (int i = 0; i < range; i++) {                                // one 8-byte slot per case
        x86_func_tab_fixup(st, cb->len, tbl->jump_table.addresses[i]);
        emit_u64(cb, 0);                                             // placeholder, patched later
    }
    size_t after_table = cb->len;                                    // position after the table

    int32_t tdisp = (int32_t)table_start - (int32_t)(table_disp_at + 4);
    memcpy(cb->buf + table_disp_at, &tdisp, 4);                      // patch table disp32
    int32_t mrel = (int32_t)after_table - (int32_t)(miss_patch + 4);
    memcpy(cb->buf + miss_patch, &mrel, 4);                          // patch non-integer miss
    int32_t brel = (int32_t)after_table - (int32_t)(bounds_patch + 4);
    memcpy(cb->buf + bounds_patch, &brel, 4);                        // patch bounds miss
}

// OP_SWITCH_STR: perfect-hash dispatch through jit_switch_str
static void match_switch_str(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    BytecodeChunk* chunk = st->chunk;                                // bytecode being compiled
    Instruction* inst = st->inst;                                    // current instruction

    int subj_reg  = inst->operands[0];                               // subject register
    int table_idx = inst->operands[1];                               // constant pool index
    Constant* c = &chunk->constants[table_idx];                      // CONST_STR_SWITCH entry
    if (c->type != CONST_STR_SWITCH)                                 // wrong constant type?
        JIT_FATAL("OP_SWITCH_STR without CONST_STR_SWITCH (func=%d pc=%d)",
                  st->func_idx, st->pc);

    size_t needed = (size_t)c->str_switch.count * 16 + 128;          // upper bound on emitted size
    if (cb->len + needed > cb->cap)                                  // buffer overflow guard
        JIT_FATAL("code buffer too small for SWITCH_STR (pc=%d)", st->pc);
    if (st->fixup_count + c->str_switch.count + 1 > (int)chunk->code_count)
        JIT_FATAL("fixup buffer too small for SWITCH_STR (pc=%d)", st->pc);

    x86_cache_flush(cache, cb);                                      // spill before the helper call

    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(subj_reg));     // rax = subject value
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64: arg0 = rcx
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv: arg0 = rdi
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)chunk);             // rax = chunk pointer
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_RAX);                      // win64: arg1 = rdx
    emit_u8(cb, 0x41); emit_u8(cb, 0xB8);                            // mov r8d, imm32
    emit_u32(cb, (uint32_t)table_idx);                               // win64: arg2 = r8d
#else
    x86_emit_mov_r64_r64(cb, X86_RSI, X86_RAX);                      // sysv: arg1 = rsi
    emit_u8(cb, 0xBA);                                               // mov edx, imm32
    emit_u32(cb, (uint32_t)table_idx);                               // sysv: arg2 = edx
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_switch_str);   // rax = helper
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_cache_clear(cache);                                          // callee clobbered all xmm

    int seen[256];                                                   // already-emitted target pcs
    int n_seen = 0;
    for (int i = 0; i <= c->str_switch.mask && n_seen < 256; i++) {
        int tgt = c->str_switch.entries[i].target;                   // case target pc
        if (tgt < 0) continue;                                       // empty slot
        if (tgt < st->start || tgt >= st->end)                       // out of function range
            JIT_FATAL("SWITCH_STR target out of range (pc=%d tgt=%d)", st->pc, tgt);
        bool dup = false;                                            // dedupe identical bodies
        for (int j = 0; j < n_seen; j++)
            if (seen[j] == tgt) { dup = true; break; }
        if (dup) continue;
        seen[n_seen++] = tgt;

        emit_u8(cb, 0x3D);                                           // cmp eax, imm32
        emit_u32(cb, (uint32_t)tgt);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                        // je rel32
        x86_func_jump_fixup(st, cb->len, tgt);
        emit_i32(cb, 0);
    }

    int def = c->str_switch.default_pc;                              // fallthrough: default arm
    if (def < st->start || def > st->end)                            // out of function range
        JIT_FATAL("SWITCH_STR default out of range (pc=%d def=%d)", st->pc, def);
    emit_u8(cb, 0xE9);                                               // jmp rel32
    x86_func_jump_fixup(st, cb->len, def);
    emit_i32(cb, 0);
}

// OP_SWITCH_NUM: perfect-hash dispatch through jit_switch_num
static void match_switch_num(X86FuncEmitState* st) {
    CodeBuf* cb = st->cb;                                            // destination code buffer
    XmmCache* cache = st->cache;                                     // live register cache
    BytecodeChunk* chunk = st->chunk;                                // bytecode being compiled
    Instruction* inst = st->inst;                                    // current instruction

    int subj_reg  = inst->operands[0];                               // subject register
    int table_idx = inst->operands[1];                               // constant pool index
    Constant* c = &chunk->constants[table_idx];                      // CONST_NUM_SWITCH entry
    if (c->type != CONST_NUM_SWITCH)                                 // wrong constant type?
        JIT_FATAL("OP_SWITCH_NUM without CONST_NUM_SWITCH (func=%d pc=%d)",
                  st->func_idx, st->pc);

    size_t needed = (size_t)c->num_switch.count * 16 + 128;          // upper bound on emitted size
    if (cb->len + needed > cb->cap)                                  // buffer overflow guard
        JIT_FATAL("code buffer too small for SWITCH_NUM (pc=%d)", st->pc);
    if (st->fixup_count + c->num_switch.count + 1 > (int)chunk->code_count)
        JIT_FATAL("fixup buffer too small for SWITCH_NUM (pc=%d)", st->pc);

    x86_cache_flush(cache, cb);                                      // spill before the helper call

    x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(subj_reg));     // rax = subject value
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RCX, X86_RAX);                      // win64: arg0 = rcx
#else
    x86_emit_mov_r64_r64(cb, X86_RDI, X86_RAX);                      // sysv: arg0 = rdi
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)chunk);             // rax = chunk pointer
#if defined(_WIN32) || defined(_WIN64)
    x86_emit_mov_r64_r64(cb, X86_RDX, X86_RAX);                      // win64: arg1 = rdx
    emit_u8(cb, 0x41); emit_u8(cb, 0xB8);                            // mov r8d, imm32
    emit_u32(cb, (uint32_t)table_idx);                               // win64: arg2 = r8d
#else
    x86_emit_mov_r64_r64(cb, X86_RSI, X86_RAX);                      // sysv: arg1 = rsi
    emit_u8(cb, 0xBA);                                               // mov edx, imm32
    emit_u32(cb, (uint32_t)table_idx);                               // sysv: arg2 = edx
#endif
    x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_switch_num);   // rax = helper
    emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                            // call rax
    x86_cache_clear(cache);                                          // callee clobbered all xmm

    int seen[256];                                                   // already-emitted target pcs
    int n_seen = 0;
    for (int i = 0; i <= c->num_switch.mask && n_seen < 256; i++) {
        int tgt = c->num_switch.entries[i].target;                   // case target pc
        if (tgt < 0) continue;                                       // empty slot
        if (tgt < st->start || tgt >= st->end)                       // out of function range
            JIT_FATAL("SWITCH_NUM target out of range (pc=%d tgt=%d)", st->pc, tgt);
        bool dup = false;                                            // dedupe identical bodies
        for (int j = 0; j < n_seen; j++)
            if (seen[j] == tgt) { dup = true; break; }
        if (dup) continue;
        seen[n_seen++] = tgt;

        emit_u8(cb, 0x3D);                                           // cmp eax, imm32
        emit_u32(cb, (uint32_t)tgt);
        emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                        // je rel32
        x86_func_jump_fixup(st, cb->len, tgt);
        emit_i32(cb, 0);
    }

    int def = c->num_switch.default_pc;                              // fallthrough: default arm
    if (def < st->start || def > st->end)                            // out of function range
        JIT_FATAL("SWITCH_NUM default out of range (pc=%d def=%d)", st->pc, def);
    emit_u8(cb, 0xE9);                                               // jmp rel32
    x86_func_jump_fixup(st, cb->len, def);
    emit_i32(cb, 0);
}

// function body dispatch for the match/switch category
void x86_func_emit_match(X86FuncEmitState* st) {
    switch (st->op) {
        case OP_JUMP_MATCH_NUM:  match_num(st);         break;       // numeric match
        case OP_JUMP_MATCH_STR:  match_str(st);         break;       // string match
        case OP_JUMP_MATCH_BOOL: match_bool(st);        break;       // bool match
        case OP_JUMP_MATCH_NONE: match_none(st);        break;       // none match
        case OP_JUMP_TABLE:      match_jump_table(st);  break;       // dense int dispatch
        case OP_SWITCH_STR:      match_switch_str(st);  break;       // string switch
        case OP_SWITCH_NUM:      match_switch_num(st);  break;       // numeric switch
        default:
            JIT_FATAL("match category: unexpected opcode %d (pc=%d)", st->op, st->pc);
    }
    st->did_flush = true;                                            // all handlers flush internally
}
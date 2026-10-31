// source/jit/x86-64/x86_64_emit_loop.c
// Loop emitters for the x86-64 JIT backend
// https://github.com/is-nobody/apex-lang
// MIT license

#include "x86_64_emit_internal.h"
#include "vm.h"
#include <stddef.h>
#include <string.h>

// hoists up to 4 loop-invariant array_part ptrs into callee-saved gprs; primary stays in rbx
static void assign_table_caches(JITContext* ctx, JitLoopInfo* info) {
    static const int cache_gprs[4] = { X86_R12, X86_R13, X86_R14, X86_R15 };
    info->n_cached_tables  = 0;                                      // no cached tables yet
    info->n_derived_caches = 0;                                      // no derived caches yet

    uint64_t invariant = info->live_in & ~info->live_out;            // slots written nowhere in body
    if (info->table.slot >= 0) invariant |= 1ULL << info->table.slot;  // primary lives in rbx

    uint64_t candidates = info->live_in & ~info->live_out;           // loop-invariant table slots
    for (int s = 0; s < JIT_MAX_SLOTS && (info->n_cached_tables + info->n_derived_caches) < 4; s++) {
        if (!(candidates & (1ULL << s))) continue;                   // must be loop-invariant
        if (s == info->table.slot) continue;                         // primary already in rbx
        JitSlotKind kind = info->live_in_kind[s];                    // expected slot kind
        if (kind != JIT_SLOT_TABLE && kind != JIT_SLOT_TABLE_ANY) continue;
        bool used_as_table = false;                                  // must be read as a table
        for (int pc = info->entry_pc; pc <= info->back_edge_pc && !used_as_table; pc++) {
            Instruction* inst = &ctx->chunk->code[pc];
            int t = -1;                                              // table operand slot
            switch (inst->opcode) {
                case OP_TABLE_GET: case OP_TABLE_GET_NUM:
                case OP_TABLE_GET_INT: case OP_TABLE_GET_KEY_STR:
                    t = inst->operands[1]; break;                    // table is the first source
                case OP_TABLE_SET: case OP_TABLE_SET_NUM:
                case OP_TABLE_SET_INT: case OP_TABLE_SET_KEY_STR:
                    t = inst->operands[0]; break;                    // table is the destination
                default: break;
            }
            if (t == s) used_as_table = true;
        }
        if (!used_as_table) continue;
        int idx = info->n_cached_tables + info->n_derived_caches;    // next cache gpr slot
        int i = info->n_cached_tables++;
        info->cached_table_slot[i] = s;
        info->cached_table_gpr[i]  = cache_gprs[idx];
        invariant |= 1ULL << s;                                      // now s is a cached base too
    }

    for (int s = 0; s < JIT_MAX_SLOTS && (info->n_cached_tables + info->n_derived_caches) < 4; s++) {
        if (!(info->live_out & (1ULL << s))) continue;               // must be written in body
        int write_pc = -1, write_count = 0;                          // locate the unique writer
        for (int pc = info->entry_pc; pc <= info->back_edge_pc; pc++) {
            Instruction* inst = &ctx->chunk->code[pc];
            bool writes_s = false;                                   // does this write s?
            switch (inst->opcode) {
                case OP_TABLE_GET: case OP_TABLE_GET_NUM:
                case OP_TABLE_GET_INT: case OP_TABLE_GET_KEY_STR:
                case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
                case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                case OP_LOAD_BOOL: case OP_LOAD_NONE:
                case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
                case OP_DIV_IMM: case OP_MOD_IMM:
                case OP_CMP_EQ: case OP_CMP_NEQ:
                case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
                case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                case OP_LOAD_GLOBAL:
                    writes_s = (inst->operands[0] == s);
                    break;
                default: break;
            }
            if (writes_s) { write_pc = pc; write_count++; }
        }
        if (write_count != 1 || write_pc < 0) continue;              // must have a single writer
        Instruction* prod = &ctx->chunk->code[write_pc];             // the single producer
        if (prod->opcode != OP_TABLE_GET_NUM && prod->opcode != OP_TABLE_GET) continue;
        int tbl = prod->operands[1], key = prod->operands[2];        // producer source operands
        if (!(invariant & (1ULL << tbl))) continue;                  // table must be invariant
        if (!(invariant & (1ULL << key))) continue;                  // key must be invariant
        int tbl_gpr = -1;                                            // locate the gpr holding tbl's array_part
        if (tbl == info->table.slot) tbl_gpr = X86_RBX;
        for (int i = 0; i < info->n_cached_tables; i++)
            if (info->cached_table_slot[i] == tbl) tbl_gpr = info->cached_table_gpr[i];
        if (tbl_gpr < 0) continue;
        bool read_as_value = false;                                  // s must be read only in table position
        for (int pc = info->entry_pc; pc <= info->back_edge_pc && !read_as_value; pc++) {
            if (pc == write_pc) continue;                            // skip the producer itself
            Instruction* inst = &ctx->chunk->code[pc];
            int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];
            switch (inst->opcode) {
                case OP_TABLE_GET: case OP_TABLE_GET_NUM:
                    if (b == s) read_as_value = true;                // key position is a value read
                    break;
                case OP_TABLE_GET_INT: case OP_TABLE_GET_KEY_STR:
                    break;                                           // a==s is fine, no other slot read
                case OP_TABLE_SET: case OP_TABLE_SET_NUM:
                    if (a == s || b == s) read_as_value = true;
                    break;
                case OP_TABLE_SET_INT:
                    if (b == s) read_as_value = true;
                    break;
                case OP_TABLE_SET_KEY_STR:
                    if (a == s) read_as_value = true;
                    break;
                case OP_MOVE: case OP_NEG:
                case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
                case OP_CMP_EQ: case OP_CMP_NEQ: case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
                case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                case OP_JUMP_IF_EQ: case OP_JUMP_IF_NEQ: case OP_JUMP_IF_EQ_NUM:
                case OP_JUMP_IF_NEQ_NUM: case OP_JUMP_IF_LT: case OP_JUMP_IF_GT:
                case OP_JUMP_IF_LTE: case OP_JUMP_IF_GTE:
                case OP_JUMP_IF_EQ_IMM: case OP_JUMP_IF_NEQ_IMM:
                case OP_JUMP_IF_LT_IMM: case OP_JUMP_IF_GT_IMM:
                case OP_JUMP_IF_LTE_IMM: case OP_JUMP_IF_GTE_IMM:
                case OP_JUMP_IF_FALSE: case OP_CALL_1:
                    if (a == s || b == s) read_as_value = true;
                    break;
                case OP_CALL_2:
                    if (a == s || b == s || b + 1 == s) read_as_value = true;
                    break;
                case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
                case OP_DIV_IMM: case OP_MOD_IMM:
                case OP_INC: case OP_DEC:
                case OP_STORE_GLOBAL:
                    if (d == s || a == s) read_as_value = true;
                    break;
                case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
                case OP_LOAD_BOOL: case OP_LOAD_NONE:
                case OP_JUMP: case OP_LOAD_GLOBAL:
                    break;                                           // pure writes / no reads
                default:
                    if (a == s || b == s || d == s) read_as_value = true;
                    break;
            }
        }
        if (read_as_value) continue;
        bool used_as_set_table = false;                              // used as SET's table operand?
        for (int pc = info->entry_pc; pc <= info->back_edge_pc && !used_as_set_table; pc++) {
            Instruction* inst = &ctx->chunk->code[pc];
            switch (inst->opcode) {
                case OP_TABLE_SET: case OP_TABLE_SET_NUM:
                case OP_TABLE_SET_INT: case OP_TABLE_SET_KEY_STR:
                    if (inst->operands[0] == s) used_as_set_table = true;
                    break;
                default: break;
            }
        }
        if (used_as_set_table) continue;                             // derived write needs grown array_part
        int idx = info->n_cached_tables + info->n_derived_caches;    // next cache gpr slot
        int i = info->n_derived_caches++;
        info->derived_slot[i]        = s;
        info->derived_gpr[i]         = cache_gprs[idx];
        info->derived_src_gpr[i]     = tbl_gpr;
        info->derived_key_slot[i]    = key;
        info->derived_producer_pc[i] = write_pc;
        invariant |= 1ULL << s;                                      // now s is invariant too
    }
}

// spill dirty loop-carried slots & optional outputs on the forced-flush path
static void x86_cache_flush_back_edge(XmmCache* c, CodeBuf* cb,
                                      BytecodeChunk* chunk, int body_start,
                                      int back_edge, uint64_t writeback_mask) {
    for (int i = 0; i < XMM_CACHE_REGS; i++) {
        int s = c->reg_slot[i];                                      // slot held in xmm<i>
        if (s < 0 || !c->slot_dirty[s]) continue;                    // empty or clean slot
        if (!slot_read_before_write(chunk, body_start, back_edge, s) &&
            !(writeback_mask & (1ULL << s))) continue;              // not loop-carried or needed after the loop
        x86_emit_movsd_store(cb, i, x86_slot_disp(s));               // spill to frame
    }
    x86_cache_clear(c);                                              // empty cache at back edge
}

// dispatch one loop body instruction through the category handlers
void emit_loop_body_instr(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                          XmmCache* cache, int pc, int const_slot, int save_slot,
                          JitLoopInfo* info) {
    for (int i = 0; i < info->n_derived_caches; i++) {               // derived invariants?
        if (pc == info->derived_producer_pc[i]) return;              // producer handled by prologue
    }
    Instruction* inst = &ctx->chunk->code[pc];                       // current instruction

    X86LoopBodyState st;                                             // per-instruction state
    memset(&st, 0, sizeof(st));
    st.abi = abi;  st.ctx = ctx;  st.cb = cb;  st.cache = cache;
    st.info = info;  st.chunk = ctx->chunk;  st.pc = pc;
    st.const_slot = const_slot;  st.save_slot = save_slot;
    st.inst = inst;
    st.d = inst->operands[0];                                        // destination operand
    st.a = inst->operands[1];                                        // first source operand
    st.b = inst->operands[2];                                        // second source operand
    st.op = inst->opcode;                                            // opcode shorthand

    switch (st.op) {
        case OP_MOVE:                                                // reg-to-reg copy
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:                      // numeric literals
        case OP_LOAD_BOOL: case OP_LOAD_NONE:                        // bool / none literals
        case OP_LOAD_GLOBAL: case OP_STORE_GLOBAL:                   // global access
            x86_loop_emit_move(&st);
            break;

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:          // binary arithmetic
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:           // immediate arithmetic
        case OP_DIV_IMM: case OP_MOD_IMM: case OP_MOD:               // mod ops
        case OP_NEG: case OP_INC: case OP_DEC:                       // unary / in-place
            x86_loop_emit_arith(&st);
            break;

        case OP_CMP_EQ: case OP_CMP_EQ_NUM:                          // numeric compares
        case OP_CMP_NEQ: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT:
        case OP_CMP_LTE: case OP_CMP_GTE:
            x86_loop_emit_cmp(&st);
            break;

        case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:              // direct calls
            x86_loop_emit_call(&st);
            break;

        case OP_TABLE_GET: case OP_TABLE_GET_NUM:                    // table reads
        case OP_TABLE_GET_INT:
        case OP_TABLE_SET: case OP_TABLE_SET_NUM:                    // table writes
        case OP_TABLE_SET_INT:
        case OP_TABLE_GET_KEY_STR: case OP_TABLE_SET_KEY_STR:        // string-key fused ops
        case OP_TABLE_GET_CONST: case OP_TABLE_SET_CONST:            // const-key ops
            x86_loop_emit_table(&st);
            break;

        default:
            JIT_FATAL("unreachable opcode %d in loop body at pc=%d", st.op, pc);
    }

    if (st.touched_nan_check && info->touches_tables) {              // NaN propagation matches interpreter
        int xd = cache->slot_reg[st.d];                              // dest slot still in cache?
        if (xd >= 0) emit_nan_check(cb, xd);                         // rewrite NaN to NONE bits
    }
}

// emits one iteration (entry test + body) and snapshots cache state for the exit edge
size_t emit_loop_iteration(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                           XmmCache* cache, JitLoopInfo* info, int const_slot,
                           int save_slot, bool iter_in_xmm, int step_sign,
                           bool needs_helper, XmmCache* out_exit_cache) {
    BytecodeChunk* chunk = ctx->chunk;                               // bytecode being compiled
    int entry = info->entry_pc;                                      // first body pc
    int back_edge = info->back_edge_pc;                              // jump back to entry

    size_t exit_patch;                                               // offset of the exit jump placeholder
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                        // counter-based for loop
        int var_reg = chunk->code[entry].operands[0];                // loop counter slot
        int end_slot = info->for_end_cache_slot >= 0
                     ? info->for_end_cache_slot : info->for_end_reg; // stable bound snapshot
        int step_slot = info->for_step_cache_slot >= 0
                      ? info->for_step_cache_slot : info->for_step_reg; // stable step snapshot

        int xa = x86_cache_load(cache, cb, end_slot);                // load preserved end bound
        if (xa < 0) JIT_FATAL("cache full: loop entry end slot=%d (pc=%d)", end_slot, entry);
        int xb = x86_cache_load_excl(cache, cb, step_slot, xa, -1);  // load preserved step, avoid xa
        if (xb < 0) JIT_FATAL("cache full: loop entry step slot=%d (pc=%d)", step_slot, entry);
        int xc = x86_cache_dest_reg(cache, cb, var_reg, xa, xb);     // reuse var's existing home, no load
        if (xc < 0) JIT_FATAL("cache full: loop entry var_reg=%d (pc=%d)", var_reg, entry);

        if (!iter_in_xmm) {                                          // iterator lives in memory?
            x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));
        }
        x86_emit_ucomisd_rr(cb, XMM_SCRATCH, xa);                    // ucomisd xmm15, xa
        emit_u8(cb, 0x0F);
        emit_u8(cb, (step_sign > 0) ? 0x87 : 0x82);                  // ja exit / jb exit
        exit_patch = cb->len;                                        // record placeholder offset
        emit_i32(cb, 0);                                             // placeholder for rel32
        if (out_exit_cache) *out_exit_cache = *cache;                // cache state on the branch-taken path

        x86_emit_sse66_rr(cb, 0x28, xc, XMM_SCRATCH);                // movapd xc, xmm15 (R[var] = c)
        x86_cache_put(cache, xc, var_reg);                           // var became dirty
        if (info->for_var_gpr < 0 || !info->for_var_frame_safe) {
            x86_emit_movsd_store(cb, xc, x86_slot_disp(var_reg));    // frame[var] = i
            cache->slot_dirty[var_reg] = false;                      // memory is in sync
        }
        x86_emit_sse_arith_rr(cb, 0x58, XMM_SCRATCH, xb);            // addsd xmm15, xb (step)
        if (!iter_in_xmm) {
            x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->nregs));  // store iterator
        }
    } else if (entry_op_is_imm(chunk->code[entry].opcode)) {         // imm-compare loop entry
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                             // left operand slot
        int imm = entry_inst->operands[2];                           // immediate
        int xa = x86_cache_load(cache, cb, a);                       // load left operand
        if (xa < 0) JIT_FATAL("cache full: loop entry IMM a=%d (pc=%d)", a, entry);
        x86_emit_load_double_imm(cb, XMM_SCRATCH, imm);              // xmm15 = (double)imm
        x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);                    // ucomisd xa, xmm15
        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);          // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                         // conditional exit
        exit_patch = cb->len;                                        // record placeholder offset
        emit_i32(cb, 0);                                             // placeholder for rel32
        if (out_exit_cache) *out_exit_cache = *cache;                // cache state on the branch-taken path
    } else {                                                         // condition-based loop (reg-reg)
        Instruction* entry_inst = &chunk->code[entry];
        int a = entry_inst->operands[1];                             // left operand slot
        int b = entry_inst->operands[2];                             // right operand slot
        int xa = x86_cache_load(cache, cb, a);                       // load left operand
        if (xa < 0) JIT_FATAL("cache full: loop entry cond a=%d (pc=%d)", a, entry);
        int xb = x86_cache_load_excl(cache, cb, b, xa, -1);          // load right, avoid xa
        if (xb < 0) JIT_FATAL("cache full: loop entry cond b=%d (pc=%d)", b, entry);

        x86_emit_ucomisd_rr(cb, xa, xb);                             // ucomisd xa, xb

        uint8_t jcc = jcc_for_entry_op(entry_inst->opcode);          // exit condition opcode
        emit_u8(cb, 0x0F); emit_u8(cb, jcc);                         // conditional exit
        exit_patch = cb->len;                                        // record placeholder offset
        emit_i32(cb, 0);                                             // placeholder for rel32
        if (out_exit_cache) *out_exit_cache = *cache;                // cache state on the branch-taken path
    }

    for (int pc = entry + 1; pc < back_edge; pc++) {                 // body
        emit_loop_body_instr(abi, ctx, cb, cache, pc, const_slot, save_slot, info);
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && info->for_var_gpr >= 0) {  // advance integer counter
        int g = info->for_var_gpr;                                   // counter gpr
        emit_u8(cb, 0x48 | (g >= 8 ? 0x01 : 0));                     // rex.w + rex.b
        emit_u8(cb, 0xFF);                                           // inc / dec g
        if (step_sign > 0) emit_u8(cb, 0xC0 | (g & 7));
        else               emit_u8(cb, 0xC8 | (g & 7));
    }

    if (needs_helper) {                                              // helpers read their args from the frame
        x86_cache_flush(cache, cb);                                  // spill live dirty slots before the call
    }

    return exit_patch;                                               // caller patches the exit jump
}

// rejects loops that would need runtime checks the emitter does not produce
bool loop_is_safe_to_emit(JITContext* ctx, JitLoopInfo* info) {
    BytecodeChunk* chunk = ctx->chunk;                               // bytecode being compiled
    int entry     = info->entry_pc;                                  // first body pc
    int back_edge = info->back_edge_pc;                              // jump back to entry

    if (info->table.used && info->uses_globals) return false;        // rbx conflict

    if (info->table.used && info->table.slot >= 0 &&                 // table-pointer guard must hold
        (info->live_out & (1ULL << info->table.slot))) return false;

    // numeric-for bounds and step are copied into private frame slots before the body runs

    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        if (inst->opcode == OP_TABLE_SET_INT) {                      // rbx holds only info->table.slot's array_part
            if (inst->operands[0] != info->table.slot) return false;
        } else if (inst->opcode == OP_TABLE_GET_INT) {               // same rbx issue as SET_INT
            if (inst->operands[1] != info->table.slot) return false;
        } else if (inst->opcode == OP_CALL_0 ||                      // call targets must be JIT-compiled
                   inst->opcode == OP_CALL_1 ||
                   inst->opcode == OP_CALL_2) {
            int target = inst->operands[1];                          // callee function index
            if (target < 0 || target >= ctx->func_count) return false;
            if (!ctx->func_table[target]) return false;              // callee has no native code
        }
    }
    return true;                                                     // only safe operations seen
}

// emits native code for a numeric loop (with optional table access)
bool x86_64_emit_numeric_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                               JitLoopInfo* info, int step_sign, void** out_fn) {
    int entry = info->entry_pc;                                      // first body pc
    int back_edge = info->back_edge_pc;                              // jump back to entry
    int nregs = info->nregs;                                         // function frame size
    int extra_slots = (info->kind == JIT_LOOP_NUMERIC_FOR) ? 1 : 0;  // iterator temp slot
    int const_slot = nregs + extra_slots;                            // reserved slot for constant 1.0

    bool needs_helper = false;                                       // does the body call a runtime helper?
    for (int pc = entry + 1; pc < back_edge; pc++) {
        Instruction* inst = &ctx->chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:                               // fused string-key op: helper
            case OP_TABLE_SET_KEY_STR:
            case OP_TABLE_GET_CONST:                                 // const-key op: helper on slow path
            case OP_TABLE_SET_CONST:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:          // user calls clobber frame_reg
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[2] : inst->operands[1];
                int tbl_reg  = (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM)
                             ? inst->operands[1] : inst->operands[0];
                bool key_is_str = (key_reg >= 0 && key_reg < 64) &&
                                  ((info->str_slots >> key_reg) & 1ULL);
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot) &&
                               !key_is_str;
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    info->n_imms = 0;                                                // precomputed immediates count
    info->imm_opt_ok = true;                                         // all fit in the pool?
    for (int pc = entry + 1; pc < back_edge; pc++) {                 // collect distinct immediates
        Instruction* inst = &ctx->chunk->code[pc];
        int32_t imm;                                                 // immediate operand
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;                                          // already pooled?
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }

    int imm_base_slot = const_slot + 1;                              // first slot for precomputed immediates
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    int private_control_slots = 0;                                  // stable snapshots for FOR_NEXT control values
    info->for_end_cache_slot  = -1;
    info->for_step_cache_slot = -1;
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        int control_base = imm_base_slot + info->n_imms;
        info->for_end_cache_slot  = control_base;
        info->for_step_cache_slot = control_base + 1;
        if (info->for_step_cache_slot >= JIT_MAX_SLOTS) return false; // cache cannot address private slots
        private_control_slots = 2;
    }

    assign_table_caches(ctx, info);                                  // must run before frame_slots is sized

    info->for_var_gpr = -1;                                          // reset cached counter state
    info->for_var_gpr_slot = -1;
    if (info->kind == JIT_LOOP_NUMERIC_FOR &&
        (info->for_step_value == 1 || info->for_step_value == -1)) {
        bool uses_counter = false;                                   // is for_var used as a table key?
        for (int pc = entry + 1; pc < back_edge && !uses_counter; pc++) {
            Instruction* inst = &ctx->chunk->code[pc];
            switch (inst->opcode) {
                case OP_TABLE_GET_NUM: case OP_TABLE_GET:
                    if (inst->operands[2] == info->for_var_reg) uses_counter = true;
                    break;
                case OP_TABLE_SET_NUM: case OP_TABLE_SET:
                    if (inst->operands[1] == info->for_var_reg) uses_counter = true;
                    break;
                default: break;
            }
        }
        if (uses_counter) {                                          // pick an unused callee-saved gpr
            static const int all_gprs[4] = { X86_R12, X86_R13, X86_R14, X86_R15 };
            bool used[4] = { false, false, false, false };
            for (int i = 0; i < info->n_cached_tables; i++)
                for (int j = 0; j < 4; j++)
                    if (info->cached_table_gpr[i] == all_gprs[j]) used[j] = true;
            for (int i = 0; i < info->n_derived_caches; i++)
                for (int j = 0; j < 4; j++)
                    if (info->derived_gpr[i] == all_gprs[j]) used[j] = true;
            for (int j = 0; j < 4; j++)
                if (!used[j]) { info->for_var_gpr = all_gprs[j]; break; }
        }
    }

    info->for_var_frame_safe = false;                                // pessimistic default
    if (info->for_var_gpr >= 0) {
        bool safe = true;                                            // does anything read frame[var]?
        for (int pc = entry + 1; pc < back_edge && safe; pc++) {
            Instruction* inst = &ctx->chunk->code[pc];
            int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];
            switch (inst->opcode) {
                case OP_CALL_1:
                    if (b == info->for_var_reg) safe = false;        // arg via frame
                    break;
                case OP_CALL_2:
                    if (b == info->for_var_reg || b + 1 == info->for_var_reg) safe = false;
                    break;
                case OP_TABLE_GET: case OP_TABLE_GET_NUM:
                    if (a == info->for_var_reg) safe = false;        // counter as table operand
                    break;
                case OP_TABLE_SET: case OP_TABLE_SET_NUM:
                    if (d == info->for_var_reg || a == info->for_var_reg ||
                        b == info->for_var_reg) safe = false;
                    break;
                case OP_TABLE_GET_INT: case OP_TABLE_SET_INT:
                case OP_TABLE_GET_KEY_STR: case OP_TABLE_SET_KEY_STR:
                    if (d == info->for_var_reg || a == info->for_var_reg ||
                        b == info->for_var_reg) safe = false;
                    break;
                default: break;
            }
        }
        info->for_var_frame_safe = safe;
    }

    int save_slot = -1;                                              // slot to stash frame_reg across calls
    int frame_slots = const_slot + 1 + info->n_imms + private_control_slots; // constants, immediates, loop controls
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;    // slots whose old value must be released
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                                     // reserve one more slot
        frame_slots++;
    }

    bool need_rbx_save = (info->table.used && info->table.slot >= 0) ||
                         info->uses_globals;
    int rbx_save_slot = -1;                                          // stack slot for caller's rbx
    if (need_rbx_save) {
        rbx_save_slot = frame_slots;
        frame_slots++;
    }
    int cached_gpr_save_slot[4] = { -1, -1, -1, -1 };                // save slots for cached table gprs
    for (int i = 0; i < info->n_cached_tables; i++) {
        cached_gpr_save_slot[i] = frame_slots;                       // one stack slot per cached gpr
        frame_slots++;
    }
    int derived_gpr_save_slot[2] = { -1, -1 };                       // save slots for derived table gprs
    for (int i = 0; i < info->n_derived_caches; i++) {
        derived_gpr_save_slot[i] = frame_slots;
        frame_slots++;
    }
    if (info->for_var_gpr >= 0) {                                    // reserve a save slot for the counter gpr
        info->for_var_gpr_slot = frame_slots;
        frame_slots++;
    }

    int range_size = back_edge - entry + 1;                          // number of body pcs
    if (range_size <= 0) return false;                               // empty range, nothing to emit

    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap)         // capacity check
        JIT_FATAL("code buffer too small for numeric loop at pc=%d", entry);

    if (!loop_is_safe_to_emit(ctx, info)) return false;              // needs runtime checks we lack

    bool iter_in_xmm = false;                                        // can iterator stay in xmm15?
    if (info->kind == JIT_LOOP_NUMERIC_FOR) {
        bool clobbers = false;                                       // any op clobber xmm15?
        for (int pc = entry + 1; pc < back_edge; pc++) {
            Opcode op = ctx->chunk->code[pc].opcode;
            bool op_is_imm = (op == OP_ADD_IMM || op == OP_SUB_IMM ||
                              op == OP_MUL_IMM || op == OP_DIV_IMM ||
                              op == OP_MOD_IMM);
            if (op_is_imm) {
                if (info->imm_opt_ok) continue;                      // memory-operand form, scratch-free
                clobbers = true;                                     // imm load clobbers XMM_SCRATCH
                break;
            }
            if (op == OP_MOD || op == OP_NEG ||                      // use XMM_SCRATCH
                op == OP_TABLE_GET_KEY_STR ||                        // helper call clobbers xmm
                op == OP_TABLE_SET_KEY_STR ||                        // helper call clobbers xmm
                op == OP_TABLE_GET_CONST ||                          // helper call clobbers xmm
                op == OP_TABLE_SET_CONST ||                          // helper call clobbers xmm
                op == OP_TABLE_SET ||                                // general path calls table_set_int
                op == OP_TABLE_SET_NUM ||                            // general path calls table_set_int
                op == OP_NEW_TABLE ||                                // helper call clobbers xmm
                op == OP_CALL_0 || op == OP_CALL_1 ||                // calls clobber all xmm regs
                op == OP_CALL_2) {
                clobbers = true;
                break;                                               // iterator must live in memory
            }
        }
        iter_in_xmm = !clobbers;
    }

    uint8_t* scratch_buf = ctx->scratch_code_buf;                    // shared scratch buffer
    size_t scratch_size  = ctx->scratch_code_buf_cap;
    if (!scratch_buf || scratch_size < (size_t)range_size * 256 + 1024)
        JIT_FATAL("scratch buffer too small for numeric loop at pc=%d", entry);
    CodeBuf scratch = { scratch_buf, 0, scratch_size };              // scratch emitter state

    XmmCache cache_start;                                            // fixed-point cache state
    x86_cache_clear(&cache_start);                                   // start from empty cache
    bool converged = false;                                          // fixed point reached?
    bool flush_at_back_edge = false;                                 // fallback policy when fixpoint oscillates
    for (int iter = 0; iter < 8; iter++) {                           // fixpoint loop
        XmmCache cache = cache_start;                                // scratch state per pass
        scratch.len = 0;                                             // reset scratch buffer
        size_t patch = emit_loop_iteration(abi, ctx, &scratch, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper, NULL);
        if (patch == (size_t)-1)
            JIT_FATAL("loop emit failed in fixpoint: pc=%d", entry);
        if (flush_at_back_edge) x86_cache_clear(&cache);             // simulate the spill before the back edge
        if (x86_cache_eq(&cache, &cache_start)) { converged = true; break; }  // stable state reached
        cache_start = cache;                                         // try again with new state
    }
    if (!converged) {
        x86_cache_clear(&cache_start);                               // empty incoming state
        flush_at_back_edge = true;                                   // force a spill before the back edge
        converged = true;                                            // empty start + flush at back edge is stable
    }

    size_t mark = cb->len;                                           // rollback point

    int base_frame = align16(8 * frame_slots);                       // aligned rbp-relative frame base
    int frame_size = align16(base_frame + abi->frame_extra);         // plus abi extras

    emit_prologue(cb, abi, frame_size, base_frame);                  // push rbp; mov rbp, rsp; sub rsp

    if (info->table.used && info->table.slot >= 0) {                 // preload primary table array_part
        x86_emit_store_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));  // save caller's rbx
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);  // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                               // strip nan-box tag
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));  // rbx = array_part
    } else if (info->uses_globals) {                                 // preload globals base
        x86_emit_store_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));  // save caller's rbx
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);                        // mov rbx, abi->globals_reg
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }
    for (int i = 0; i < info->n_cached_tables; i++) {                // preload cached table array_parts
        int slot = info->cached_table_slot[i];
        int gpr  = info->cached_table_gpr[i];
        x86_emit_store_r64_rbp(cb, gpr, x86_slot_disp(cached_gpr_save_slot[i]));  // save caller's gpr
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, slot * 8);            // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                               // strip nan-box tag
        x86_emit_load_r64_base(cb, gpr, X86_RAX, (int32_t)offsetof(Table, array_part));  // gpr = array_part
    }

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);                  // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                          // xmm7 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // const_slot = 1.0

    for (int i = 0; i < info->n_imms; i++) {                         // store precomputed immediates
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR) {                        // preserve FOR_INIT values before body register reuse
        x86_emit_movsd_load_base(cb, abi->frame_reg, XMM_SCRATCH, info->for_end_reg * 8);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->for_end_cache_slot));
        x86_emit_movsd_load_base(cb, abi->frame_reg, XMM_SCRATCH, info->for_step_reg * 8);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->for_step_cache_slot));
    }

    uint64_t m = info->live_in | info->live_out;                     // seed live-in AND live-out slots
    while (m) {
        int s = __builtin_ctzll(m);                                  // next slot to seed
        m &= m - 1;
        if (s >= 64) break;                                          // beyond tracked range
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);      // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));               // frame[s] = xmm0
    }

    for (int i = 0; i < info->n_derived_caches; i++) {               // compute derived invariants
        int gpr     = info->derived_gpr[i];
        int src_gpr = info->derived_src_gpr[i];
        int key     = info->derived_key_slot[i];
        x86_emit_store_r64_rbp(cb, gpr, x86_slot_disp(derived_gpr_save_slot[i]));  // save caller's gpr
        x86_emit_movsd_load(cb, 0, x86_slot_disp(key));              // xmm0 = frame[key]
        x86_emit_cvttsd2si_edx(cb, 0);                               // edx = (int)key
        x86_emit_dec_edx(cb);                                        // 1-based -> 0-based
        x86_emit_load_rax_idx8(cb, src_gpr, X86_RDX);                // rax = src_gpr[rdx] (R[d] value)
        x86_emit_clear_high16_rax(cb);                               // strip nan-box tag
        x86_emit_load_r64_base(cb, gpr, X86_RAX, (int32_t)offsetof(Table, array_part));  // gpr = R[d].array_part
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && !iter_in_xmm) {        // seed iterator in memory
        int var_reg = ctx->chunk->code[entry].operands[0];           // counter slot
        x86_emit_movsd_load(cb, 0, x86_slot_disp(var_reg));          // xmm0 = R[var]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(nregs));           // frame[nregs] = xmm0
        if (info->for_var_gpr >= 0) {                                // seed gpr with 0-based counter
            int g = info->for_var_gpr;
            x86_emit_store_r64_rbp(cb, g, x86_slot_disp(info->for_var_gpr_slot));  // save caller's gpr
            x86_emit_movsd_load(cb, 0, x86_slot_disp(var_reg));      // xmm0 = R[var]
            x86_emit_cvttsd2si_gpr64(cb, g, 0);                      // g = (int64)R[var]
            emit_u8(cb, 0x48 | (g >= 8 ? 0x01 : 0));                 // rex.w + rex.b
            emit_u8(cb, 0x83); emit_u8(cb, 0xE8 | (g & 7));          // sub g, 1
            emit_u8(cb, 0x01);
        }
    }

    for (int i = 0; i < XMM_CACHE_REGS; i++) {                       // preload the fixpoint cache state
        int s = cache_start.reg_slot[i];                             // slot held in xmm<i>
        if (s >= 0) x86_emit_movsd_load(cb, i, x86_slot_disp(s));    // xmm<i> = frame[s]
    }

    if (info->kind == JIT_LOOP_NUMERIC_FOR && iter_in_xmm) {         // load iterator into xmm15
        int var_reg = ctx->chunk->code[entry].operands[0];           // counter slot
        x86_emit_movsd_load(cb, XMM_SCRATCH, x86_slot_disp(var_reg));  // xmm15 = R[var]
        if (info->for_var_gpr >= 0) {                                // seed gpr with 0-based counter
            int g = info->for_var_gpr;
            x86_emit_store_r64_rbp(cb, g, x86_slot_disp(info->for_var_gpr_slot));  // save caller's gpr
            x86_emit_cvttsd2si_gpr64(cb, g, XMM_SCRATCH);            // g = (int64)xmm15
            emit_u8(cb, 0x48 | (g >= 8 ? 0x01 : 0));                 // rex.w + rex.b
            emit_u8(cb, 0x83); emit_u8(cb, 0xE8 | (g & 7));          // sub g, 1
            emit_u8(cb, 0x01);
        }
    }

    int loop_top = (int)cb->len;                                     // loop start address
    XmmCache cache = cache_start;                                    // live cache state
    XmmCache exit_cache;                                              // cache state after the loop guard, before the body
    size_t entry_patch = emit_loop_iteration(abi, ctx, cb, &cache, info, const_slot, save_slot, iter_in_xmm, step_sign, needs_helper, &exit_cache);
    if (entry_patch == (size_t)-1)
        JIT_FATAL("loop emit failed (real pass): pc=%d", entry);

    if (flush_at_back_edge) x86_cache_flush_back_edge(&cache, cb, ctx->chunk,
                                                      entry + 1, back_edge,
                                                      info->live_out & ~info->ref_writes);
    emit_u8(cb, 0xE9);                                               // jmp loop_top
    size_t back_patch = cb->len;                                     // back-edge rel32 placeholder
    emit_i32(cb, 0);
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);                      // patch back edge

    size_t exit_stub = cb->len;                                     // branch target for the failed loop test
    x86_cache_flush(&exit_cache, cb);                                // spill values still cached at the guard
    int32_t exit_rel = (int32_t)exit_stub - (int32_t)(entry_patch + 4);
    memcpy(cb->buf + entry_patch, &exit_rel, 4);                     // patch exit jump to its own flush stub

    m = info->live_out;                                              // copy written slots back to vm
    while (m) {
        int s = __builtin_ctzll(m);                                  // next written slot
        m &= m - 1;
        if (s >= 64) break;                                          // beyond tracked range
        if (info->ref_writes & (1ULL << s)) continue;                // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

            if (s != 0) {                                            // arg0: &pool[s] = frame_reg + s*8
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);                                   // win64: add rcx, imm32
#else
                emit_u8(cb, 0xC7);                                   // sysv: add rdi, imm32
#endif
                emit_i32(cb, s * 8);
            }

            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));    // arg1: frame[s]
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2); // win64: rdx = value
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6); // sysv: rsi = value
#endif
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                    // call rax
            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // restore frame_reg
        } else {
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));            // plain store, no refcount
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    for (int i = 0; i < info->n_cached_tables; i++) {                // restore cached table gprs
        x86_emit_load_r64_rbp(cb, info->cached_table_gpr[i], x86_slot_disp(cached_gpr_save_slot[i]));
    }
    for (int i = 0; i < info->n_derived_caches; i++) {               // restore derived table gprs
        x86_emit_load_r64_rbp(cb, info->derived_gpr[i], x86_slot_disp(derived_gpr_save_slot[i]));
    }
    if (info->for_var_gpr >= 0) {                                    // restore caller's counter gpr
        x86_emit_load_r64_rbp(cb, info->for_var_gpr, x86_slot_disp(info->for_var_gpr_slot));
    }

    if (need_rbx_save) {
        x86_emit_load_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);                             // leave; ret

    *out_fn = (void*)(cb->buf + mark);                               // publish entry pointer
    return true;                                                     // emission successful
}

// emits native code for a table iteration loop (for v in t)
bool x86_64_emit_table_iter_loop(const X86_64Abi* abi, JITContext* ctx,
                                  CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;                               // bytecode being compiled
    int entry = info->entry_pc;                                      // first body pc
    int back_edge = info->back_edge_pc;                              // jump back to entry
    int nregs = info->nregs;                                         // function frame size
    int table_slot = info->table.slot;                               // slot holding the iterated table
    int var_reg = chunk->code[entry].operands[0];                    // loop variable slot

    if (table_slot < 0) return false;                                // no table pinned, bail

    int range_size = back_edge - entry + 1;                          // number of body pcs
    if (range_size <= 0) return false;                               // empty range
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap)         // capacity check
        JIT_FATAL("code buffer too small for table-iter loop at pc=%d", entry);

    int const_slot = nregs;                                          // reserved 1.0 slot
    int save_slot  = nregs + 1;                                      // frame_reg spill slot
    int slot_rbx   = nregs + 2;                                      // caller's rbx (array_part)
    int slot_r12   = nregs + 3;                                      // caller's r12 (array_count)
    int slot_r13   = nregs + 4;                                      // caller's r13 (reserved)
    int slot_r14   = nregs + 5;                                      // caller's r14 (iteration counter)
    int slot_r15   = nregs + 6;                                      // caller's r15 (NONE bits)
    int frame_slots = nregs + 7;                                     // total frame slots
    int base_frame = align16(8 * frame_slots);                       // aligned rbp-relative frame base
    int frame_size = align16(base_frame + abi->frame_extra);         // plus abi extras

    size_t mark = cb->len;                                           // rollback point

    emit_prologue(cb, abi, frame_size, base_frame);                  // push rbp; mov rbp, rsp; sub rsp

    x86_emit_store_r64_rbp(cb, X86_RBX, x86_slot_disp(slot_rbx));    // save caller's rbx
    x86_emit_store_r64_rbp(cb, X86_R12, x86_slot_disp(slot_r12));    // save caller's r12
    x86_emit_store_r64_rbp(cb, X86_R13, x86_slot_disp(slot_r13));    // save caller's r13
    x86_emit_store_r64_rbp(cb, X86_R14, x86_slot_disp(slot_r14));    // save caller's r14
    x86_emit_store_r64_rbp(cb, X86_R15, x86_slot_disp(slot_r15));    // save caller's r15

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);                  // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                          // xmm15 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // frame[const_slot] = 1.0

    x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, table_slot * 8);  // rax = regs[table_slot]
    x86_emit_clear_high16_rax(cb);                                   // strip nan-box tag
    x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));   // rbx = array_part
    x86_emit_load_r64_base(cb, X86_R12, X86_RAX, (int32_t)offsetof(Table, array_count));  // r12 = array_count

    emit_u8(cb, 0x45); emit_u8(cb, 0x31); emit_u8(cb, 0xF6);         // xor r14d, r14d

    emit_u8(cb, 0x49); emit_u8(cb, 0xBF);                            // movabs r15, imm64
    emit_u64(cb, X86_NONE_BITS);                                     // r15 = NONE bit pattern

    uint64_t m = info->live_in | info->live_out;                     // seed live-in AND live-out slots
    while (m) {
        int s = __builtin_ctzll(m);                                  // next slot to seed
        m &= m - 1;
        if (s >= 64) break;
        if (s == table_slot) continue;                               // already in rbx
        if (s == var_reg)    continue;                               // written each iteration
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);      // xmm0 = regs[s]
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));               // frame[s] = xmm0
    }

    int loop_top = (int)cb->len;                                     // loop start address

    emit_u8(cb, 0x4D); emit_u8(cb, 0x39); emit_u8(cb, 0xE6);         // cmp r14, r12
    emit_u8(cb, 0x0F); emit_u8(cb, 0x83);                            // jae rel32
    size_t exit_patch = cb->len;                                     // exit jump placeholder offset
    emit_i32(cb, 0);

    emit_u8(cb, 0x4A); emit_u8(cb, 0x8B); emit_u8(cb, 0x04); emit_u8(cb, 0xF3);  // rax = array_part[r14]
    emit_u8(cb, 0x4D); emit_u8(cb, 0x39); emit_u8(cb, 0xF8);         // cmp rax, r15
    emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                            // je rel32 (skip)
    size_t skip_patch = cb->len;                                     // skip jump placeholder offset
    emit_i32(cb, 0);

    x86_emit_store_r64_rbp(cb, X86_RAX, x86_slot_disp(var_reg));     // frame[var_reg] = element

    XmmCache cache;                                                  // live register cache
    x86_cache_clear(&cache);                                         // body starts with empty cache

    int xv = x86_cache_alloc_excl(&cache, cb, -1, -1);               // xmm to hold the element
    if (xv < 0) JIT_FATAL("cache full: table-iter element reg (pc=%d)", entry);
    x86_emit_movq_xmm_rax(cb, xv);                                   // xv = element bits
    x86_cache_put(&cache, xv, var_reg);                              // cache var_reg in xv

    for (int pc = entry + 1; pc < back_edge; pc++) {                 // emit body
        emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, save_slot, info);
    }

    x86_cache_flush(&cache, cb);                                     // spill dirty slots to frame

    int skip_label = (int)cb->len;                                   // target for hole-skip
    int32_t skip_rel = skip_label - (int32_t)(skip_patch + 4);
    memcpy(cb->buf + skip_patch, &skip_rel, 4);                      // patch skip jump

    emit_u8(cb, 0x49); emit_u8(cb, 0xFF); emit_u8(cb, 0xC6);         // inc r14

    emit_u8(cb, 0xE9);                                               // jmp loop_top
    size_t back_patch = cb->len;
    emit_i32(cb, 0);
    int32_t back_rel = loop_top - (int32_t)(back_patch + 4);
    memcpy(cb->buf + back_patch, &back_rel, 4);

    int exit_label = (int)cb->len;                                   // exit target
    int32_t exit_rel = exit_label - (int32_t)(exit_patch + 4);
    memcpy(cb->buf + exit_patch, &exit_rel, 4);

    m = info->live_out;                                              // write back live_out slots
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        if (s == var_reg) continue;                                  // loop var already written per-iter
        if (info->ref_writes & (1ULL << s)) continue;                // refcounted — keep vm's copy

        x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

        if (s != 0) {                                                // arg0: &pool[s]
            emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0xC1);
#else
            emit_u8(cb, 0xC7);
#endif
            emit_i32(cb, s * 8);
        }

        x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));        // arg1: frame[s]
#if defined(_WIN32) || defined(_WIN64)
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2);     // win64: rdx = value
#else
        emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6);     // sysv: rsi = value
#endif
        x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
        emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);                        // call jit_store_slot
        x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));
    }

    x86_emit_load_r64_rbp(cb, X86_RBX, x86_slot_disp(slot_rbx));     // restore caller's rbx
    x86_emit_load_r64_rbp(cb, X86_R12, x86_slot_disp(slot_r12));     // restore caller's r12
    x86_emit_load_r64_rbp(cb, X86_R13, x86_slot_disp(slot_r13));     // restore caller's r13
    x86_emit_load_r64_rbp(cb, X86_R14, x86_slot_disp(slot_r14));     // restore caller's r14
    x86_emit_load_r64_rbp(cb, X86_R15, x86_slot_disp(slot_r15));     // restore caller's r15

    emit_leave_ret(cb, abi, base_frame);                             // leave; ret

    *out_fn = (void*)(cb->buf + mark);                               // publish entry pointer
    return true;                                                     // emission successful
}

// emits native code for a condition-entry loop: no implicit entry test, the caller already passed
bool x86_64_emit_cond_enter_loop(const X86_64Abi* abi, JITContext* ctx,
                                  CodeBuf* cb, JitLoopInfo* info, void** out_fn) {
    BytecodeChunk* chunk = ctx->chunk;                               // bytecode being compiled
    int entry     = info->entry_pc;                                  // first body pc
    int back_edge = info->back_edge_pc;                              // JUMP back to entry
    int nregs     = info->nregs;                                     // function frame size
    int const_slot = nregs;                                          // reserved slot for constant 1.0

    if (back_edge <= entry) return false;                            // empty body
    int range_size = back_edge - entry + 1;                          // number of body pcs
    if (cb->len + (size_t)range_size * 256 + 1024 > cb->cap)         // capacity check
        JIT_FATAL("code buffer too small for cond-enter loop at pc=%d", entry);

    if (!loop_is_safe_to_emit(ctx, info)) return false;              // needs checks the emitter lacks
    assign_table_caches(ctx, info);                                  // hoist loop-invariant table bases to gprs

    info->n_imms = 0;                                                // precomputed immediates count
    info->imm_opt_ok = true;                                         // all fit in the pool?
    for (int pc = entry; pc < back_edge; pc++) {                     // collect distinct immediates
        Instruction* inst = &chunk->code[pc];
        int32_t imm;                                                 // immediate operand
        switch (inst->opcode) {
            case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
            case OP_DIV_IMM: case OP_MOD_IMM:
                imm = inst->operands[2];
                break;
            default: continue;
        }
        bool found = false;                                          // already pooled?
        for (int i = 0; i < info->n_imms; i++)
            if (info->imms[i].value == imm) { found = true; break; }
        if (found) continue;
        if (info->n_imms >= JIT_MAX_IMMS) { info->imm_opt_ok = false; info->n_imms = 0; break; }
        info->imms[info->n_imms++].value = imm;
    }
    int imm_base_slot = const_slot + 1;                              // first slot for precomputed immediates
    for (int i = 0; i < info->n_imms; i++) info->imms[i].slot = imm_base_slot + i;

    bool needs_helper = false;                                       // does the body call a runtime helper?
    for (int pc = entry; pc < back_edge; pc++) {
        Instruction* inst = &chunk->code[pc];
        switch (inst->opcode) {
            case OP_TABLE_GET_KEY_STR:
            case OP_TABLE_SET_KEY_STR:
            case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
                needs_helper = true;
                break;
            case OP_TABLE_GET: case OP_TABLE_SET:
            case OP_TABLE_GET_NUM: case OP_TABLE_SET_NUM: {
                int key_reg, tbl_reg;                                // unpack operands
                if (inst->opcode == OP_TABLE_GET || inst->opcode == OP_TABLE_GET_NUM) {
                    tbl_reg = inst->operands[1]; key_reg = inst->operands[2];
                } else {
                    tbl_reg = inst->operands[0]; key_reg = inst->operands[1];
                }
                bool is_fast = (key_reg == info->for_var_reg) &&
                               (tbl_reg == info->table.slot);
                if (!is_fast) needs_helper = true;
                break;
            }
            default: break;
        }
        if (needs_helper) break;
    }

    int save_slot = -1;                                              // slot to stash frame_reg across calls
    int frame_slots = const_slot + 1 + info->n_imms;                 // incl. constant slot + immediates
    uint64_t writeback_mask = info->live_out & ~info->ref_writes;    // slots whose old value must be released
    if (needs_helper || writeback_mask != 0) {
        save_slot = frame_slots;                                     // reserve one more slot
        frame_slots++;
    }

    bool need_rbx_save = (info->table.used && info->table.slot >= 0) ||
                         info->uses_globals;
    int rbx_save_slot = -1;                                          // stack slot for caller's rbx
    if (need_rbx_save) {
        rbx_save_slot = frame_slots;
        frame_slots++;
    }
    int cached_gpr_save_slot[4] = { -1, -1, -1, -1 };                // save slots for cached table gprs
    for (int i = 0; i < info->n_cached_tables; i++) {
        cached_gpr_save_slot[i] = frame_slots;                       // one stack slot per cached gpr
        frame_slots++;
    }
    int derived_gpr_save_slot[2] = { -1, -1 };                       // save slots for derived table gprs
    for (int i = 0; i < info->n_derived_caches; i++) {
        derived_gpr_save_slot[i] = frame_slots;
        frame_slots++;
    }

    size_t mark = cb->len;                                           // rollback point

    int base_frame = align16(8 * frame_slots);                       // aligned rbp-relative frame base
    int frame_size = align16(base_frame + abi->frame_extra);         // plus abi extras

    emit_prologue(cb, abi, frame_size, base_frame);                  // push rbp; mov rbp, rsp; sub rsp

    if (info->table.used && info->table.slot >= 0) {                 // preload primary table array_part
        x86_emit_store_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));  // save caller's rbx
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, info->table.slot * 8);  // rax = regs[slot]
        x86_emit_clear_high16_rax(cb);                               // strip nan-box tag
        x86_emit_load_r64_base(cb, X86_RBX, X86_RAX, (int32_t)offsetof(Table, array_part));  // rbx = array_part
    } else if (info->uses_globals) {                                 // preload globals base
        x86_emit_store_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));  // save caller's rbx
        emit_u8(cb, 0x48); emit_u8(cb, 0x89);                        // mov rbx, abi->globals_reg
        emit_u8(cb, 0xC0 | (abi->globals_reg << 3) | X86_RBX);
    }
    for (int i = 0; i < info->n_cached_tables; i++) {                // preload cached table array_parts
        int slot = info->cached_table_slot[i];
        int gpr  = info->cached_table_gpr[i];
        x86_emit_store_r64_rbp(cb, gpr, x86_slot_disp(cached_gpr_save_slot[i]));
        x86_emit_load_r64_base(cb, X86_RAX, abi->frame_reg, slot * 8);
        x86_emit_clear_high16_rax(cb);
        x86_emit_load_r64_base(cb, gpr, X86_RAX, (int32_t)offsetof(Table, array_part));
    }

    x86_emit_movabs_rax(cb, 0x3FF0000000000000ULL);                  // rax = bits of 1.0
    x86_emit_movq_xmm_rax(cb, XMM_SCRATCH);                          // xmm15 = 1.0
    x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(const_slot));  // frame[const_slot] = 1.0
    for (int i = 0; i < info->n_imms; i++) {                         // store precomputed immediates
        x86_emit_load_double_imm(cb, XMM_SCRATCH, info->imms[i].value);
        x86_emit_movsd_store(cb, XMM_SCRATCH, x86_slot_disp(info->imms[i].slot));
    }

    uint64_t m = info->live_in | info->live_out;                     // seed live-in AND live-out slots
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        x86_emit_movsd_load_base(cb, abi->frame_reg, 0, s * 8);
        x86_emit_movsd_store(cb, 0, x86_slot_disp(s));
    }

    for (int i = 0; i < info->n_derived_caches; i++) {               // compute derived invariants
        int gpr     = info->derived_gpr[i];
        int src_gpr = info->derived_src_gpr[i];
        int key     = info->derived_key_slot[i];
        x86_emit_store_r64_rbp(cb, gpr, x86_slot_disp(derived_gpr_save_slot[i]));
        x86_emit_movsd_load(cb, 0, x86_slot_disp(key));
        x86_emit_cvttsd2si_edx(cb, 0);
        x86_emit_dec_edx(cb);
        x86_emit_load_rax_idx8(cb, src_gpr, X86_RDX);
        x86_emit_clear_high16_rax(cb);
        x86_emit_load_r64_base(cb, gpr, X86_RAX, (int32_t)offsetof(Table, array_part));
    }

    XmmCache cache;                                                  // live register cache
    x86_cache_clear(&cache);

    int32_t* label_off = ctx->scratch_label_off;                     // shared label offset array
    JumpFixup* fixups  = ctx->scratch_fixups;                        // shared jump fixup array
    if (!label_off || !fixups)
        JIT_FATAL("scratch arrays missing for cond-enter loop at pc=%d", entry);
    for (int i = 0; i <= range_size; i++) label_off[i] = -1;
    int nfix = 0;

    int loop_top = (int)cb->len;                                     // loop start address

    for (int pc = entry; pc < back_edge; pc++) {
        label_off[pc - entry] = (int32_t)cb->len;
        Instruction* inst = &chunk->code[pc];

        if (pc + 1 < back_edge &&                                    // fused cmp+jump?
            chunk->code[pc + 1].opcode == OP_JUMP_IF_FALSE &&
            chunk->code[pc + 1].operands[1] == inst->operands[0]) {
            uint8_t fused_jcc = 0;                                   // inverted jcc for "jump on false"
            switch (inst->opcode) {
                case OP_CMP_EQ:  case OP_CMP_EQ_NUM:  fused_jcc = 0x85; break;  // jne
                case OP_CMP_NEQ: case OP_CMP_NEQ_NUM: fused_jcc = 0x84; break;  // je
                case OP_CMP_LT:  fused_jcc = 0x83; break;                       // jae
                case OP_CMP_GT:  fused_jcc = 0x86; break;                       // jbe
                case OP_CMP_LTE: fused_jcc = 0x87; break;                       // ja
                case OP_CMP_GTE: fused_jcc = 0x82; break;                       // jb
                default: break;
            }
            bool pc1_is_target = false;                              // is pc+1 already a jump target?
            if (fused_jcc) {
                for (int q = entry; q < pc + 1; q++) {
                    if (chunk->code[q].opcode == OP_JUMP_IF_FALSE &&
                        chunk->code[q].operands[0] == pc + 1) { pc1_is_target = true; break; }
                }
            }
            int fuse_target = fused_jcc ? chunk->code[pc + 1].operands[0] : -1;
            bool d_dead = fused_jcc && !pc1_is_target && fuse_target >= 0 &&
                          !slot_read_before_write(chunk, pc + 2, back_edge, inst->operands[0]) &&
                          (fuse_target < pc + 2 ||
                           !slot_read_before_write(chunk, fuse_target, back_edge, inst->operands[0]));
            if (d_dead) {
                int a = inst->operands[1];
                int b = inst->operands[2];
                int xa = x86_cache_load(&cache, cb, a);
                if (xa < 0) JIT_FATAL("cache full: cond-enter fused CMP a=%d (pc=%d)", a, pc);
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);
                if (xb < 0) JIT_FATAL("cache full: cond-enter fused CMP b=%d (pc=%d)", b, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);                     // ucomisd a, b
                x86_cache_flush(&cache, cb);                         // target may be a merge point
                emit_u8(cb, 0x0F); emit_u8(cb, fused_jcc);           // jcc rel32
                if (nfix >= range_size)
                    JIT_FATAL("fixup overflow in cond-enter loop at pc=%d", entry);
                fixups[nfix].patch_at  = cb->len;                    // record placeholder
                fixups[nfix].target_pc = fuse_target;                // remember target pc
                nfix++;
                emit_i32(cb, 0);
                pc++;                                                // skip the JUMP_IF_FALSE
                continue;
            }
        }

        if (inst->opcode == OP_JUMP_IF_FALSE) {                      // falsy test
            int cond_reg = inst->operands[1];                        // condition register
            int tgt      = inst->operands[0];                        // forward target
            int xa = x86_cache_load(&cache, cb, cond_reg);
            if (xa < 0) JIT_FATAL("cache full: cond-enter JUMP_IF_FALSE cond=%d (pc=%d)", cond_reg, pc);
            x86_emit_movq_rax_xmm(cb, xa);                           // rax = raw 64-bit slot
            emit_u8(cb, 0xA8); emit_u8(cb, 0x01);                    // test al, 1
            bool local_skip = (tgt == pc + 2) &&                     // local skip pattern?
                              (chunk->code[pc + 1].operands[0] == cond_reg) &&
                              (chunk->code[pc + 1].opcode == OP_CMP_EQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_EQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ ||
                               chunk->code[pc + 1].opcode == OP_CMP_NEQ_NUM ||
                               chunk->code[pc + 1].opcode == OP_CMP_LT ||
                               chunk->code[pc + 1].opcode == OP_CMP_GT ||
                               chunk->code[pc + 1].opcode == OP_CMP_LTE ||
                               chunk->code[pc + 1].opcode == OP_CMP_GTE);
            if (!local_skip) x86_cache_flush(&cache, cb);            // flush before branch
            emit_u8(cb, 0x0F); emit_u8(cb, 0x84);                    // je rel32 (bit0 == 0 -> false)
            if (nfix >= range_size)
                JIT_FATAL("fixup overflow in cond-enter loop at pc=%d", entry);
            fixups[nfix].patch_at  = cb->len;
            fixups[nfix].target_pc = tgt;                            // exit vs internal decided at patch time
            nfix++;
            emit_i32(cb, 0);
        } else if (inst->opcode == OP_JUMP_IF_NONE ||
                   inst->opcode == OP_JUMP_IF_NOT_NONE) {            // none-test jumps
            int tgt = inst->operands[0];
            int a   = inst->operands[1];
            int xa  = x86_cache_load(&cache, cb, a);
            if (xa < 0) JIT_FATAL("cache full: cond-enter JUMP_IF_NONE a=%d (pc=%d)", a, pc);
            x86_emit_movq_rax_xmm(cb, xa);                           // rax = raw 64-bit value
            x86_emit_movabs_r11(cb, X86_NONE_BITS);                  // r11 = NONE bit pattern
            x86_emit_cmp_rax_r11(cb);                                // cmp rax, r11
            x86_cache_flush(&cache, cb);
            uint8_t jcc = (inst->opcode == OP_JUMP_IF_NONE) ? 0x84 : 0x85;
            emit_u8(cb, 0x0F); emit_u8(cb, jcc);                     // je / jne rel32
            if (nfix >= range_size)
                JIT_FATAL("fixup overflow in cond-enter loop at pc=%d", entry);
            fixups[nfix].patch_at  = cb->len;
            fixups[nfix].target_pc = tgt;
            nfix++;
            emit_i32(cb, 0);
        } else if (inst->opcode == OP_JUMP_IF_EQ || inst->opcode == OP_JUMP_IF_NEQ ||
                   inst->opcode == OP_JUMP_IF_EQ_NUM || inst->opcode == OP_JUMP_IF_NEQ_NUM ||
                   inst->opcode == OP_JUMP_IF_LT || inst->opcode == OP_JUMP_IF_GT ||
                   inst->opcode == OP_JUMP_IF_LTE || inst->opcode == OP_JUMP_IF_GTE ||
                   inst->opcode == OP_JUMP_IF_EQ_IMM || inst->opcode == OP_JUMP_IF_NEQ_IMM ||
                   inst->opcode == OP_JUMP_IF_LT_IMM || inst->opcode == OP_JUMP_IF_GT_IMM ||
                   inst->opcode == OP_JUMP_IF_LTE_IMM || inst->opcode == OP_JUMP_IF_GTE_IMM) {
            int tgt = inst->operands[0];
            int a   = inst->operands[1];
            int b   = inst->operands[2];
            bool is_imm = (inst->opcode >= OP_JUMP_IF_EQ_IMM &&
                           inst->opcode <= OP_JUMP_IF_GTE_IMM);
            int xa = x86_cache_load(&cache, cb, a);
            if (xa < 0) JIT_FATAL("cache full: cond-enter JUMP_IF cond a=%d (pc=%d)", a, pc);
            if (is_imm) {
                x86_emit_load_double_imm(cb, XMM_SCRATCH, b);
                x86_emit_ucomisd_rr(cb, xa, XMM_SCRATCH);
            } else {
                int xb = x86_cache_load_excl(&cache, cb, b, xa, -1);
                if (xb < 0) JIT_FATAL("cache full: cond-enter JUMP_IF cond b=%d (pc=%d)", b, pc);
                x86_emit_ucomisd_rr(cb, xa, xb);
            }
            x86_cache_flush(&cache, cb);
            uint8_t jcc = jcc_for_entry_op(inst->opcode);
            emit_u8(cb, 0x0F); emit_u8(cb, jcc);
            if (nfix >= range_size)
                JIT_FATAL("fixup overflow in cond-enter loop at pc=%d", entry);
            fixups[nfix].patch_at  = cb->len;
            fixups[nfix].target_pc = tgt;
            nfix++;
            emit_i32(cb, 0);
        } else {                                                     // non-jump body instruction
            emit_loop_body_instr(abi, ctx, cb, &cache, pc, const_slot, save_slot, info);
        }
    }
    label_off[back_edge - entry] = (int32_t)cb->len;                 // label for the back-edge pc

    x86_cache_flush_back_edge(&cache, cb, chunk, entry, back_edge, 0);

    emit_u8(cb, 0xE9);                                               // jmp loop_top
    int32_t back_rel = loop_top - (int32_t)(cb->len + 4);
    emit_i32(cb, back_rel);

    int exit_label = (int)cb->len;                                   // exit target

    for (int i = 0; i < nfix; i++) {                                 // patch every recorded jump
        int tgt = fixups[i].target_pc;
        int32_t rel;
        if (tgt == info->exit_pc) {
            rel = exit_label - (int32_t)(fixups[i].patch_at + 4);
        } else {
            int idx = tgt - entry;
            if (idx < 0 || idx > range_size || label_off[idx] < 0)
                JIT_FATAL("jump target out of range in cond-enter loop (pc=%d tgt=%d)", entry, tgt);
            rel = label_off[idx] - (int32_t)(fixups[i].patch_at + 4);
        }
        memcpy(cb->buf + fixups[i].patch_at, &rel, 4);
    }

    m = info->live_out;                                              // write back live_out slots
    while (m) {
        int s = __builtin_ctzll(m);
        m &= m - 1;
        if (s >= 64) break;
        if (info->ref_writes & (1ULL << s)) continue;                // refcounted value — keep vm's copy

        if (save_slot >= 0) {
            x86_emit_store_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));  // save frame_reg

            if (s != 0) {                                            // arg0: &pool[s]
                emit_u8(cb, 0x48); emit_u8(cb, 0x81);
#if defined(_WIN32) || defined(_WIN64)
                emit_u8(cb, 0xC1);
#else
                emit_u8(cb, 0xC7);
#endif
                emit_i32(cb, s * 8);
            }

            x86_emit_load_r64_rbp(cb, X86_RAX, x86_slot_disp(s));    // arg1: frame[s]
#if defined(_WIN32) || defined(_WIN64)
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC2); // win64: rdx = value
#else
            emit_u8(cb, 0x48); emit_u8(cb, 0x89); emit_u8(cb, 0xC6); // sysv: rsi = value
#endif
            x86_emit_movabs_rax(cb, (uint64_t)(uintptr_t)&jit_store_slot);
            emit_u8(cb, 0xFF); emit_u8(cb, 0xD0);

            x86_emit_load_r64_rbp(cb, abi->frame_reg, x86_slot_disp(save_slot));
        } else {
            x86_emit_movsd_load(cb, 0, x86_slot_disp(s));            // plain store, no refcount
            x86_emit_movsd_store_base(cb, abi->frame_reg, 0, s * 8);
        }
    }

    for (int i = 0; i < info->n_cached_tables; i++) {                // restore cached table gprs
        x86_emit_load_r64_rbp(cb, info->cached_table_gpr[i], x86_slot_disp(cached_gpr_save_slot[i]));
    }
    for (int i = 0; i < info->n_derived_caches; i++) {               // restore derived table gprs
        x86_emit_load_r64_rbp(cb, info->derived_gpr[i], x86_slot_disp(derived_gpr_save_slot[i]));
    }

    if (need_rbx_save) {
        x86_emit_load_r64_rbp(cb, X86_RBX, x86_slot_disp(rbx_save_slot));   // restore caller's rbx
    }

    emit_leave_ret(cb, abi, base_frame);                             // leave; ret

    *out_fn = (void*)(cb->buf + mark);                               // publish entry pointer
    return true;                                                     // emission successful
}

// emits native code for a single native loop, dispatching by kind
bool x86_64_emit_loop(const X86_64Abi* abi, JITContext* ctx, CodeBuf* cb,
                      JitLoopInfo* info, int step_sign, void** out_fn) {
    if (info->kind == JIT_LOOP_TABLE_ITER) {                         // table iteration loop
        return x86_64_emit_table_iter_loop(abi, ctx, cb, info, out_fn);
    }
    if (info->kind == JIT_LOOP_COND_ENTER) {                         // condition-entry loop
        return x86_64_emit_cond_enter_loop(abi, ctx, cb, info, out_fn);
    }
    return x86_64_emit_numeric_loop(abi, ctx, cb, info, step_sign, out_fn);  // numeric or condition loop
}
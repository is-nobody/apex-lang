// source/compiler/opt_bc_dce.c
// Bytecode-level local dead-store elimination
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// true when an instruction is a semantic no-op
static bool inst_is_nop(Instruction* inst) {
    return inst->opcode == OP_MOVE &&
           inst->operands[0] == inst->operands[1];
}

// true when control does not fall through past this opcode
static bool inst_is_terminator(Opcode op) {
    switch (op) {
        case OP_JUMP:
        case OP_RETURN: case OP_RETURN_NUM: case OP_RETURN_NUM_IMM:
        case OP_RETURN_BOOL: case OP_RETURN_NONE:
        case OP_HALT:
            return true;
        default:
            return false;
    }
}

// replaces a dead store with MOVE R0, R0 (peephole-friendly no-op)
static void inst_make_nop(Instruction* inst) {
    inst->opcode = OP_MOVE;
    inst->operands[0] = 0;
    inst->operands[1] = 0;
    inst->operands[2] = 0;
}

// removes pure stores whose destination is never read before being overwritten
int dce_local_range(CodeGenerator* cg, int from_pc, int to_pc) {
    if (from_pc >= to_pc) return 0;                          // empty range
    if (!inst_is_terminator(cg->chunk->code[to_pc - 1].opcode))
        return 0;                                            // fall-through: unsafe

    int removed = 0;                                         // stores killed
    for (int pc = from_pc; pc < to_pc; pc++) {
        Instruction* in = &cg->chunk->code[pc];              // candidate store
        if (inst_is_nop(in)) continue;                       // already a no-op
        if (!op_writes_dest_reg(in->opcode)) continue;       // must write operands[0]
        if (!op_is_pure(in->opcode)) continue;               // must be side-effect free

        int d = in->operands[0];                             // destination register
        if (d < cg->cache_floor) continue;                   // pinned by a const cache

        bool dead = true;                                    // assume dead until proven live
        for (int q = pc + 1; q < to_pc; q++) {               // scan forward
            Instruction* nx = &cg->chunk->code[q];
            if (inst_is_nop(nx)) continue;                   // no-ops neither read nor write
            if (inst_reads_reg(nx, d)) { dead = false; break; }   // value used
            if (op_writes_dest_reg(nx->opcode) &&
                nx->operands[0] == d) break;                 // overwritten: dead
        }
        if (dead) {                                          // store is dead
            imm_lvn_invalidate(cg, d);                       // drop LVN entries for d
            inst_make_nop(in);                               // replace with MOVE R0, R0
            removed++;
        }
    }
    return removed;
}

// true when the opcode's operands[0] is a code offset that must be remapped
bool op_has_pc_in_op0(Opcode op) {
    if (op == OP_JUMP) return true;
    if (op >= OP_JUMP_IF_FALSE && op <= OP_JUMP_IF_GTE) return true;
    if (op >= OP_JUMP_IF_EQ_IMM && op <= OP_JUMP_IF_GTE_IMM) return true;
    if (op == OP_JUMP_MATCH_NUM || op == OP_JUMP_MATCH_STR ||
        op == OP_JUMP_MATCH_BOOL || op == OP_JUMP_MATCH_NONE) return true;
    return false;
}

// drops every MOVE R,R no-op, every JUMP that lands on the instruction immediately after it
void compact_bytecode(CodeGenerator* cg) {
    int total = cg->chunk->code_count;
    if (total == 0) return;

    // pass 1: mark every instruction that will be dropped
    uint8_t* remove = (uint8_t*)calloc(total, 1);
    for (int pc = 0; pc < total; pc++) {
        if (inst_is_nop(&cg->chunk->code[pc])) remove[pc] = 1;    // MOVE R,R
    }

    // pass 2: iteratively mark JUMP -> next-pc as removable
    int* tmp_map = (int*)malloc(sizeof(int) * (total + 1));
    bool changed = true;
    while (changed) {
        changed = false;
        int nc = 0;                                               // running compacted pc
        for (int pc = 0; pc < total; pc++) {
            tmp_map[pc] = nc;
            if (!remove[pc]) nc++;
        }
        tmp_map[total] = nc;
        for (int pc = 0; pc < total; pc++) {
            if (remove[pc]) continue;
            Instruction* in = &cg->chunk->code[pc];
            if (in->opcode != OP_JUMP) continue;                  // only unconditional JUMP
            int T = in->operands[0];
            if (T < 0 || T > total) continue;
            if (tmp_map[T] == tmp_map[pc] + 1) {                  // jump lands on next kept
                remove[pc] = 1;
                changed = true;
            }
        }
    }
    free(tmp_map);

    // pass 3: build the final map and shift kept instructions down
    int* map = (int*)malloc(sizeof(int) * (total + 1));
    int new_count = 0;
    for (int pc = 0; pc < total; pc++) {
        map[pc] = new_count;                                      // dropped pc maps to next kept
        if (remove[pc]) continue;
        if (new_count != pc) cg->chunk->code[new_count] = cg->chunk->code[pc];
        new_count++;
    }
    map[total] = new_count;                                       // sentinel for end-of-chunk
    cg->chunk->code_count = new_count;
    free(remove);

    for (int pc = 0; pc < new_count; pc++) {                      // rewrite every PC-bearing operand
        Instruction* in = &cg->chunk->code[pc];
        if (op_has_pc_in_op0(in->opcode)) {
            int old = in->operands[0];
            if (old >= 0 && old <= total) in->operands[0] = map[old];
        }
        if (in->opcode == OP_FOR_NEXT || in->opcode == OP_FOR_NEXT_LOOP) {
            int old = in->operands[1];
            if (old >= 0 && old <= total) in->operands[1] = map[old];
        }
        if (in->opcode == OP_TABLE_ITER_NEXT) {
            int old = in->operands[2];
            if (old >= 0 && old <= total) in->operands[2] = map[old];
        }
    }

    for (int i = 0; i < cg->chunk->func_count; i++) {             // function entry addresses
        int old = cg->chunk->functions[i].address;
        if (old >= 0 && old <= total) cg->chunk->functions[i].address = map[old];
    }

    for (int i = 0; i < cg->chunk->const_count; i++) {            // jump table target addresses
        Constant* c = &cg->chunk->constants[i];
        if (c->type != CONST_JUMP_TABLE) continue;
        for (int j = 0; j < c->jump_table.count; j++) {
            int old = c->jump_table.addresses[j];
            if (old >= 0 && old <= total) c->jump_table.addresses[j] = map[old];
        }
    }

    if (cg->jump_targets) {                                       // rebuild the bitset at new pcs
        memset(cg->jump_targets, 0, cg->jump_targets_cap);
        for (int pc = 0; pc < new_count; pc++) {
            Instruction* in = &cg->chunk->code[pc];
            if (op_has_pc_in_op0(in->opcode)) mark_jump_target(cg, in->operands[0]);
            if (in->opcode == OP_FOR_NEXT || in->opcode == OP_FOR_NEXT_LOOP)
                mark_jump_target(cg, in->operands[1]);
            if (in->opcode == OP_TABLE_ITER_NEXT)
                mark_jump_target(cg, in->operands[2]);
        }
        for (int i = 0; i < cg->chunk->const_count; i++) {
            Constant* c = &cg->chunk->constants[i];
            if (c->type != CONST_JUMP_TABLE) continue;
            for (int j = 0; j < c->jump_table.count; j++) {
                mark_jump_target(cg, c->jump_table.addresses[j]);
            }
        }
    }

    free(map);
}

// true when operands[1] holds a func_idx (direct call)
static bool op_is_direct_call(Opcode op) {
    return op == OP_CALL || op == OP_CALL_0 || op == OP_CALL_1 ||
           op == OP_CALL_2 || op == OP_ASYNC_CALL;
}

// drops dead entries from the function table and constant pool
static void compact_functions_and_constants(CodeGenerator* cg, uint8_t* used_func) {
    int n  = cg->chunk->code_count;
    int nf = cg->chunk->func_count;
    int nc = cg->chunk->const_count;

    // 1. function table compaction
    int* fmap = (int*)malloc(sizeof(int) * (nf > 0 ? nf : 1));
    int new_nf = 0;
    for (int f = 0; f < nf; f++) fmap[f] = used_func[f] ? new_nf++ : -1;

    if (new_nf < nf) {
        for (int pc = 0; pc < n; pc++) {                          // remap call operands
            Instruction* in = &cg->chunk->code[pc];
            if (!op_is_direct_call(in->opcode)) continue;
            int t = in->operands[1];
            if (t >= 0 && t < nf && fmap[t] >= 0) in->operands[1] = fmap[t];
        }
        for (int i = 0; i < nc; i++) {                            // remap CONST_FUNCTION refs
            Constant* c = &cg->chunk->constants[i];
            if (c->type != CONST_FUNCTION) continue;
            int fi = c->function_index;
            if (fi >= 0 && fi < nf && fmap[fi] >= 0) c->function_index = fmap[fi];
        }
        int w = 0;                                                // compact in place
        for (int f = 0; f < nf; f++) {
            if (!used_func[f]) continue;
            if (w != f) cg->chunk->functions[w] = cg->chunk->functions[f];
            w++;
        }
        cg->chunk->func_count = new_nf;
    }
    free(fmap);

    if (nc == 0) return;

    // 2. mark every constant referenced by a live instruction
    uint8_t* used_c = (uint8_t*)calloc(nc, 1);
    for (int pc = 0; pc < n; pc++) {
        Instruction* in = &cg->chunk->code[pc];
        switch (in->opcode) {
            case OP_LOAD_CONST: case OP_LOAD_NUM: case OP_LOAD_TABLE:
            case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:
            case OP_TABLE_SET_CONST: {
                int ci = in->operands[1];
                if (ci >= 0 && ci < nc) used_c[ci] = 1;
                break;
            }
            case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:
            case OP_TABLE_GET_CONST:
            case OP_JUMP_TABLE: {                                  // jump-table const in op2
                int ci = in->operands[2];
                if (ci >= 0 && ci < nc) used_c[ci] = 1;
                break;
            }
            case OP_TABLE_GET_KEY_STR: case OP_TABLE_SET_KEY_STR: {
                int ci = (in->operands[2] >> 16) & 0xFFFF;         // packed high half
                if (ci >= 0 && ci < nc) used_c[ci] = 1;
                break;
            }
            default: break;
        }
    }

    // 2b. propagate liveness through CONST_TABLE internal indices
    {
        int* wl = (int*)malloc(sizeof(int) * nc);
        int wn = 0;
        for (int i = 0; i < nc; i++) if (used_c[i]) wl[wn++] = i;
        while (wn > 0) {
            int ci = wl[--wn];
            Constant* c = &cg->chunk->constants[ci];
            if (c->type != CONST_TABLE) continue;
            for (int i = 0; i < c->table.hash_count; i++) {
                int ki = c->table.key_indices[i];
                if (ki >= 0 && ki < nc && !used_c[ki]) {
                    used_c[ki] = 1;
                    wl[wn++] = ki;
                }
            }
            int total = c->table.array_count + c->table.hash_count;
            for (int i = 0; i < total; i++) {
                int vi = c->table.value_indices[i];
                if (vi >= 0 && vi < nc && !used_c[vi]) {
                    used_c[vi] = 1;
                    wl[wn++] = vi;
                }
            }
        }
        free(wl);
    }

    // 3. constant pool compaction
    int* cmap = (int*)malloc(sizeof(int) * nc);
    int new_nc = 0;
    for (int i = 0; i < nc; i++) cmap[i] = used_c[i] ? new_nc++ : -1;

    if (new_nc < nc) {
        for (int pc = 0; pc < n; pc++) {                          // remap const operands
            Instruction* in = &cg->chunk->code[pc];
            switch (in->opcode) {
                case OP_LOAD_CONST: case OP_LOAD_NUM: case OP_LOAD_TABLE:
                case OP_CALL_BUILTIN: case OP_ASYNC_CALL_BUILTIN:
                case OP_TABLE_SET_CONST: {
                    int ci = in->operands[1];
                    if (ci >= 0 && ci < nc && cmap[ci] >= 0) in->operands[1] = cmap[ci];
                    break;
                }
                case OP_JUMP_MATCH_NUM: case OP_JUMP_MATCH_STR:
                case OP_TABLE_GET_CONST:
                case OP_JUMP_TABLE: {
                    int ci = in->operands[2];
                    if (ci >= 0 && ci < nc && cmap[ci] >= 0) in->operands[2] = cmap[ci];
                    break;
                }
                case OP_TABLE_GET_KEY_STR: case OP_TABLE_SET_KEY_STR: {
                    int ci      = (in->operands[2] >> 16) & 0xFFFF;
                    int num_reg =  in->operands[2]        & 0xFFFF;
                    if (ci >= 0 && ci < nc && cmap[ci] >= 0)
                        in->operands[2] = (cmap[ci] << 16) | num_reg;
                    break;
                }
                default: break;
            }
        }

        for (int i = 0; i < nc; i++) {                            // remap table-internal refs
            if (!used_c[i]) continue;
            Constant* c = &cg->chunk->constants[i];
            if (c->type != CONST_TABLE) continue;
            for (int k = 0; k < c->table.hash_count; k++) {
                int ki = c->table.key_indices[k];
                if (ki >= 0 && ki < nc && cmap[ki] >= 0)
                    c->table.key_indices[k] = cmap[ki];
            }
            int total = c->table.array_count + c->table.hash_count;
            for (int k = 0; k < total; k++) {
                int vi = c->table.value_indices[k];
                if (vi >= 0 && vi < nc && cmap[vi] >= 0)
                    c->table.value_indices[k] = cmap[vi];
            }
        }

        for (int i = 0; i < nc; i++) {                            // release dropped tables' buffers
            if (used_c[i]) continue;
            Constant* c = &cg->chunk->constants[i];
            if (c->type == CONST_TABLE) {
                free(c->table.key_indices);
                free(c->table.value_indices);
                c->table.key_indices = NULL;
                c->table.value_indices = NULL;
            }
        }

        int w = 0;                                                // compact in place
        for (int i = 0; i < nc; i++) {
            if (!used_c[i]) continue;
            if (w != i) cg->chunk->constants[w] = cg->chunk->constants[i];
            w++;
        }
        cg->chunk->const_count = new_nc;
    }
    free(cmap);
    free(used_c);
}

// drops functions never reachable from entry
void dce_functions_and_globals(CodeGenerator* cg) {
    int n  = cg->chunk->code_count;
    int nf = cg->chunk->func_count;
    if (n == 0 || nf <= 1) return;
    if (!cg->fn_cache.end_pc) return;  // requires fn_cache from codegen_fn

    // pass 1: fn->global bindings, raw refs, max global index
    int*     bind_global = (int*)malloc(sizeof(int) * nf);      // fn -> global idx or -1
    int*     bind_pc     = (int*)malloc(sizeof(int) * nf);      // pc of LOAD_CONST or -1
    int*     raw_ref_pc  = (int*)malloc(sizeof(int) * nf);      // pc of raw LOAD_CONST
    int*     raw_ref_fn  = (int*)malloc(sizeof(int) * nf);      // fn referenced there
    int      raw_ref_n   = 0;
    for (int f = 0; f < nf; f++) { bind_global[f] = -1; bind_pc[f] = -1; }

    int max_global = -1;
    for (int pc = 0; pc < n; pc++) {
        Instruction* in = &cg->chunk->code[pc];
        switch (in->opcode) {
            case OP_LOAD_GLOBAL:
            case OP_STORE_GLOBAL:
                if (in->operands[1] > max_global) max_global = in->operands[1];
                break;
            case OP_LOAD_CONST: {
                int cidx = in->operands[1];
                if (cidx < 0 || cidx >= cg->chunk->const_count) break;
                Constant* c = &cg->chunk->constants[cidx];
                if (c->type != CONST_FUNCTION) break;
                int f = c->function_index;
                if (f < 0 || f >= nf) break;
                if (pc + 1 < n &&                                    // paired with STORE_GLOBAL?
                    cg->chunk->code[pc + 1].opcode == OP_STORE_GLOBAL &&
                    cg->chunk->code[pc + 1].operands[0] == in->operands[0]) {
                    int g = cg->chunk->code[pc + 1].operands[1];
                    bind_global[f] = g;
                    bind_pc[f]     = pc;
                    if (g > max_global) max_global = g;
                } else if (raw_ref_n < nf) {                         // value escapes: conservative
                    raw_ref_pc[raw_ref_n] = pc;
                    raw_ref_fn[raw_ref_n] = f;
                    raw_ref_n++;
                }
                break;
            }
            default: break;
        }
    }

    // pc -> owning function (0 = entry, default)
    int* pc_owner = (int*)calloc(n, sizeof(int));
    for (int f = 1; f < nf; f++) {
        int a = cg->chunk->functions[f].address;
        int e = cg->fn_cache.end_pc[f];
        if (a < 0 || e < 0 || e > n || a >= e) continue;
        for (int pc = a; pc < e; pc++) pc_owner[pc] = f;
    }

    // call edges grouped by owner
    int* edge_head   = (int*)malloc(sizeof(int) * nf);
    int* edge_next   = (int*)malloc(sizeof(int) * (n > 0 ? n : 1));
    int* edge_target = (int*)malloc(sizeof(int) * (n > 0 ? n : 1));
    for (int f = 0; f < nf; f++) edge_head[f] = -1;
    int edge_count = 0;
    for (int pc = 0; pc < n; pc++) {
        Instruction* in = &cg->chunk->code[pc];
        if (!op_is_direct_call(in->opcode)) continue;
        int t = in->operands[1];
        if (t < 0 || t >= nf) continue;
        int owner = pc_owner[pc];
        edge_target[edge_count] = t;
        edge_next[edge_count]   = edge_head[owner];
        edge_head[owner]        = edge_count;
        edge_count++;
    }

    // fixed point (3 iterations is plenty in practice)
    uint8_t* used_func   = (uint8_t*)calloc(nf, 1);
    uint8_t* used_global = max_global >= 0
        ? (uint8_t*)calloc(max_global + 1, 1) : NULL;
    int* queue = (int*)malloc(sizeof(int) * nf);

    for (int iter = 0; iter < 3; iter++) {
        // (a) globals loaded by used functions only
        if (used_global) {
            memset(used_global, 0, max_global + 1);
            for (int pc = 0; pc < n; pc++) {
                Instruction* in = &cg->chunk->code[pc];
                if (in->opcode != OP_LOAD_GLOBAL) continue;
                int g = in->operands[1];
                if (g < 0 || !used_func[pc_owner[pc]]) continue;
                used_global[g] = 1;
            }
        }

        // (b) recompute used functions from roots + bfs
        memset(used_func, 0, nf);
        int qh = 0, qt = 0;
        used_func[0] = 1;
        queue[qt++] = 0;
        for (int i = 0; i < raw_ref_n; i++) {                    // raw ref from a used fn
            int f = raw_ref_fn[i];
            if (!used_func[f] && used_func[pc_owner[raw_ref_pc[i]]]) {
                used_func[f] = 1;
                queue[qt++] = f;
            }
        }
        for (int f = 1; f < nf; f++) {                           // binding global is loaded
            if (!used_func[f] && bind_global[f] >= 0 && used_global &&
                used_global[bind_global[f]]) {
                used_func[f] = 1;
                queue[qt++] = f;
            }
        }
        while (qh < qt) {                                        // transitive callees
            int f = queue[qh++];
            for (int e = edge_head[f]; e != -1; e = edge_next[e]) {
                int t = edge_target[e];
                if (!used_func[t]) { used_func[t] = 1; queue[qt++] = t; }
            }
        }
    }

    // rewrite: nop unused fn bodies + their bindings
    for (int f = 1; f < nf; f++) {
        if (used_func[f]) continue;
        int a = cg->chunk->functions[f].address;
        int e = cg->fn_cache.end_pc[f];
        if (a >= 0 && e > a && e <= n) {
            for (int pc = a; pc < e; pc++) inst_make_nop(&cg->chunk->code[pc]);
        }
        if (bind_pc[f] >= 0 && bind_pc[f] + 1 < n) {
            inst_make_nop(&cg->chunk->code[bind_pc[f]]);
            inst_make_nop(&cg->chunk->code[bind_pc[f] + 1]);
        }
    }

    // rewrite: nop stores to globals that are never loaded
    if (used_global) {
        for (int pc = 0; pc < n; pc++) {
            Instruction* in = &cg->chunk->code[pc];
            if (in->opcode == OP_STORE_GLOBAL && in->operands[1] >= 0 &&
                !used_global[in->operands[1]]) {
                inst_make_nop(in);
            }
        }
    }

    compact_functions_and_constants(cg, used_func);  // drop dead table + pool entries

    free(bind_global); free(bind_pc);
    free(raw_ref_pc);  free(raw_ref_fn);
    free(pc_owner);
    free(edge_head);   free(edge_next); free(edge_target);
    free(used_func);   free(used_global); free(queue);
}
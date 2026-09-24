// source/compiler/opt_peephole.c
// Peephole instruction fusion for adjacent bytecode pairs
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <math.h>

// pure ops: writes operands[0] with no observable side effect
static bool op_is_pure(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_CONST: case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_GLOBAL:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_AND: case OP_OR: case OP_NOT:
        case OP_TABLE_GET: case OP_TABLE_GET_CONST: case OP_TABLE_GET_INT:
        case OP_CONCAT: case OP_NEW_TABLE:
            return true;
        default:
            return false;
    }
}

// does inst read `reg` as a source operand? op-specific, respects operand types
static bool inst_reads_reg(Instruction* inst, int reg) {
    switch (inst->opcode) {
        case OP_LOAD_NUM_IMM: case OP_LOAD_NUM: case OP_LOAD_CONST:
        case OP_LOAD_BOOL: case OP_LOAD_NONE: case OP_LOAD_GLOBAL:
            return false;                                       // no source registers
        case OP_MOVE: case OP_NEG: case OP_ADD: case OP_SUB:
        case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_AND: case OP_OR: case OP_CONCAT:
            return inst->operands[1] == reg || inst->operands[2] == reg;
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
            return inst->operands[1] == reg;                    // operand 2 is immediate
        case OP_INC: case OP_DEC:
            return inst->operands[0] == reg;                    // reads its own dest
        case OP_TABLE_GET: case OP_TABLE_GET_NUM:
            return inst->operands[1] == reg || inst->operands[2] == reg;
        case OP_TABLE_GET_INT:
            return inst->operands[1] == reg;
        default:
            return true;                                        // conservative
    }
}

// fuse this instruction into the previous one; returns fused pc or -1
int try_peephole_fuse(CodeGenerator* cg, Instruction* inst) {
    if (cg->chunk->code_count == 0) return -1;
    Instruction* prev = &cg->chunk->code[cg->chunk->code_count - 1];
    int d = inst->operands[0], a = inst->operands[1], b = inst->operands[2];

    // bail when a jump lands on the slot we would drop
    if (code_has_jump_to(cg, cg->chunk->code_count)) return -1;

    // MOVE d,d as the incoming instruction is a no-op regardless of prev
    if (inst->opcode == OP_MOVE && inst->operands[0] == inst->operands[1]) {
        return cg->chunk->code_count - 1;
    }

    // MOVE d,d as prev is a no-op: move the incoming inst into prev's slot
    if (prev->opcode == OP_MOVE && prev->operands[0] == prev->operands[1]) {
        prev->opcode = inst->opcode;
        prev->operands[0] = d;
        prev->operands[1] = a;
        prev->operands[2] = b;
        return cg->chunk->code_count - 1;
    }

    // LOAD_NONE d + RETURN|RETURN_NUM|RETURN_BOOL d  ->  RETURN_NONE
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM ||
         inst->opcode == OP_RETURN_BOOL) &&
        prev->opcode == OP_LOAD_NONE && prev->operands[0] == d) {
        prev->opcode = OP_RETURN_NONE;
        prev->operands[0] = prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // LOAD_NUM_IMM d,k + RETURN|RETURN_NUM d  ->  RETURN_NUM_IMM k
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM) &&
        prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        prev->operands[1] >= 0 && prev->operands[1] <= 65535) {
        int k = prev->operands[1];
        prev->opcode = OP_RETURN_NUM_IMM;
        prev->operands[0] = 0;
        prev->operands[1] = k;
        prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // MOVE d,a + RETURN|RETURN_NUM|RETURN_BOOL d  ->  RETURN* a
    if ((inst->opcode == OP_RETURN || inst->opcode == OP_RETURN_NUM ||
         inst->opcode == OP_RETURN_BOOL) &&
        prev->opcode == OP_MOVE && prev->operands[0] == d &&
        prev->operands[1] != d) {
        int ma = prev->operands[1];
        prev->opcode = inst->opcode;
        prev->operands[0] = ma;
        prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // <any pure write> + RETURN_NONE  ->  RETURN_NONE
    if (inst->opcode == OP_RETURN_NONE && op_is_pure(prev->opcode)) {
        prev->opcode = OP_RETURN_NONE;
        prev->operands[0] = prev->operands[1] = prev->operands[2] = 0;
        return cg->chunk->code_count - 1;
    }

    // <pure write to d> + <write to d not reading d>  ->  drop prev, keep inst
    if (op_writes_dest_reg(inst->opcode) && op_is_pure(prev->opcode) &&
        op_writes_dest_reg(prev->opcode) && prev->operands[0] == d &&
        !inst_reads_reg(inst, d)) {
        prev->opcode = inst->opcode;
        prev->operands[0] = d;
        prev->operands[1] = a;
        prev->operands[2] = b;
        return cg->chunk->code_count - 1;
    }

    // MOVE d,a + <write d reading d>  ->  substitute a for d in sources
    if (prev->opcode == OP_MOVE && op_writes_dest_reg(inst->opcode) &&
        prev->operands[0] == d && prev->operands[1] != d) {
        int ma = prev->operands[1];
        prev->opcode = inst->opcode;
        prev->operands[0] = d;
        prev->operands[1] = (a == d) ? ma : a;
        prev->operands[2] = (b == d) ? ma : b;
        return cg->chunk->code_count - 1;
    }

    // MOVE md,ms + <op reading md as source, writing != md>  ->  substitute ms for md in sources
    if (prev->opcode == OP_MOVE && op_writes_dest_reg(inst->opcode)) {
        int md = prev->operands[0];
        int ms = prev->operands[1];
        if (md != ms && md != d) {
            bool changed = false;
            switch (inst->opcode) {
                case OP_MOVE:
                    if (inst->operands[1] == md) { inst->operands[1] = ms; changed = true; }
                    break;
                case OP_NEG: case OP_ADD: case OP_SUB:
                case OP_MUL: case OP_DIV: case OP_MOD:
                case OP_CMP_EQ: case OP_CMP_NEQ:
                case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
                case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
                case OP_AND: case OP_OR: case OP_CONCAT:
                case OP_TABLE_GET: case OP_TABLE_GET_NUM:
                    if (inst->operands[1] == md) { inst->operands[1] = ms; changed = true; }
                    if (inst->operands[2] == md) { inst->operands[2] = ms; changed = true; }
                    break;
                case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
                case OP_DIV_IMM: case OP_MOD_IMM:
                case OP_TABLE_GET_INT:
                    if (inst->operands[1] == md) { inst->operands[1] = ms; changed = true; }
                    break;
                default:
                    break;
            }
            if (changed) return -1;
        }
    }

    // LOAD_NUM_IMM d,k + ADD_IMM|SUB_IMM|MUL_IMM|DIV_IMM|MOD_IMM d,d,m
    if (prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        inst->operands[1] == d &&
        (inst->opcode == OP_ADD_IMM || inst->opcode == OP_SUB_IMM ||
         inst->opcode == OP_MUL_IMM || inst->opcode == OP_DIV_IMM ||
         inst->opcode == OP_MOD_IMM)) {
        int k = prev->operands[1];
        int m = b;
        double r = 0.0;
        bool ok = true;
        switch (inst->opcode) {
            case OP_ADD_IMM: r = (double)k + (double)m; break;
            case OP_SUB_IMM: r = (double)k - (double)m; break;
            case OP_MUL_IMM: r = (double)k * (double)m; break;
            case OP_DIV_IMM: if (m == 0) ok = false; else r = (double)k / (double)m; break;
            case OP_MOD_IMM: if (m == 0) ok = false; else r = fmod((double)k, (double)m); break;
            default: ok = false; break;
        }
        if (ok) {
            if (r == (double)(int)r && r >= 0 && r <= 65535) {
                prev->opcode = OP_LOAD_NUM_IMM;
                prev->operands[0] = d;
                prev->operands[1] = (int)r;
                prev->operands[2] = 0;
            } else {
                int ci = bytecode_add_number_constant(cg->chunk, r);
                prev->opcode = OP_LOAD_NUM;
                prev->operands[0] = d;
                prev->operands[1] = ci;
                prev->operands[2] = 0;
            }
            return cg->chunk->code_count - 1;
        }
    }

    // LOAD_NUM_IMM d,k + NEG d,d  ->  LOAD_NUM_IMM/LOAD_NUM d,-k
    if (prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        inst->opcode == OP_NEG && inst->operands[0] == d &&
        inst->operands[1] == d) {
        double r = -(double)prev->operands[1];
        if (r == (double)(int)r && r >= 0 && r <= 65535) {
            prev->opcode = OP_LOAD_NUM_IMM;
            prev->operands[0] = d;
            prev->operands[1] = (int)r;
            prev->operands[2] = 0;
        } else {
            int ci = bytecode_add_number_constant(cg->chunk, r);
            prev->opcode = OP_LOAD_NUM;
            prev->operands[0] = d;
            prev->operands[1] = ci;
            prev->operands[2] = 0;
        }
        return cg->chunk->code_count - 1;
    }

    // LOAD_NUM_IMM d,k + INC d  ->  LOAD_NUM_IMM d,k+1  (k < 65535)
    if (prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        inst->opcode == OP_INC && inst->operands[0] == d &&
        prev->operands[1] < 65535) {
        prev->operands[1] += 1;
        return cg->chunk->code_count - 1;
    }

    // LOAD_NUM_IMM d,k + DEC d  ->  LOAD_NUM_IMM d,k-1  (k >= 1)
    if (prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        inst->opcode == OP_DEC && inst->operands[0] == d &&
        prev->operands[1] > 0) {
        prev->operands[1] -= 1;
        return cg->chunk->code_count - 1;
    }

    // LOAD_NUM_IMM d,k + ADD|SUB|MUL|DIV|MOD d,d,d  ->  LOAD_NUM d,fold
    if (prev->opcode == OP_LOAD_NUM_IMM && prev->operands[0] == d &&
        inst->operands[0] == d && inst->operands[1] == d &&
        inst->operands[2] == d) {
        double k = (double)prev->operands[1];
        double r = 0.0;
        bool ok = true;
        switch (inst->opcode) {
            case OP_ADD: r = k + k; break;
            case OP_SUB: r = 0.0;   break;
            case OP_MUL: r = k * k; break;
            case OP_DIV: if (k == 0) ok = false; else r = 1.0; break;
            case OP_MOD: if (k == 0) ok = false; else r = 0.0; break;
            default: ok = false; break;
        }
        if (ok) {
            if (r == (double)(int)r && r >= 0 && r <= 65535) {
                prev->opcode = OP_LOAD_NUM_IMM;
                prev->operands[0] = d;
                prev->operands[1] = (int)r;
                prev->operands[2] = 0;
            } else {
                int ci = bytecode_add_number_constant(cg->chunk, r);
                prev->opcode = OP_LOAD_NUM;
                prev->operands[0] = d;
                prev->operands[1] = ci;
                prev->operands[2] = 0;
            }
            return cg->chunk->code_count - 1;
        }
    }

    // LOAD_NUM_IMM d,k + JUMP_IF_<cmp>_IMM d,m,tgt, statically taken
    if (prev->opcode == OP_LOAD_NUM_IMM &&
        prev->operands[0] == inst->operands[1]) {
        int k = prev->operands[1];
        int m = inst->operands[2];
        bool taken = false, known = false;
        switch (inst->opcode) {
            case OP_JUMP_IF_EQ_IMM:  taken = (k == m); known = true; break;
            case OP_JUMP_IF_NEQ_IMM: taken = (k != m); known = true; break;
            case OP_JUMP_IF_LT_IMM:  taken = (k <  m); known = true; break;
            case OP_JUMP_IF_GT_IMM:  taken = (k >  m); known = true; break;
            case OP_JUMP_IF_LTE_IMM: taken = (k <= m); known = true; break;
            case OP_JUMP_IF_GTE_IMM: taken = (k >= m); known = true; break;
            default: break;
        }
        if (known && taken) {
            inst->opcode = OP_JUMP;
            inst->operands[1] = 0;
            inst->operands[2] = 0;
            return -1;
        }
    }

    // LOAD_BOOL d,0 + JUMP_IF_FALSE d,tgt -> unconditional JUMP
    if (prev->opcode == OP_LOAD_BOOL && prev->operands[0] == inst->operands[1] &&
        prev->operands[1] == 0 && inst->opcode == OP_JUMP_IF_FALSE) {
        inst->opcode = OP_JUMP;
        inst->operands[1] = 0;
        inst->operands[2] = 0;
        return -1;
    }

    return -1;
}
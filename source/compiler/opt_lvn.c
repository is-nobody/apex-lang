// source/compiler/opt_lvn.c
// Local value numbering cache for arithmetic results
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"

// looks up a previously computed arithmetic result with identical inputs
int imm_lvn_lookup(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm) {
    for (int i = 0; i < cg->imm_lvn.count; i++) {                    // scan live entries
        if (cg->imm_lvn.entries[i].op == op &&
            cg->imm_lvn.entries[i].left_reg == left_reg &&
            cg->imm_lvn.entries[i].right_reg == right_reg &&
            cg->imm_lvn.entries[i].imm == imm) {
            return cg->imm_lvn.entries[i].result_reg;                // cache hit
        }
    }
    return -1;                                                       // not found
}

// records a freshly computed arithmetic result so later uses can reuse the register
void imm_lvn_add(CodeGenerator* cg, Opcode op, int left_reg, int right_reg, int imm, int result_reg) {
    if (result_reg >= 0 && (result_reg == left_reg || result_reg == right_reg))
        return;                                                      // in-place: entry stale the moment it is added
    if (cg->imm_lvn.count >= 16) return;                             // cap to bound pressure
    cg->imm_lvn.entries[cg->imm_lvn.count].op         = op;          // opcode
    cg->imm_lvn.entries[cg->imm_lvn.count].left_reg   = left_reg;    // left operand
    cg->imm_lvn.entries[cg->imm_lvn.count].right_reg  = right_reg;   // right operand (-1 for *_IMM)
    cg->imm_lvn.entries[cg->imm_lvn.count].imm        = imm;         // immediate (0 for register-register)
    cg->imm_lvn.entries[cg->imm_lvn.count].result_reg = result_reg;  // register with result
    cg->imm_lvn.count++;                                             // one more entry
}

// drops every cache entry whose operand or result has just been overwritten
void imm_lvn_invalidate(CodeGenerator* cg, int written_reg) {
    for (int i = 0; i < cg->imm_lvn.count; i++) {                    // compact in-place
        if (cg->imm_lvn.entries[i].left_reg == written_reg ||
            cg->imm_lvn.entries[i].right_reg == written_reg ||
            cg->imm_lvn.entries[i].result_reg == written_reg) {
            cg->imm_lvn.entries[i] = cg->imm_lvn.entries[--cg->imm_lvn.count];
            i--;                                                     // recheck swapped-in entry
        }
    }
}

// invalidate cache entries whose value the emitted instruction may have changed
void imm_lvn_on_emit(CodeGenerator* cg, Instruction* inst) {
    switch (inst->opcode) {
        case OP_CALL: case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
        case OP_CALL_BUILTIN:
        case OP_ASYNC_CALL: case OP_ASYNC_CALL_BUILTIN:
            cg->imm_lvn.count = 0;
            return;

        case OP_STORE_GLOBAL: {
            int g = inst->operands[1];
            for (int i = 0; i < cg->imm_lvn.count; i++) {
                if (cg->imm_lvn.entries[i].op == OP_LOAD_GLOBAL &&
                    cg->imm_lvn.entries[i].imm == g) {
                    cg->imm_lvn.entries[i] =
                        cg->imm_lvn.entries[--cg->imm_lvn.count];
                    i--;
                }
            }
            return;
        }

        case OP_TABLE_SET: case OP_TABLE_SET_CONST:
        case OP_TABLE_SET_INT: case OP_TABLE_SET_NUM:
        case OP_TABLE_SET_KEY_STR: case OP_TABLE_APPEND: {
            int t = inst->operands[0];
            for (int i = 0; i < cg->imm_lvn.count; i++) {
                Opcode op = cg->imm_lvn.entries[i].op;
                if ((op == OP_TABLE_GET_CONST || op == OP_TABLE_GET_INT) &&
                    cg->imm_lvn.entries[i].left_reg == t) {
                    cg->imm_lvn.entries[i] =
                        cg->imm_lvn.entries[--cg->imm_lvn.count];
                    i--;
                }
            }
            return;
        }

        default:
            break;
    }

    if (op_writes_dest_reg(inst->opcode)) {
        imm_lvn_invalidate(cg, inst->operands[0]);
    }
}

// true when an opcode writes its operands[0] as a destination register
bool op_writes_dest_reg(Opcode op) {
    switch (op) {
        case OP_MOVE: case OP_NEG: case OP_INC: case OP_DEC:
        case OP_LOAD_CONST: case OP_LOAD_NUM_IMM: case OP_LOAD_NUM:
        case OP_LOAD_BOOL: case OP_LOAD_NONE:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
        case OP_DIV_IMM: case OP_MOD_IMM:
        case OP_CMP_EQ: case OP_CMP_NEQ:
        case OP_CMP_EQ_NUM: case OP_CMP_NEQ_NUM:
        case OP_CMP_LT: case OP_CMP_GT: case OP_CMP_LTE: case OP_CMP_GTE:
        case OP_CALL: case OP_CALL_0: case OP_CALL_1: case OP_CALL_2:
        case OP_CALL_BUILTIN: case OP_ASYNC_CALL: case OP_ASYNC_CALL_BUILTIN:
        case OP_LOAD_GLOBAL: case OP_TABLE_GET: case OP_TABLE_GET_CONST:
        case OP_TABLE_GET_INT: case OP_NEW_TABLE: case OP_LOAD_TABLE:
        case OP_CONCAT:
        case OP_AND: case OP_OR: case OP_NOT:
        case OP_AWAIT:
            return true;                                             // these write operands[0]
        default:
            return false;                                            // jumps, returns, stores do not
    }
}
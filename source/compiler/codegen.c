// source/compiler/codegen.c
// Codegen entry point, chunk emission, top-level API
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen.h"
#include "codegen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// emits an instruction with source line info for debugging
int emit(CodeGenerator* cg, Instruction inst, int line) {
    int fused_idx = try_peephole_fuse(cg, &inst);
    if (fused_idx >= 0) {
        Instruction* fused = &cg->chunk->code[fused_idx];
        if (op_writes_dest_reg(fused->opcode)) {
            imm_lvn_invalidate(cg, fused->operands[0]);
            str_cache_invalidate(cg, fused->operands[0]);
        }
        return fused_idx;
    }

    bool is_jump = (inst.opcode == OP_JUMP ||
                    (inst.opcode >= OP_JUMP_IF_FALSE && inst.opcode <= OP_JUMP_IF_GTE) ||
                    (inst.opcode >= OP_JUMP_IF_EQ_IMM && inst.opcode <= OP_JUMP_IF_GTE_IMM) ||
                    inst.opcode == OP_JUMP_MATCH_NUM || inst.opcode == OP_JUMP_MATCH_STR ||
                    inst.opcode == OP_JUMP_MATCH_BOOL || inst.opcode == OP_JUMP_MATCH_NONE);

    // backward jumps are emitted with their final target already in operands[0]
    if (is_jump && inst.operands[0] != 0) {
        inst.operands[0] = resolve_jump_target(cg, inst.operands[0]);
        mark_jump_target(cg, inst.operands[0]);
    }

    if (is_jump) {
        cg->imm_lvn.count = 0;
    } else if (op_writes_dest_reg(inst.opcode)) {
        imm_lvn_invalidate(cg, inst.operands[0]);
        str_cache_invalidate(cg, inst.operands[0]);
    }
    return bytecode_emit_line(cg->chunk, inst, line);
}

// creates a new code generator
CodeGenerator* codegen_create(BytecodeChunk* chunk) {
    CodeGenerator* cg = (CodeGenerator*)calloc(1, sizeof(CodeGenerator));  // allocate and zero
    cg->chunk = chunk;                                                     // store bytecode chunk
    cg->next_register = 0;                                                 // start at 0
    cg->max_registers = 0;                                                 // no registers yet
    cg->iv_reduce.loop_var = NULL;                                         // no reduction active
    cg->iv_reduce.count = 0;                                               // no candidates
    cg->current_function = -1;                                             // no active function
    cg->label_counter = 0;                                                 // label counter
    cg->current_call_is_awaited = false;                                   // no awaited call in flight

    cg->loop_stack.break_capacity = 16;                                    // initial break capacity
    cg->loop_stack.break_jumps = (int*)malloc(sizeof(int) * cg->loop_stack.break_capacity);  // allocate breaks

    cg->current_module = NULL;                                             // no current module
    cg->imported_modules = NULL;                                           // no imports
    cg->module_count = 0;                                                  // zero imports
    cg->module_capacity = 0;                                               // no capacity
    cg->module_globals = NULL;                                             // no module globals
    cg->module_globals_count = 0;                                          // zero module globals
    cg->module_globals_capacity = 0;                                       // no capacity
    cg->fn_decls     = NULL;                                               // no per-function ASTs yet
    cg->fn_decls_cap = 0;                                                  // zero capacity
    cg->register_floor = 0;                                                // no floor at top level
    cg->cache_floor    = 0;                                                // no cache pins yet
    cg->for_scope_depth = 0;                                               // not inside any for
    cg->unswitch_depth = 0;                                                // no unswitch in flight
    cg->unroll_depth = 0;                                                  // no unroll in flight
    cg->jump_targets     = NULL;
    cg->jump_targets_cap = 0;
    cg->hoist.active   = false;
    cg->hoist.values   = NULL;
    cg->hoist.regs     = NULL;
    cg->hoist.count    = 0;
    cg->hoist.capacity = 0;
    cg->hoist.get_names    = NULL;
    cg->hoist.get_indices  = NULL;
    cg->hoist.get_regs     = NULL;
    cg->hoist.get_count    = 0;
    cg->hoist.get_capacity = 0;

    cg->num_cache.values   = NULL;
    cg->num_cache.regs     = NULL;
    cg->num_cache.count    = 0;
    cg->num_cache.capacity = 0;

    cg->str_cache.values   = NULL;
    cg->str_cache.regs     = NULL;
    cg->str_cache.count    = 0;
    cg->str_cache.capacity = 0;

    cg->locals.is_integer  = NULL;                                         // allocated lazily by add_local
    cg->locals.is_number   = NULL;
    cg->locals.const_known = NULL;
    cg->locals.const_value = NULL;
    cg->locals.materialized = NULL;

    return cg;                                                             // return generator
}

// frees all code generator resources
void codegen_destroy(CodeGenerator* cg) {
    if (!cg) return;                                                       // guard against null

    for (int i = 0; i < cg->locals.count; i++) {                           // free local names
        free(cg->locals.names[i]);
    }
    free(cg->locals.names);                                                // free names array
    free(cg->locals.registers);                                            // free registers array
    free(cg->locals.is_number);                                            // free numeric-ness flags array
    free(cg->locals.is_integer);                                           // free integer-ness flags array
    free(cg->locals.const_known);                                          // free const-known flags array
    free(cg->locals.const_value);                                          // free const-value array
    free(cg->locals.materialized);                                         // free materialized flags array

    free(cg->hoist.values);                                                // free hoist arrays (defensive)
    free(cg->hoist.regs);
    free(cg->hoist.get_names);
    free(cg->hoist.get_indices);
    free(cg->hoist.get_regs);

    free(cg->num_cache.values);                                            // free numeric constant cache
    free(cg->num_cache.regs);

    for (int i = 0; i < cg->str_cache.count; i++) {                        // free string constant cache
        free(cg->str_cache.values[i]);
    }
    free(cg->str_cache.values);
    free(cg->str_cache.regs);

    free(cg->loop_stack.break_jumps);                                      // free break jumps
    free(cg->jump_targets);

    for (int i = 0; i < cg->module_count; i++) {                           // free imported modules
        free(cg->imported_modules[i]);
    }
    free(cg->imported_modules);                                            // free modules array

    for (int i = 0; i < cg->module_globals_count; i++) {                   // free module globals
        free(cg->module_globals[i]);
    }
    free(cg->module_globals);                                              // free globals array

    free(cg->fn_decls);                                                    // ASTs are owned by the parser; only the table is ours

    free(cg);                                                              // free generator
}

// public entry point that generates bytecode from an ast
bool codegen_generate(CodeGenerator* cg, ASTNode* ast) {
    if (!cg || !ast) return false;                                           // validate

    bytecode_add_function(cg->chunk, "__entry-apex__", 0);                   // add entry function
    cg->current_function = 0;                                                // set current function

    if (ast->type == AST_PROGRAM || ast->type == AST_BLOCK) {                // program or block
        codegen_block(cg, ast);                                              // emit block
    } else {                                                                 // single statement
        codegen_statement(cg, ast);                                          // emit statement
        cg->next_register = locals_high_water(cg);                           // drop temps
    }

    int entry_max = cg->max_registers;                                       // computed entry max regs
    if (entry_max < REGISTER_INITIAL_SIZE) entry_max = REGISTER_INITIAL_SIZE; // enforce pool minimum
    cg->chunk->functions[0].max_registers = entry_max;                       // store padded entry max regs

    emit(cg, INST(OP_HALT, 0, 0, 0), 0);                                     // halt instruction
    compact_bytecode(cg);                                                    // drop no-ops, remap pcs
    return true;                                                             // success
}
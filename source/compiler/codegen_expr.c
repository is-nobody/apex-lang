// source/compiler/codegen_expr.c
// Expression codegen (dispatch, literals, calls, tables, string interp)
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// default entry: no destination hint, caller owns the fresh temp
int codegen_expression(CodeGenerator* cg, ASTNode* node) {
    return codegen_expression_into(cg, node, -1);                  // -1 means "fresh temp please"
}

// emits a number literal into dest (or fresh temp)
static int codegen_literal_number(CodeGenerator* cg, ASTNode* node, int dest) {
    double val = node->literal_number.number_value;                        // extract value
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    if (val == (int)val && val >= 0 && val <= 65535) {                     // fits in immediate
        emit(cg, INST(OP_LOAD_NUM_IMM, dest, (int)val, 0), node->line);    // load immediate
    } else {                                                               // large value
        int const_idx = bytecode_add_number_constant(cg->chunk, val);      // add to constant pool
        emit(cg, INST(OP_LOAD_NUM, dest, const_idx, 0), node->line);       // load from pool
    }
    return dest;                                                           // return destination
}

// emits a string literal into dest (or fresh temp), reusing a cached register when possible
static int codegen_literal_string(CodeGenerator* cg, ASTNode* node, int dest) {
    const char* value = node->literal_string.string_value;                 // literal text
    int const_idx = bytecode_add_string_constant(cg->chunk, value);        // pool index

    // lvn backstop: str_cache may have evicted this entry
    int lvn_cached = imm_lvn_lookup(cg, OP_LOAD_CONST, -1, -1, const_idx);
    if (lvn_cached >= 0) {
        if (dest < 0 || dest == lvn_cached) return lvn_cached;
        emit(cg, INST(OP_MOVE, dest, lvn_cached, 0), node->line);
        return dest;
    }

    int cached = str_cache_lookup(cg, value);                              // already materialised?
    if (cached >= 0) {
        imm_lvn_add(cg, OP_LOAD_CONST, -1, -1, const_idx, cached);
        if (dest < 0 || dest == cached) return cached;                     // reuse cached register
        emit(cg, INST(OP_MOVE, dest, cached, 0), node->line);              // copy into requested dest
        return dest;                                                       // return destination
    }

    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    emit(cg, INST(OP_LOAD_CONST, dest, const_idx, 0), node->line);         // load constant
    str_cache_add(cg, value, dest);                                        // cache the destination register
    imm_lvn_add(cg, OP_LOAD_CONST, -1, -1, const_idx, dest);               // LVN backstop
    return dest;                                                           // return destination
}

// emits a none literal into dest (or fresh temp)
static int codegen_literal_none(CodeGenerator* cg, ASTNode* node, int dest) {
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    emit(cg, INST(OP_LOAD_NONE, dest, 0, 0), node->line);                  // load none directly
    return dest;                                                           // return destination
}

// emits a bool literal into dest (or fresh temp)
static int codegen_literal_bool(CodeGenerator* cg, ASTNode* node, int dest) {
    if (dest < 0) dest = alloc_register(cg);                               // allocate if no hint
    emit(cg, INST(OP_LOAD_BOOL, dest,                                      // load bool
                  node->literal_bool.bool_value ? 1 : 0, 0), node->line);
    return dest;                                                           // return destination
}

// load a global into a register, reusing a cached lvn register when one already holds that global's value
static int load_global_cached(CodeGenerator* cg, int global_idx, int dest, int line) {
    int cached = imm_lvn_lookup(cg, OP_LOAD_GLOBAL, -1, -1, global_idx);
    if (cached >= 0) {
        if (dest < 0 || dest == cached) return cached;
        emit(cg, INST(OP_MOVE, dest, cached, 0), line);
        return dest;
    }
    int reg = dest >= 0 ? dest : alloc_register(cg);
    emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), line);
    imm_lvn_add(cg, OP_LOAD_GLOBAL, -1, -1, global_idx, reg);
    return reg;
}

// emits an identifier, preferring local variables over globals
static int codegen_identifier(CodeGenerator* cg, ASTNode* node, int dest) {
    const char* name = node->identifier.name;                              // get identifier name

    int local_reg = find_local(cg, name);                                  // check local scope
    if (local_reg >= 0) {                                                  // found locally
        if (dest < 0 || dest == local_reg) return local_reg;               // no copy needed
        emit(cg, INST(OP_MOVE, dest, local_reg, 0), node->line);           // copy into requested dest
        return dest;                                                       // return destination
    }

    for (int i = 0; i < cg->module_count; i++) {                           // check imported modules
        char full_name[512];                                               // qualified name buffer
        snprintf(full_name, sizeof(full_name), "%s.%s", cg->imported_modules[i], name);

        int global_idx = bytecode_get_global(cg->chunk, full_name);        // lookup global
        if (global_idx >= 0) {                                             // found in module
            return load_global_cached(cg, global_idx, dest, node->line);
        }
    }

    if (cg->current_module && !strchr(name, '.')) {                        // inside module and bare name
        char qualified[512];                                               // buffer for qualified name
        snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, name);
        int global_idx = bytecode_get_global(cg->chunk, qualified);        // lookup qualified global
        if (global_idx >= 0) {                                             // found qualified global
            return load_global_cached(cg, global_idx, dest, node->line);
        }
    }

    int global_idx = bytecode_get_global(cg->chunk, name);                 // lookup in global scope
    if (global_idx >= 0) {                                                 // found globally
        if (cg->current_function == 0) {                                   // top-level scope
            int reg = add_local(cg, name);                                 // add as local cache
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);  // load global into local
            if (dest < 0 || dest == reg) return reg;                       // local is the dest
            emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);             // copy into requested dest
            return dest;                                                   // return destination
        }
        return load_global_cached(cg, global_idx, dest, node->line);
    }

    if (cg->current_module && !strchr(name, '.')) {                        // inside module and bare name
        char qualified[512];                                               // buffer for qualified name
        snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, name);
        global_idx = bytecode_add_global(cg->chunk, qualified);            // create qualified global slot
        if (cg->current_function == 0) {                                   // top-level scope
            int reg = add_local(cg, name);                                 // add as local cache
            emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
            if (dest < 0 || dest == reg) return reg;                       // local is the dest
            emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);             // copy into requested dest
            return dest;                                                   // return destination
        }
        return load_global_cached(cg, global_idx, dest, node->line);
    }

    // fallback: create bare global (for non-module code)
    global_idx = bytecode_add_global(cg->chunk, name);                     // add new global
    if (cg->current_function == 0) {                                       // top-level scope
        int reg = add_local(cg, name);                                     // add as local cache
        emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);
        if (dest < 0 || dest == reg) return reg;                           // local is the dest
        emit(cg, INST(OP_MOVE, dest, reg, 0), node->line);                 // copy into requested dest
        return dest;                                                       // return destination
    }
    return load_global_cached(cg, global_idx, dest, node->line);
}

// emits a function call, resolving builtins and user functions by name
static int codegen_call(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    ASTNodeList* args_list = node->call.arguments;                      // arguments list
    int arg_count = args_list->count;                                   // number of arguments
    int* arg_regs = NULL;                                               // argument registers

    // result register must be allocated BEFORE args so args land above it
    int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);   // result destination

    if (arg_count > 0) {                                                // has arguments
        arg_regs = (int*)malloc(sizeof(int) * arg_count);               // allocate arg array
        for (int i = 0; i < arg_count; i++) {                           // evaluate each arg
            arg_regs[i] = codegen_expression(cg, args_list->nodes[i]);  // store register
        }
    }

    char func_name[256] = "";                                           // function name buffer
    ASTNode* callee = node->call.callee;                                // callee expression
    if (callee->type == AST_IDENTIFIER) {                               // simple identifier
        strcpy(func_name, callee->identifier.name);                     // copy name
    } else if (callee->type == AST_INDEX_ACCESS) {                      // dotted access
        ASTNode* parts[32];                                             // path parts
        int part_count = 0;                                             // number of parts
        ASTNode* current = callee;                                      // current node

        while (current->type == AST_INDEX_ACCESS) {                     // traverse access chain
            if (current->access.member->type == AST_IDENTIFIER) {       // valid member
                parts[part_count++] = current->access.member;           // add part
            } else {
                part_count = 0;                                         // invalid
                break;                                                  // exit loop
            }
            current = current->access.object;                           // move to object
        }

        if (part_count > 0 && current->type == AST_IDENTIFIER) {        // valid path
            parts[part_count++] = current;                              // add last part

            func_name[0] = '\0';                                        // clear name
            for (int i = part_count - 1; i >= 0; i--) {                 // build from right
                if (i < part_count - 1) {                               // not first
                    strcat(func_name, ".");                             // add dot separator
                }
                strcat(func_name, parts[i]->identifier.name);           // add part name
            }
        }
    }

    bool is_builtin = false;  // check if this is a known builtin

    if (strcmp(func_name, "number") == 0 ||
        strcmp(func_name, "string") == 0 ||
        strcmp(func_name, "type") == 0) {
        is_builtin = true;
    }
    else if (strncmp(func_name, "os.", 3) == 0 ||
             strncmp(func_name, "sys.", 4) == 0 ||
             strncmp(func_name, "math.", 5) == 0 ||
             strncmp(func_name, "string.", 7) == 0 ||
             strncmp(func_name, "table.", 6) == 0 ||
             strncmp(func_name, "random.", 7) == 0 ||
             strncmp(func_name, "json.", 5) == 0 ||
             strncmp(func_name, "xml.", 4) == 0 ||
             strncmp(func_name, "csv.", 4) == 0 ||
             strncmp(func_name, "base.", 5) == 0 ||
             strncmp(func_name, "regex.", 6) == 0 ||
             strncmp(func_name, "crypto.", 7) == 0 ||
             strncmp(func_name, "zip.", 4) == 0) {
        is_builtin = true;
    }

    if (is_builtin) {                                                         // built-in function
        for (int i = 0; i < arg_count; i++) {                                 // push args
            emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
        }
        int name_idx = bytecode_add_string_constant(cg->chunk, func_name);    // add name constant
        int op = cg->current_call_is_awaited ? OP_ASYNC_CALL_BUILTIN : OP_CALL_BUILTIN;
        emit(cg, INST(op, result_reg, name_idx, arg_count), node->line);      // call builtin
    } else {                                                                           // user function
        int func_idx = -1;                                                             // function index

        for (int i = 0; i < cg->chunk->func_count; i++) {                              // search function table
            if (strcmp(cg->chunk->functions[i].name, func_name) == 0) {                // exact name match
                func_idx = i;                                                          // store function index
                break;                                                                 // exit loop
            }
        }

        if (func_idx < 0 && func_name[0] != '\0' && !strchr(func_name, '.') && cg->current_module) {
            char qualified[512];                                                             // buffer for qualified name
            snprintf(qualified, sizeof(qualified), "%s.%s", cg->current_module, func_name);  // build qualified name
            for (int i = 0; i < cg->chunk->func_count; i++) {                                // search function table
                if (strcmp(cg->chunk->functions[i].name, qualified) == 0) {                  // qualified name match
                    func_idx = i;                                                            // store function index
                    break;                                                                   // exit loop
                }
            }
        }

        if (func_idx < 0 && func_name[0] != '\0') {                                    // still not found
            const char* last_dot = strrchr(func_name, '.');                            // find last dot
            if (last_dot) {                                                            // dotted name
                const char* short_name = last_dot + 1;                                 // extract short name
                for (int i = 0; i < cg->chunk->func_count; i++) {                      // search function table
                    if (strcmp(cg->chunk->functions[i].name, short_name) == 0) {       // short name match
                        func_idx = i;                                                  // store function index
                        break;                                                         // exit loop
                    }
                }
            }
        }

        if (func_idx >= 0 && cg->chunk->functions[func_idx].is_async) {   // async call: defer body
            for (int i = 0; i < arg_count; i++) {                         // push captured args
                emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
            }
            emit(cg, INST(OP_ASYNC_CALL, result_reg, func_idx, arg_count), node->line);  // return pending future
            if (arg_regs) {                                               // free arg registers
                for (int i = 0; i < arg_count; i++) free_register(cg, arg_regs[i]);
                free(arg_regs);                                           // free arg array
            }
            return result_reg;                                            // caller gets a future
        }

        // ipa: inline small pure single-return functions at the call site
        if (func_idx >= 0 && func_idx < cg->fn_decls_cap &&                   // AST available?
            cg->fn_decls[func_idx] &&                                         // non-null decl
            function_is_inlinable(cg->fn_decls[func_idx])) {                  // pure `return <expr>`
            if (try_inline_function(cg, func_idx, args_list, arg_regs, arg_count,
                                    result_reg, node->line)) {                // body codegen into dest
                if (arg_regs) {                                               // release arg temporaries
                    for (int i = 0; i < arg_count; i++) free_register(cg, arg_regs[i]);
                    free(arg_regs);
                }
                return result_reg;                                            // result is already in dest
            }
        }

        if (func_idx >= 0) {                                                   // function found
            if (arg_count == 0) {                                              // zero args
                emit(cg, INST(OP_CALL_0, result_reg, func_idx, 0), node->line);
            } else if (arg_count == 1) {                                       // one arg
                emit(cg, INST(OP_CALL_1, result_reg, func_idx, arg_regs[0]), node->line);
            } else if (arg_count == 2) {                                       // two args
                if (arg_regs[1] != arg_regs[0] + 1) {                          // not already contiguous
                    int t0 = alloc_register(cg);                               // fresh slot for arg0
                    int t1 = alloc_register(cg);                               // fresh slot for arg1
                    emit(cg, INST(OP_MOVE, t0, arg_regs[0], 0), node->line);   // copy arg0
                    emit(cg, INST(OP_MOVE, t1, arg_regs[1], 0), node->line);   // copy arg1
                    free_register(cg, arg_regs[1]);                            // free old arg1
                    free_register(cg, arg_regs[0]);                            // free old arg0
                    arg_regs[0] = t0;                                          // point to fresh slot
                    arg_regs[1] = t1;                                          // point to fresh slot
                }
                emit(cg, INST(OP_CALL_2, result_reg, func_idx, arg_regs[0]), node->line);
            } else {                                                           // many args
                for (int i = 0; i < arg_count; i++) {                          // push all args
                    emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
                }
                emit(cg, INST(OP_CALL, result_reg, func_idx, arg_count), node->line);
            }
        } else {                                                              // function not found
            for (int i = 0; i < arg_count; i++) {                             // push args
                emit(cg, INST(OP_PUSH_ARG, arg_regs[i], 0, 0), node->line);
            }
            int name_idx = bytecode_add_string_constant(cg->chunk, func_name);             // add name constant
            emit(cg, INST(OP_CALL_BUILTIN, result_reg, name_idx, arg_count), node->line);  // fallback to builtin
        }
    }

    if (arg_regs) {                                                           // free arg registers
        for (int i = 0; i < arg_count; i++) {
            free_register(cg, arg_regs[i]);
        }
        free(arg_regs);                                                       // free arg array
    }
    return result_reg;                                                        // return result register
}

// emits string interpolation by concatenating parts, first part goes into dest
int codegen_string_interp(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (node->string_interp.parts->count == 0) {                           // empty interpolation
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);         // use hint or fresh
        int empty_idx = bytecode_add_string_constant(cg->chunk, "");       // empty string constant
        emit(cg, INST(OP_LOAD_CONST, reg, empty_idx, 0), node->line);      // load empty
        return reg;                                                        // return register
    }

    // single part: no concatenation to accumulate into, forward the value as-is
    if (node->string_interp.parts->count == 1) {
        return codegen_expression_into(cg, node->string_interp.parts->nodes[0], dest_hint);
    }

    int result_reg;                                                        // result register
    if (dest_hint >= 0) {                                                  // write first part into dest
        codegen_expression_into(cg, node->string_interp.parts->nodes[0], dest_hint);
        result_reg = dest_hint;                                            // result lives in dest
    } else {                                                               // no hint: fresh temp
        result_reg = alloc_register(cg);                                   // allocate a fresh destination
        codegen_expression_into(cg, node->string_interp.parts->nodes[0], result_reg);  // force first part into it
    }

    for (int i = 1; i < node->string_interp.parts->count; i++) {           // remaining parts
        ASTNode* part = node->string_interp.parts->nodes[i];               // current part
        int part_reg = codegen_expression(cg, part);                       // fresh temp for part
        emit(cg, INST(OP_CONCAT, result_reg, result_reg, part_reg),        // in-place concatenation
             node->line);
        free_register(cg, part_reg);                                       // free part temp
    }
    return result_reg;                                                     // return final result
}

// checks whether `node` is a two-part interpolation "literal{expr}";
// returns the constant pool index of the literal prefix, or -1.
// does not emit any code and does not allocate registers.
int key_str_prefix_idx(CodeGenerator* cg, ASTNode* node) {
    if (!node || node->type != AST_STRING_INTERP) return -1;                 // not an interpolation
    if (node->string_interp.parts->count != 2) return -1;                    // need exactly two parts
    if (node->string_interp.parts->nodes[0]->type != AST_LITERAL_STRING)
        return -1;                                                           // left must be a literal
    const char* prefix = node->string_interp.parts->nodes[0]
                             ->literal_string.string_value;                  // prefix text
    int idx = bytecode_add_string_constant(cg->chunk, prefix);               // pool index
    if (idx < 0 || idx > 0xFFFF) return -1;                                  // must fit packed op2
    return idx;                                                              // ready to fuse
}

// main expression dispatcher with dest_hint contract
int codegen_expression_into(CodeGenerator* cg, ASTNode* node, int dest_hint) {
    if (!node) {                                                                     // null node
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_NONE, reg, 0, 0), 0);                                  // load none
        return reg;                                                                  // return register
    }

    // licm: substitute a loop-invariant expression with its pre-loaded register
    if (cg->hoist.active && cg->hoist.expr_count > 0 &&
        (node->type == AST_BINARY || node->type == AST_UNARY || node->type == AST_INDEX_ACCESS)) {
        for (int i = 0; i < cg->hoist.expr_count; i++) {
            if (expr_struct_eq(node, cg->hoist.exprs[i])) {
                int r = cg->hoist.expr_regs[i];
                if (dest_hint < 0 || dest_hint == r) return r;
                emit(cg, INST(OP_MOVE, dest_hint, r, 0), node->line);
                return dest_hint;
            }
        }
    }

    // iv strength reduction: substitute `loop_var OP c` with its accumulator
    if (cg->iv_reduce.loop_var && node->type == AST_BINARY &&
        node->binary.left && node->binary.right) {
        ApexTokenType bop = node->binary.op;
        Opcode red = (bop == TOKEN_STAR) ? OP_MUL :
                     (bop == TOKEN_PLUS) ? OP_ADD : OP_MOVE;
        if (red != OP_MOVE) {
            ASTNode* cn = NULL;                                                      // constant side
            if (node->binary.left->type == AST_IDENTIFIER &&
                strcmp(node->binary.left->identifier.name, cg->iv_reduce.loop_var) == 0) {
                cn = node->binary.right;
            } else if (node->binary.right->type == AST_IDENTIFIER &&
                       strcmp(node->binary.right->identifier.name, cg->iv_reduce.loop_var) == 0) {
                cn = node->binary.left;
            }
            if (cn) {
                double cv;
                if (try_fold_number(cg, cn, &cv)) {
                    for (int k = 0; k < cg->iv_reduce.count; k++) {
                        if (cg->iv_reduce.entries[k].op == red &&
                            cg->iv_reduce.entries[k].value == cv) {
                            int reg = cg->iv_reduce.entries[k].reg;
                            if (dest_hint < 0 || dest_hint == reg) return reg;
                            emit(cg, INST(OP_MOVE, dest_hint, reg, 0), node->line);
                            return dest_hint;
                        }
                    }
                }
            }
        }
    }

    double nv;                                                                       // folded numeric value
    if (try_fold_number(cg, node, &nv)) {                                            // constant subtree?
        if (cg->hoist.active) {                                                      // inside a loop with hoisted constants
            for (int i = 0; i < cg->hoist.count; i++) {                              // look up folded value
                if (cg->hoist.values[i] == nv) {                                     // matches a hoisted constant
                    int hreg = cg->hoist.regs[i];                                    // hoisted register
                    if (dest_hint < 0 || dest_hint == hreg) return hreg;             // reuse directly
                    emit(cg, INST(OP_MOVE, dest_hint, hreg, 0), node->line);         // copy into hint
                    return dest_hint;
                }
            }
        }
        // reuse the local's live register when the fold came from an identifier
        if (node->type == AST_IDENTIFIER) {
            int slot = find_local_slot(cg, node->identifier.name);
            if (slot >= 0 && cg->locals.const_known[slot] && cg->locals.materialized[slot]) {
                int lreg = cg->locals.registers[slot];
                if (dest_hint < 0 || dest_hint == lreg) return lreg;                 // reuse directly
                emit(cg, INST(OP_MOVE, dest_hint, lreg, 0), node->line);             // copy into hint
                return dest_hint;
            }
        }
        int cached = num_cache_lookup(cg, nv);                                       // check per-function cache
        if (cached >= 0) {                                                           // already materialized earlier
            if (dest_hint < 0 || dest_hint == cached) return cached;                 // reuse cached register
            emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);               // copy into hint
            return dest_hint;
        }
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        if (nv == (int)nv && nv >= 0 && nv <= 65535) {                               // fits in immediate
            emit(cg, INST(OP_LOAD_NUM_IMM, reg, (int)nv, 0), node->line);            // load immediate
        } else {                                                                     // large or fractional
            int const_idx = bytecode_add_number_constant(cg->chunk, nv);             // add to constant pool
            emit(cg, INST(OP_LOAD_NUM, reg, const_idx, 0), node->line);              // load from pool
        }
        if (dest_hint < 0) num_cache_add(cg, nv, reg);                               // cache self-allocated reg
        return reg;                                                                  // single load replaces subtree
    }

    bool bv;                                                                         // folded boolean value
    if (try_fold_bool(cg, node, &bv)) {                                              // constant boolean?
        int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                   // use hint or fresh
        emit(cg, INST(OP_LOAD_BOOL, reg, bv ? 1 : 0, 0), node->line);                // load bool directly
        return reg;                                                                  // single load replaces subtree
    }

    switch (node->type) {                                                            // dispatch by type
        case AST_ASSIGN:                                                             // assignment
            return codegen_assign_expr(cg, node, dest_hint);

        case AST_LITERAL_NUMBER:                                                     // number literal
            return codegen_literal_number(cg, node, dest_hint);

        case AST_LITERAL_STRING:                                                     // string literal
            return codegen_literal_string(cg, node, dest_hint);

        case AST_LITERAL_NONE:                                                       // none literal
            return codegen_literal_none(cg, node, dest_hint);

        case AST_LITERAL_BOOL:                                                       // boolean literal
            return codegen_literal_bool(cg, node, dest_hint);

        case AST_IDENTIFIER:                                                         // identifier
            return codegen_identifier(cg, node, dest_hint);

        case AST_BINARY: {                                                           // binary operation
            if (node->binary.op == TOKEN_AND || node->binary.op == TOKEN_OR) {       // logical and/or
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);    // result dest

                int left_reg = codegen_expression_into(cg, node->binary.left, result_reg);  // evaluate left into dest
                if (left_reg != result_reg) {                                        // left ignored hint
                    emit(cg, INST(OP_MOVE, result_reg, left_reg, -1), node->line);
                    free_register(cg, left_reg);                                     // free left temp
                }

                int jump_idx;                                                        // jump to skip right
                if (node->binary.op == TOKEN_AND) {                                  // AND
                    jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, result_reg, -1), node->line);  // if false skip
                } else {                                                             // OR
                    int not_reg = alloc_register(cg);                                // inverted left register
                    emit(cg, INST(OP_NOT, not_reg, result_reg, 0), node->line);      // not left
                    jump_idx = emit(cg, INST(OP_JUMP_IF_FALSE, 0, not_reg, -1), node->line);  // if truthy skip
                    free_register(cg, not_reg);                                      // free inverted
                }

                int right_reg = codegen_expression_into(cg, node->binary.right, result_reg);  // evaluate right into dest
                if (right_reg != result_reg) {                                       // right ignored hint
                    emit(cg, INST(OP_MOVE, result_reg, right_reg, -1), node->line);
                    free_register(cg, right_reg);                                    // free right temp
                }

                PATCH_JUMP(cg, jump_idx, bytecode_current_offset(cg->chunk));                // patch skip

                return result_reg;                                                   // return result
            }

            // algebraic identities (run AFTER constant folding)
            {
                ApexTokenType bop = node->binary.op;
                double lv, rv;
                bool lconst = try_fold_number(cg, node->binary.left,  &lv);
                bool rconst = try_fold_number(cg, node->binary.right, &rv);

                switch (bop) {
                    // x + 0 = x, 0 + x = x  (holds for ALL values incl. none)
                    case TOKEN_PLUS:
                        if (rconst && rv == 0.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        if (lconst && lv == 0.0)
                            return codegen_expression_into(cg, node->binary.right, dest_hint);
                        break;

                    // x - 0 = x  (holds for ALL values incl. none)
                    case TOKEN_MINUS:
                        if (rconst && rv == 0.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        // 0 - x = -x  (safe only when x is a number)
                        if (lconst && lv == 0.0 &&
                            is_number_expression(cg, node->binary.right)) {
                            int op_reg = codegen_expression(cg, node->binary.right);
                            int res    = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_NEG, res, op_reg, 0), node->line);
                            free_register(cg, op_reg);
                            return res;
                        }
                        // x - x = 0  (safe only when x is a number)
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    // x * 1 = x, 1 * x = x  (holds for ALL values incl. none)
                    case TOKEN_STAR:
                        if (rconst && rv == 1.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        if (lconst && lv == 1.0)
                            return codegen_expression_into(cg, node->binary.right, dest_hint);
                        // x * 0 = 0, 0 * x = 0  (safe only when x is a number)
                        if (rconst && rv == 0.0 &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        if (lconst && lv == 0.0 &&
                            is_number_expression(cg, node->binary.right)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_NUM_IMM, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    // x / 1 = x  (holds for ALL values incl. none)
                    case TOKEN_SLASH:
                        if (rconst && rv == 1.0)
                            return codegen_expression_into(cg, node->binary.left,  dest_hint);
                        break;

                    // x == x = true, x != x = false, x <= x = true, x >= x = true,
                    // x < x = false, x > x = false   (safe only when x is a number)
                    case TOKEN_EQUAL_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 1, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_NOT_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 0, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_LESS_EQUAL:
                    case TOKEN_GREATER_EQUAL:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 1, 0), node->line);
                            return res;
                        }
                        break;
                    case TOKEN_LESS:
                    case TOKEN_GREATER:
                        if (ast_same_expr(node->binary.left, node->binary.right) &&
                            is_number_expression(cg, node->binary.left)) {
                            int res = dest_hint >= 0 ? dest_hint : alloc_register(cg);
                            emit(cg, INST(OP_LOAD_BOOL, res, 0, 0), node->line);
                            return res;
                        }
                        break;

                    default: break;
                }
            }

            // strength reduction: x / 2^k  ->  x * (1/2^k)
            {
                double divisor;
                if (node->binary.op == TOKEN_SLASH &&
                    try_fold_number(cg, node->binary.right, &divisor)) {
                    long long iv = (long long)divisor;
                    if (divisor == (double)iv &&
                        iv >= 2 && iv <= (1LL << 52) &&
                        (iv & (iv - 1)) == 0) {
                        double recip = 1.0 / divisor;
                        int left_reg = codegen_expression(cg, node->binary.left);
                        int recip_reg = num_cache_lookup(cg, recip);
                        if (recip_reg < 0) {
                            recip_reg = alloc_register(cg);
                            int const_idx =
                                bytecode_add_number_constant(cg->chunk, recip);
                            emit(cg, INST(OP_LOAD_NUM, recip_reg, const_idx, 0),
                                 node->line);
                            num_cache_add(cg, recip, recip_reg);
                        }
                        int result_reg = dest_hint >= 0 ? dest_hint
                                                        : alloc_register(cg);
                        emit(cg, INST(OP_MUL, result_reg, left_reg, recip_reg),
                             node->line);
                        free_register(cg, left_reg);
                        return result_reg;
                    }
                }
            }

            // try IMM-optimized arithmetic when the right operand folds to a numeric constant
            // that fits the immediate field (integer 0..65535); the fold covers literals,
            // negative literals via unary minus, and nested constant arithmetic like (1 + 1)
            Opcode imm_op = OP_MOVE;                                             // sentinel; overwritten on match
            bool has_imm = false;                                                // true when op maps to an IMM variant
            switch (node->binary.op) {                                           // map arithmetic op to IMM variant
                case TOKEN_PLUS:    imm_op = OP_ADD_IMM; has_imm = true; break;
                case TOKEN_MINUS:   imm_op = OP_SUB_IMM; has_imm = true; break;
                case TOKEN_STAR:    imm_op = OP_MUL_IMM; has_imm = true; break;
                case TOKEN_SLASH:   imm_op = OP_DIV_IMM; has_imm = true; break;
                case TOKEN_PERCENT: imm_op = OP_MOD_IMM; has_imm = true; break;
                default: break;                                              // not an arithmetic op
            }
            double imm_val;                                                      // folded immediate value
            if (has_imm && try_fold_number(cg, node->binary.right, &imm_val) &&
                imm_val == (int)imm_val && imm_val >= 0 && imm_val <= 65535) {
                int left_reg = codegen_expression(cg, node->binary.left);        // evaluate left
                int cached = imm_lvn_lookup(cg, imm_op, left_reg, -1, (int)imm_val); // reuse if identical
                if (cached >= 0) {
                    free_register(cg, left_reg);                                 // release left temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;     // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);   // copy into hint
                    return dest_hint;
                }
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result destination
                emit(cg, INST(imm_op, result_reg, left_reg, (int)imm_val), node->line);  // fused op
                imm_lvn_add(cg, imm_op, left_reg, -1, (int)imm_val, result_reg);   // cache every result
                free_register(cg, left_reg);                                     // free left
                return result_reg;                                               // return result
            }
            // commutative IMM: when the left operand is a small non-negative constant and the
            // operator is + or *, swap the operands so the constant lands in the immediate
            // field; x + 1 / 1 + x and x * 2 / 2 * x generate identical bytecode
            if (has_imm && (node->binary.op == TOKEN_PLUS || node->binary.op == TOKEN_STAR) &&
                try_fold_number(cg, node->binary.left, &imm_val) &&
                imm_val == (int)imm_val && imm_val >= 0 && imm_val <= 65535) {
                int right_reg = codegen_expression(cg, node->binary.right);      // evaluate right
                int cached = imm_lvn_lookup(cg, imm_op, right_reg, -1, (int)imm_val);  // reuse if identical
                if (cached >= 0) {
                    free_register(cg, right_reg);                                // release right temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;     // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);   // copy into hint
                    return dest_hint;
                }
                int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);  // result destination
                emit(cg, INST(imm_op, result_reg, right_reg, (int)imm_val), node->line);  // fused op
                imm_lvn_add(cg, imm_op, right_reg, -1, (int)imm_val, result_reg);   // cache every result
                free_register(cg, right_reg);                                    // free right
                return result_reg;                                               // return result
            }

            int left_reg = codegen_expression(cg, node->binary.left);               // evaluate left
            int right_reg = codegen_expression(cg, node->binary.right);             // evaluate right

            bool both_numbers = is_number_expression(cg, node->binary.left) &&      // check both operands known numeric
                                is_number_expression(cg, node->binary.right);

            Opcode op;                                                              // opcode
            switch (node->binary.op) {                                              // map operator
                case TOKEN_PLUS:           op = OP_ADD; break;
                case TOKEN_MINUS:          op = OP_SUB; break;
                case TOKEN_STAR:           op = OP_MUL; break;
                case TOKEN_SLASH:          op = OP_DIV; break;
                case TOKEN_PERCENT:        op = OP_MOD; break;
                case TOKEN_EQUAL_EQUAL:    op = both_numbers ? OP_CMP_EQ_NUM : OP_CMP_EQ; break;    // specialized if both numbers
                case TOKEN_NOT_EQUAL:      op = both_numbers ? OP_CMP_NEQ_NUM : OP_CMP_NEQ; break;  // specialized if both numbers
                case TOKEN_LESS:           op = OP_CMP_LT; break;
                case TOKEN_GREATER:        op = OP_CMP_GT; break;
                case TOKEN_LESS_EQUAL:     op = OP_CMP_LTE; break;
                case TOKEN_GREATER_EQUAL:  op = OP_CMP_GTE; break;
                default:                                                             // unknown
                    free_register(cg, left_reg);                                     // free left
                    free_register(cg, right_reg);                                    // free right
                    return dest_hint >= 0 ? dest_hint : alloc_register(cg);          // return uninitialized
            }

            // LVN for register-register arithmetic only; comparisons still need
            // their own register per result because the flag bit is not preserved
            bool lvn_ok = (op == OP_ADD || op == OP_SUB || op == OP_MUL ||
                           op == OP_DIV || op == OP_MOD);
            if (lvn_ok) {
                int cached = imm_lvn_lookup(cg, op, left_reg, right_reg, 0);        // reuse if identical
                if (cached >= 0) {
                    free_register(cg, left_reg);                                     // release left temp
                    free_register(cg, right_reg);                                    // release right temp
                    if (dest_hint < 0 || dest_hint == cached) return cached;         // return cached reg
                    emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);       // copy into hint
                    return dest_hint;
                }
            }

            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);       // result destination
            emit(cg, INST(op, result_reg, left_reg, right_reg), node->line);         // emit operation
            if (lvn_ok) {
                imm_lvn_add(cg, op, left_reg, right_reg, 0, result_reg);             // cache every result
            }
            free_register(cg, left_reg);                                             // free left
            free_register(cg, right_reg);                                            // free right
            return result_reg;                                                       // return result
        }
        case AST_UNARY: {                                                            // unary operation
            int operand_reg = codegen_expression(cg, node->unary.operand);           // evaluate operand
            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);        // result destination
            switch (node->unary.op) {                                                // map operator
                case TOKEN_MINUS:                                                    // negation
                    emit(cg, INST(OP_NEG, result_reg, operand_reg, 0), node->line);
                    break;
                case TOKEN_NOT:                                                      // logical not
                    emit(cg, INST(OP_NOT, result_reg, operand_reg, 0), node->line);
                    break;
                default: break;                                                      // unknown
            }
            free_register(cg, operand_reg);                                          // free operand
            return result_reg;                                                       // return result
        }

        case AST_CALL:                                                               // function call
            return codegen_call(cg, node, dest_hint);

        case AST_INDEX_ACCESS: {                                                     // table index access
            // licm: if the enclosing loop hoisted this exact access, return the pre-loaded register
            if (cg->hoist.get_count > 0 &&
                node->access.object->type == AST_IDENTIFIER &&
                node->access.member->type == AST_LITERAL_NUMBER) {
                const char* nm = node->access.object->identifier.name;
                double idx = node->access.member->literal_number.number_value;
                for (int i = 0; i < cg->hoist.get_count; i++) {
                    if (strcmp(cg->hoist.get_names[i], nm) == 0 &&
                        cg->hoist.get_indices[i] == idx) {
                        int r = cg->hoist.get_regs[i];
                        if (dest_hint < 0 || dest_hint == r) return r;
                        emit(cg, INST(OP_MOVE, dest_hint, r, 0), node->line);
                        return dest_hint;
                    }
                }
            }

            bool is_module_access = false;                                           // module flag
            char full_name[512] = "";                                                // qualified name

            if (node->access.object->type == AST_IDENTIFIER) {                       // object is identifier
                const char* obj_name = node->access.object->identifier.name;         // object name
                const char* member_name = NULL;                                      // member name

                if (node->access.member->type == AST_IDENTIFIER) {                   // member identifier
                    member_name = node->access.member->identifier.name;              // get name
                }
                else if (node->access.member->type == AST_LITERAL_STRING) {          // member string
                    member_name = node->access.member->literal_string.string_value;  // get string
                }

                if (member_name) {                                                   // valid member
                    for (int i = 0; i < cg->module_count; i++) {                     // check imported modules
                        if (strcmp(cg->imported_modules[i], obj_name) == 0) {        // matches import
                            is_module_access = true;                                 // mark as module
                            break;                                                   // exit loop
                        }
                    }
                    if (!is_module_access && is_known_builtin_module(obj_name)) {    // builtin module
                        is_module_access = true;                                     // mark as module
                    }

                    if (is_module_access) {                                                      // module access
                        snprintf(full_name, sizeof(full_name), "%s.%s", obj_name, member_name);  // build name
                    }
                }
            }

            if (is_module_access) {                                                              // module access
                int global_idx = bytecode_get_global(cg->chunk, full_name);                      // lookup global
                if (global_idx < 0) {                                                            // not found
                    global_idx = bytecode_add_global(cg->chunk, full_name);                      // add global
                }
                int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                       // use hint or fresh
                emit(cg, INST(OP_LOAD_GLOBAL, reg, global_idx, 0), node->line);                  // load global
                return reg;                                                                      // return register
            }

            int obj_reg = codegen_expression(cg, node->access.object);                           // evaluate object
            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);                    // result dest

            if (node->access.member->type == AST_LITERAL_NUMBER) {                               // member is number literal
                double key_val = node->access.member->literal_number.number_value;               // get key value

                if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {               // fits in immediate
                    int cached = imm_lvn_lookup(cg, OP_TABLE_GET_INT, obj_reg, -1, (int)key_val);
                    if (cached >= 0) {
                        free_register(cg, obj_reg);
                        if (dest_hint < 0 || dest_hint == cached) return cached;
                        emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);
                        return dest_hint;
                    }
                    emit(cg, INST(OP_TABLE_GET_INT, result_reg, obj_reg, (int)key_val), node->line);  // direct array access
                    imm_lvn_add(cg, OP_TABLE_GET_INT, obj_reg, -1, (int)key_val, result_reg);
                    free_register(cg, obj_reg);                                                  // free object
                    return result_reg;                                                           // return result
                }
            }

            if (node->access.member->type == AST_IDENTIFIER) {                                   // member identifier
                int local_reg = find_local(cg, node->access.member->identifier.name);            // check local
                if (local_reg >= 0) {                                                            // local variable
                    Opcode get_op = is_integer_expression(cg, node->access.member)               // whole-number key?
                                    ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                    emit(cg, INST(get_op, result_reg, obj_reg, local_reg), node->line);          // get by local
                } else {                                                                         // not local
                    int global_idx = bytecode_get_global(cg->chunk, node->access.member->identifier.name);  // check global
                    if (global_idx >= 0) {                                                       // global exists
                        int key_reg = alloc_register(cg);                                        // allocate key reg
                        emit(cg, INST(OP_LOAD_GLOBAL, key_reg, global_idx, 0), node->line);      // load global
                        emit(cg, INST(OP_TABLE_GET, result_reg, obj_reg, key_reg), node->line);  // get by global
                        free_register(cg, key_reg);                                              // free key
                    } else {                                                                     // constant string
                        int key_idx = bytecode_add_string_constant(cg->chunk,                    // add string constant
                            node->access.member->identifier.name);
                        int cached = imm_lvn_lookup(cg, OP_TABLE_GET_CONST, obj_reg, -1, key_idx);
                        if (cached >= 0) {
                            free_register(cg, obj_reg);
                            if (dest_hint < 0 || dest_hint == cached) return cached;
                            emit(cg, INST(OP_MOVE, dest_hint, cached, 0), node->line);
                            return dest_hint;
                        }
                        emit(cg, INST(OP_TABLE_GET_CONST, result_reg, obj_reg, key_idx), node->line); // get by const
                        imm_lvn_add(cg, OP_TABLE_GET_CONST, obj_reg, -1, key_idx, result_reg);
                        free_register(cg, obj_reg);
                        return result_reg;
                    }
                }
            } else if (node->access.member->type == AST_STRING_INTERP) {                 // "prefix{expr}" key?
                int prefix_idx = key_str_prefix_idx(cg, node->access.member);            // check without emitting
                if (prefix_idx >= 0) {                                                   // fuse-able pattern
                    int num_reg = codegen_expression(cg,                                  // evaluate the number
                                     node->access.member->string_interp.parts->nodes[1]);
                    int packed = (prefix_idx << 16) | (num_reg & 0xFFFF);                // pack op2
                    emit(cg, INST(OP_TABLE_GET_KEY_STR, result_reg, obj_reg, packed),    // fused get
                         node->line);
                    free_register(cg, num_reg);                                          // free number temp
                    free_register(cg, obj_reg);                                          // free object
                    return result_reg;                                                   // return result
                }
                int key_reg = codegen_expression(cg, node->access.member);               // fallback: full interp
                Opcode get_op = is_integer_expression(cg, node->access.member)           // whole-number key?
                                ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                emit(cg, INST(get_op, result_reg, obj_reg, key_reg), node->line);        // get by key
                free_register(cg, key_reg);                                              // free key
                free_register(cg, obj_reg);                                              // free object
                return result_reg;                                                       // return result
            } else {                                                                     // plain member expression
                int key_reg = codegen_expression(cg, node->access.member);               // evaluate key
                Opcode get_op = is_integer_expression(cg, node->access.member)           // whole-number key?
                                ? OP_TABLE_GET_NUM : OP_TABLE_GET;
                emit(cg, INST(get_op, result_reg, obj_reg, key_reg), node->line);        // get by key
                free_register(cg, key_reg);                                              // free key
            }
            free_register(cg, obj_reg);                                                  // free object
            return result_reg;                                                           // return result
        }

        case AST_TABLE_LITERAL: {                                                        // table literal
            int table_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);             // table dest
            emit(cg, INST(OP_NEW_TABLE, table_reg, 0, 0), node->line);                   // create table

            for (int i = 0; i < node->table_literal.items->count; i++) {                      // sequential items
                int value_reg = codegen_expression(cg, node->table_literal.items->nodes[i]);  // evaluate
                emit(cg, INST(OP_TABLE_APPEND, table_reg, value_reg, 0), node->line);         // append
                free_register(cg, value_reg);                                                 // free value
            }

            for (int i = 0; i < node->table_literal.key_values->count; i++) {        // key-value pairs
                ASTNode* kv = node->table_literal.key_values->nodes[i];              // key-value node
                ASTNode* key = kv->binary.left;                                      // key expression
                int value_reg = codegen_expression(cg, kv->binary.right);            // evaluate value

                if (key->type == AST_LITERAL_NUMBER) {                               // numeric key
                    double key_val = key->literal_number.number_value;               // key value

                    if (key_val == (int)key_val && key_val >= 1 && key_val <= 65535) {  // fits immediate
                        emit(cg, INST(OP_TABLE_SET_INT, table_reg, (int)key_val,     // direct array set
                                      value_reg), node->line);
                    } else {                                                         // large or fractional
                        int key_reg = codegen_expression(cg, key);                   // materialize key
                        emit(cg, INST(OP_TABLE_SET, table_reg, key_reg,              // set by numeric key
                                      value_reg), node->line);
                        free_register(cg, key_reg);                                  // free key
                    }
                } else if (key->type == AST_LITERAL_STRING) {                        // string key
                    const char* key_str = key->literal_string.string_value;          // key string
                    int key_idx = bytecode_add_string_constant(cg->chunk, key_str);  // add key constant
                    emit(cg, INST(OP_TABLE_SET_CONST, table_reg, key_idx,            // set by string key
                                  value_reg), node->line);
                } else {                                                             // dynamic key expression
                    int key_reg = codegen_expression(cg, key);                       // evaluate key
                    emit(cg, INST(OP_TABLE_SET, table_reg, key_reg,                  // set by dynamic key
                                  value_reg), node->line);
                    free_register(cg, key_reg);                                      // free key
                }
                free_register(cg, value_reg);                                        // free value
            }
            return table_reg;                                                        // return table
        }

        case AST_STRING_INTERP:                                                   // string interpolation
            return codegen_string_interp(cg, node, dest_hint);

        case AST_TERNARY: {                                                       // ternary expression
            ASTNode* condition = node->ternary.condition;                         // condition
            ASTNode* true_expr = node->ternary.true_expr;                         // true branch
            ASTNode* false_expr = node->ternary.false_expr;                       // false branch

            int dest_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);       // result destination

            LocalNumSnap pre = snap_numbers(cg);                                  // snapshot at branch point

            // num_cache / str_cache entries added while emitting one branch are only valid on that branch's execution path
            int saved_num_count = cg->num_cache.count;
            int saved_str_count = cg->str_cache.count;

            int jump_to_false = codegen_optimized_condition(cg, condition, node->line);
            if (jump_to_false < 0) {                                              // not a fusable comparison
                int cond_reg = codegen_expression(cg, condition);                 // evaluate condition
                jump_to_false = emit(cg, INST(OP_JUMP_IF_FALSE, 0, cond_reg, 0), node->line);
                free_register(cg, cond_reg);                                      // free condition
            }

            codegen_expression_into(cg, true_expr, dest_reg);                     // write true into dest
            LocalNumSnap true_snap = snap_numbers(cg);                            // snapshot after true

            int jump_to_end = bytecode_current_offset(cg->chunk);                 // jump to end
            emit(cg, INST(OP_JUMP, 0, 0, 0), node->line);                         // emit jump

            restore_numbers(cg, pre);                                             // reset to branch point for false
            cg->num_cache.count = saved_num_count;                                // discard then-branch numeric cache
            cg->str_cache.count = saved_str_count;                                // discard then-branch string cache

            int false_addr = bytecode_current_offset(cg->chunk);                  // false address
            PATCH_JUMP(cg, jump_to_false, false_addr);                            // patch jump

            codegen_expression_into(cg, false_expr, dest_reg);                    // write false into dest
            LocalNumSnap false_snap = snap_numbers(cg);                           // snapshot after false

            merge_numbers(cg, true_snap, false_snap);                             // intersect both branches
            free_snap(true_snap);                                                 // release temporaries
            free_snap(false_snap);
            free_snap(pre);

            // after the ternary, register contents depend on which branch ran
            cg->num_cache.count = saved_num_count;
            cg->str_cache.count = saved_str_count;

            int end_addr = bytecode_current_offset(cg->chunk);                    // end address
            PATCH_JUMP(cg, jump_to_end, end_addr);                                // patch jump

            cg->imm_lvn.count = 0;                                                // ternary end is a merge point

            return dest_reg;                                                      // return result
        }

        case AST_AWAIT: {                                                          // await expression
            bool saved_flag = cg->current_call_is_awaited;
            cg->current_call_is_awaited = true;
            int inner_reg = codegen_expression(cg, node->await_expr.expression);   // evaluate inner
            cg->current_call_is_awaited = saved_flag;

            int result_reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);      // result destination
            emit(cg, INST(OP_AWAIT, result_reg, inner_reg, 0), node->line);        // unwrap future
            free_register(cg, inner_reg);                                          // free inner temp
            return result_reg;                                                     // return result
        }

        default: {                                                                // unknown node
            int reg = dest_hint >= 0 ? dest_hint : alloc_register(cg);            // use hint or fresh
            emit(cg, INST(OP_LOAD_BOOL, reg, 0, 0), node->line);                  // load false
            return reg;                                                           // return false
        }
    }
}
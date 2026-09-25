// source/compiler/codegen_match.c
// Match statement codegen with fused constant checks
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// emits a fused match check that jumps to a target when the subject matches
int emit_match_check(CodeGenerator* cg, int subject_reg, ASTNode* pattern, int line) {
    if (pattern->type == AST_LITERAL_NUMBER) {
        int idx = bytecode_add_number_constant(cg->chunk,
                                               pattern->literal_number.number_value);
        return emit(cg, INST(OP_JUMP_MATCH_NUM, 0, subject_reg, idx), line);
    }
    if (pattern->type == AST_LITERAL_STRING) {
        int idx = bytecode_add_string_constant(cg->chunk,
                                               pattern->literal_string.string_value);
        return emit(cg, INST(OP_JUMP_MATCH_STR, 0, subject_reg, idx), line);
    }
    if (pattern->type == AST_LITERAL_BOOL) {
        return emit(cg, INST(OP_JUMP_MATCH_BOOL, 0, subject_reg,
                             pattern->literal_bool.bool_value ? 1 : 0), line);
    }
    if (pattern->type == AST_LITERAL_NONE) {
        return emit(cg, INST(OP_JUMP_MATCH_NONE, 0, subject_reg, 0), line);
    }
    if (pattern->type == AST_UNARY && pattern->unary.op == TOKEN_MINUS &&
        pattern->unary.operand->type == AST_LITERAL_NUMBER) {
        double val = -pattern->unary.operand->literal_number.number_value;
        int idx = bytecode_add_number_constant(cg->chunk, val);
        return emit(cg, INST(OP_JUMP_MATCH_NUM, 0, subject_reg, idx), line);
    }
    return -1;  // parser already reported invalid pattern
}

// clears const-known for any local that `body` assigns; leaves other flags alone
static void invalidate_consts_for_body(CodeGenerator* cg, ASTNode* body) {
    if (!body) return;
    for (int i = 0; i < cg->locals.count; i++) {
        if (body_assigns_name(body, cg->locals.names[i])) {
            cg->locals.const_known[i] = false;
        }
    }
}

// extracts an integer from a constant match pattern; returns false for anything else
static bool pattern_int_value(ASTNode* pattern, int* out) {
    double v;
    if (pattern->type == AST_LITERAL_NUMBER) {
        v = pattern->literal_number.number_value;
    } else if (pattern->type == AST_UNARY && pattern->unary.op == TOKEN_MINUS &&
               pattern->unary.operand->type == AST_LITERAL_NUMBER) {
        v = -pattern->unary.operand->literal_number.number_value;
    } else {
        return false;
    }
    if (v != (double)(long long)v) return false;
    if (v < (double)INT_MIN || v > (double)INT_MAX) return false;
    *out = (int)v;
    return true;
}

// emits a jump-table dispatch when the case patterns are dense integer constants
static bool try_match_jump_table(CodeGenerator* cg, int subject_reg,
                                 ASTNodeList* cases, ASTNode* default_case,
                                 int case_count, int line,
                                 LocalNumSnap match_before,
                                 int saved_num_count, int saved_str_count) {
    if (case_count < 3) return false;                            // chain is cheaper for tiny matches

    int* values = (int*)malloc(sizeof(int) * case_count);        // extracted integer per case
    int min_v = INT_MAX, max_v = INT_MIN;
    for (int i = 0; i < case_count; i++) {
        if (!pattern_int_value(cases->nodes[i]->case_stmt.pattern, &values[i])) {
            free(values);
            return false;                                        // non-integer pattern: bail
        }
        if (values[i] < min_v) min_v = values[i];
        if (values[i] > max_v) max_v = values[i];
    }
    for (int i = 0; i < case_count; i++) {                       // reject duplicate values
        for (int j = i + 1; j < case_count; j++) {
            if (values[i] == values[j]) { free(values); return false; }
        }
    }

    long long range = (long long)max_v - (long long)min_v + 1;
    if (range > 4LL * case_count || range > 64) {                // density and absolute caps
        free(values);
        return false;
    }

    int* addresses = (int*)malloc(sizeof(int) * (size_t)range);  // owned by the constant
    for (long long i = 0; i < range; i++) addresses[i] = -1;     // filled in after bodies

    Constant c;
    memset(&c, 0, sizeof(c));
    c.type = CONST_JUMP_TABLE;
    c.jump_table.addresses = addresses;
    c.jump_table.count     = (int)range;
    int table_idx = bytecode_add_constant(cg->chunk, c);

    emit(cg, INST(OP_JUMP_TABLE, subject_reg, min_v, table_idx), line);  // fused dispatch
    int no_match_jump = emit(cg, INST(OP_JUMP, 0, 0, 0), line);          // out-of-range / hole

    int prev_floor = cg->register_floor;
    int new_floor = subject_reg + 1;                             // pin subject across all bodies
    if (new_floor < prev_floor) new_floor = prev_floor;
    cg->register_floor = new_floor;

    int* case_addrs = (int*)malloc(sizeof(int) * case_count);    // body start per case
    int* end_jumps  = (int*)malloc(sizeof(int) * (case_count + 2));
    int  end_jump_count = 0;

    for (int i = 0; i < case_count; i++) {                       // emit case bodies
        ASTNode* case_node = cases->nodes[i];
        restore_numbers(cg, match_before);
        cg->num_cache.count = saved_num_count;
        cg->str_cache.count = saved_str_count;
        int body_start = bytecode_current_offset(cg->chunk);
        case_addrs[i] = body_start;
        codegen_block(cg, case_node->case_stmt.body);
        if (!stmt_always_exits(case_node->case_stmt.body)) {     // dead JUMP after return/break/continue
            end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);
        }
    }

    int default_addr = -1;
    if (default_case) {                                          // optional default body
        restore_numbers(cg, match_before);
        cg->num_cache.count = saved_num_count;
        cg->str_cache.count = saved_str_count;
        default_addr = bytecode_current_offset(cg->chunk);
        codegen_block(cg, default_case->case_stmt.body);
        if (!stmt_always_exits(default_case->case_stmt.body)) {
            end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);
        }
    }

    cg->register_floor = prev_floor;

    restore_numbers(cg, match_before);                           // merge: clobber consts each body touches
    for (int i = 0; i < case_count; i++) {
        invalidate_consts_for_body(cg, cases->nodes[i]->case_stmt.body);
    }
    if (default_case) {
        invalidate_consts_for_body(cg, default_case->case_stmt.body);
    }
    cg->num_cache.count = saved_num_count;
    cg->str_cache.count = saved_str_count;

    int end_addr = bytecode_current_offset(cg->chunk);
    int fallback = (default_addr >= 0) ? default_addr : end_addr;

    for (int i = 0; i < case_count; i++) {                       // populate table slots
        addresses[values[i] - min_v] = case_addrs[i];
    }
    for (long long i = 0; i < range; i++) {                      // holes route to default/end
        if (addresses[i] == -1) addresses[i] = fallback;
        mark_jump_target(cg, addresses[i]);
    }

    PATCH_JUMP(cg, no_match_jump, fallback);                     // out-of-range fallthrough
    for (int i = 0; i < end_jump_count; i++) {
        PATCH_JUMP(cg, end_jumps[i], end_addr);
    }

    free(values);
    free(case_addrs);
    free(end_jumps);

    free_register(cg, subject_reg);
    cg->imm_lvn.count = 0;                                       // match end is a merge point
    return true;
}

// emits a match statement as a chain of fused constant checks and jumps
void codegen_match_statement(CodeGenerator* cg, ASTNode* node) {
    LocalNumSnap match_before = snap_numbers(cg);                        // snapshot before any case body

    int saved_num_count = cg->num_cache.count;
    int saved_str_count = cg->str_cache.count;

    int subject_reg = codegen_expression(cg, node->match_stmt.subject);  // evaluate subject
    ASTNodeList* cases = node->match_stmt.cases;                         // non-default cases
    ASTNode* default_case = node->match_stmt.default_case;               // optional default
    int case_count = cases->count;                                       // number of non-default cases
    int line = node->line;                                               // source line for debug info

    // dense integer cases: fuse into a single dispatch table
    if (try_match_jump_table(cg, subject_reg, cases, default_case,
                             case_count, line, match_before,
                             saved_num_count, saved_str_count)) {
        free_snap(match_before);                                         // helper consumed everything
        return;
    }

    int* match_jumps = (int*)malloc(sizeof(int) * case_count);           // jump offsets for each case
    int* end_jumps = (int*)malloc(sizeof(int) * (case_count + 2));       // jumps to match end
    int end_jump_count = 0;                                              // number of end jumps emitted

    for (int i = 0; i < case_count; i++) {                               // emit all case checks first
        ASTNode* case_node = cases->nodes[i];
        match_jumps[i] = emit_match_check(cg, subject_reg,
                                          case_node->case_stmt.pattern, line);
    }

    int no_match_jump = emit(cg, INST(OP_JUMP, 0, 0, 0), line);          // jump when no case matched

    int prev_floor = cg->register_floor;                                 // save floor
    int new_floor = subject_reg + 1;                                     // pin subject across cases
    if (new_floor < prev_floor) new_floor = prev_floor;                  // never lower an outer floor
    cg->register_floor = new_floor;
    for (int i = 0; i < case_count; i++) {                               // emit each case body
        ASTNode* case_node = cases->nodes[i];
        restore_numbers(cg, match_before);                               // reset flags before each case body
        cg->num_cache.count = saved_num_count;                           // discard previous case's numeric cache
        cg->str_cache.count = saved_str_count;                           // discard previous case's string cache
        int body_start = bytecode_current_offset(cg->chunk);             // body start address
        PATCH_JUMP(cg, match_jumps[i], body_start);                      // patch match jump
        codegen_block(cg, case_node->case_stmt.body);                    // emit body block
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }

    if (default_case) {                                                  // emit default body
        restore_numbers(cg, match_before);                               // reset flags before default body
        cg->num_cache.count = saved_num_count;                           // discard previous case's numeric cache
        cg->str_cache.count = saved_str_count;                           // discard previous case's string cache
        int default_start = bytecode_current_offset(cg->chunk);          // default body start
        PATCH_JUMP(cg, no_match_jump, default_start);                    // patch no-match jump
        codegen_block(cg, default_case->case_stmt.body);                 // emit default body
        end_jumps[end_jump_count++] = emit(cg, INST(OP_JUMP, 0, 0, 0), line);  // jump to end
    }
    cg->register_floor = prev_floor;                                     // restore floor after match

    // restore numeric flags to the pre-match state, but clear const-known for any local a case body
    restore_numbers(cg, match_before);
    for (int i = 0; i < case_count; i++) {
        invalidate_consts_for_body(cg, cases->nodes[i]->case_stmt.body);
    }
    if (default_case) {
        invalidate_consts_for_body(cg, default_case->case_stmt.body);
    }

    // after the match, register contents depend on which case ran
    cg->num_cache.count = saved_num_count;
    cg->str_cache.count = saved_str_count;

    int end_addr = bytecode_current_offset(cg->chunk);                   // match end address
    if (!default_case) {
        PATCH_JUMP(cg, no_match_jump, end_addr);                         // no default: go to end
    }
    for (int i = 0; i < end_jump_count; i++) {                           // patch all end jumps
        PATCH_JUMP(cg, end_jumps[i], end_addr);
    }

    free(match_jumps);
    free(end_jumps);
    free_snap(match_before);                                             // release snapshot
    free_register(cg, subject_reg);                                      // release subject register

    cg->imm_lvn.count = 0;                                               // match end is a merge point
}
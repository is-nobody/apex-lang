// source/compiler/opt_for_fold.c
// Symbolic folding of constant range and condition loops
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// symbolic value: v(i) = P(i) + Q(i) * r^i, P,Q poly of degree <= SV_MAX_DEG
#define SV_MAX_DEG 3
typedef struct {
    double poly[SV_MAX_DEG + 1];
    bool   has_geo;
    double geo[SV_MAX_DEG + 1];
    double r;
    bool   ok;
} SymF;

// zero value
static SymF sv_zero(void) { SymF v; memset(&v, 0, sizeof(v)); v.ok = true; return v; }
// failure value
static SymF sv_fail(void) { SymF v; memset(&v, 0, sizeof(v)); v.ok = false; return v; }
// constant value
static SymF sv_const(double c) { SymF v = sv_zero(); v.poly[0] = c; return v; }
// loop variable i
static SymF sv_loop(void) { SymF v = sv_zero(); v.poly[1] = 1.0; return v; }

// a + b
static SymF sv_add(SymF a, SymF b) {
    if (!a.ok || !b.ok) return sv_fail();
    SymF r = a;
    for (int k = 0; k <= SV_MAX_DEG; k++) r.poly[k] += b.poly[k];
    if (b.has_geo) {
        if (!r.has_geo) { r.has_geo = true; r.r = b.r;
                          for (int k = 0; k <= SV_MAX_DEG; k++) r.geo[k] = b.geo[k]; }
        else if (r.r == b.r) { for (int k = 0; k <= SV_MAX_DEG; k++) r.geo[k] += b.geo[k]; }
        else return sv_fail();
    }
    return r;
}

// -a
static SymF sv_neg(SymF a) {
    SymF r = a;
    for (int k = 0; k <= SV_MAX_DEG; k++) r.poly[k] = -r.poly[k];
    if (r.has_geo) for (int k = 0; k <= SV_MAX_DEG; k++) r.geo[k] = -r.geo[k];
    return r;
}

// a * b; supports poly*poly and poly*geo (not geo*geo)
static SymF sv_mul(SymF a, SymF b) {
    if (!a.ok || !b.ok) return sv_fail();
    if (a.has_geo && b.has_geo) return sv_fail();
    if (a.has_geo) { SymF t = a; a = b; b = t; }               // keep b as the only geo side
    SymF r = sv_zero();
    for (int i = 0; i <= SV_MAX_DEG; i++) {
        if (a.poly[i] == 0) continue;
        for (int j = 0; i + j <= SV_MAX_DEG; j++)
            r.poly[i + j] += a.poly[i] * b.poly[j];
    }
    if (b.has_geo) {
        r.has_geo = true; r.r = b.r;
        for (int i = 0; i <= SV_MAX_DEG; i++) {
            if (a.poly[i] == 0) continue;
            for (int j = 0; i + j <= SV_MAX_DEG; j++)
                r.geo[i + j] += a.poly[i] * b.geo[j];
        }
    }
    return r;
}

// binding table: name -> SymF for locals defined earlier in the body
typedef struct { const char* name; SymF sv; } SymBind;

// fold an ast expression to a SymF in `var`, using env for prior body locals
static SymF sym_from_ast(CodeGenerator* cg, ASTNode* n, const char* var,
                         SymBind* env, int env_n, ASTNode* body) {
    if (!n) return sv_fail();
    switch (n->type) {
        case AST_LITERAL_NUMBER:
            return sv_const(n->literal_number.number_value);
        case AST_IDENTIFIER: {
            const char* nm = n->identifier.name;
            for (int k = 0; k < env_n; k++)
                if (strcmp(env[k].name, nm) == 0) return env[k].sv;
            if (var && strcmp(nm, var) == 0) return sv_loop();
            if (body && body_assigns_name(body, nm)) return sv_fail();
            double v;
            if (try_fold_number(cg, n, &v)) return sv_const(v);
            return sv_fail();
        }
        case AST_UNARY:
            if (n->unary.op == TOKEN_MINUS)
                return sv_neg(sym_from_ast(cg, n->unary.operand, var, env, env_n, body));
            return sv_fail();
        case AST_BINARY: {
            SymF a = sym_from_ast(cg, n->binary.left,  var, env, env_n, body);
            SymF b = sym_from_ast(cg, n->binary.right, var, env, env_n, body);
            if (!a.ok || !b.ok) return sv_fail();
            switch (n->binary.op) {
                case TOKEN_PLUS:  return sv_add(a, b);
                case TOKEN_MINUS: return sv_add(a, sv_neg(b));
                case TOKEN_STAR:  return sv_mul(a, b);
                default:          return sv_fail();
            }
        }
        default: return sv_fail();
    }
}

// evaluate a SymF at integer point i
static double sv_eval(SymF f, double i) {
    if (!f.ok) return 0.0;
    double v = 0.0;
    for (int k = SV_MAX_DEG; k >= 0; k--) v = v * i + f.poly[k];
    if (f.has_geo) {
        double q = 0.0;
        for (int k = SV_MAX_DEG; k >= 0; k--) q = q * i + f.geo[k];
        v += q * pow(f.r, i);
    }
    return v;
}

// numeric sum of a SymF over the inclusive range [a, b]
static double sv_sum(SymF f, double a, double b) {
    if (!f.ok) return 0.0;
    double M = b - a + 1.0;
    double sp = 0.0;
    {
        double sh[SV_MAX_DEG + 1] = {0};                       // P(a + m) coefficients
        double apow[SV_MAX_DEG + 1]; apow[0] = 1.0;
        for (int k = 1; k <= SV_MAX_DEG; k++) apow[k] = apow[k - 1] * a;
        for (int k = 0; k <= SV_MAX_DEG; k++) {
            double ck = f.poly[k];
            if (ck == 0.0) continue;
            double binom = 1.0;
            for (int j = 0; j <= k; j++) {
                sh[j] += ck * binom * apow[k - j];
                binom = binom * (k - j) / (j + 1);
            }
        }
        double S[4] = { M, M * (M - 1.0) / 2.0,
                        M * (M - 1.0) * (2.0 * M - 1.0) / 6.0,
                        M * M * (M - 1.0) * (M - 1.0) / 4.0 };
        for (int j = 0; j <= SV_MAX_DEG; j++) sp += sh[j] * S[j];
    }
    double sg = 0.0;
    if (f.has_geo) {
        if (f.geo[2] != 0 || f.geo[3] != 0) return 0.0;        // degree > 1 in Q: not supported
        double g0 = f.geo[0] + f.geo[1] * a;                   // Q(a)
        double g1 = f.geo[1];                                  // linear coefficient of m
        double rM = pow(f.r, M);
        double s0, s1;
        if (f.r == 1.0) {
            s0 = M; s1 = M * (M - 1.0) / 2.0;
        } else {
            s0 = (rM - 1.0) / (f.r - 1.0);
            s1 = (f.r - M * rM + (M - 1.0) * f.r * rM) / ((1.0 - f.r) * (1.0 - f.r));
        }
        sg = pow(f.r, a) * (g0 * s0 + g1 * s1);
    }
    return sp + sg;
}

// sum_{j=a_val}^{i} delta(j) as SymF in i, delta must be polynomial
static SymF sv_cumsum_incl(SymF delta, double a_val) {
    if (!delta.ok || delta.has_geo) return sv_fail();
    double sh[SV_MAX_DEG + 1] = {0};                           // delta(a_val + m) in m
    double apow[SV_MAX_DEG + 1]; apow[0] = 1.0;
    for (int k = 1; k <= SV_MAX_DEG; k++) apow[k] = apow[k - 1] * a_val;
    for (int k = 0; k <= SV_MAX_DEG; k++) {
        double ck = delta.poly[k];
        if (ck == 0.0) continue;
        double binom = 1.0;
        for (int j = 0; j <= k; j++) {
            sh[j] += ck * binom * apow[k - j];
            binom = binom * (k - j) / (j + 1);
        }
    }
    if (sh[3] != 0) return sv_fail();                          // cumsum would be degree 4
    SymF r = sv_zero();
    double A = a_val - 1.0;                                    // shift so M = i - A
    r.poly[0] += sh[0] * (-A);
    r.poly[1] += sh[0];
    r.poly[0] += sh[1] * (A*A + A) / 2.0;
    r.poly[1] += sh[1] * (-(2.0*A + 1.0)) / 2.0;
    r.poly[2] += sh[1] * 0.5;
    r.poly[0] += sh[2] * (-2.0*A*A*A - 3.0*A*A - A) / 6.0;
    r.poly[1] += sh[2] * (6.0*A*A + 6.0*A + 1.0) / 6.0;
    r.poly[2] += sh[2] * (-6.0*A - 3.0) / 6.0;
    r.poly[3] += sh[2] * 2.0 / 6.0;
    r.ok = true;
    return r;
}

// value form of ACC_MUL: v(i) = v0 * c^(i - a_val), valid for i >= a_val - 1
static SymF sv_geo_val(double v0, double c, double a_val) {
    if (c == 0.0) return sv_fail();                            // y becomes zero after first iter
    SymF r = sv_zero();
    r.has_geo = true;
    r.geo[0] = v0 / pow(c, a_val);                             // v0 * c^(-a_val)
    r.r = c;
    return r;
}

// one body variable: how it was classified and its resolved form
typedef enum { VK_SIMPLE, VK_ACC_PLUS, VK_ACC_MUL, VK_UNKNOWN } VarKind;

typedef struct {
    const char* name;
    VarKind     kind;
    ASTNode*    rhs;
    double      v0;
    double      c;
    SymF        form;
    bool        form_ready;
    double      final_val;
} VarInfo;

// index of the unique `name = ...` in body (AST_ASSIGN or AST_VAR_DECL)
static int find_unique_assign(ASTNode* body, const char* name) {
    int idx = -1;
    for (int i = 0; i < body->block.statements->count; i++) {
        ASTNode* s = body->block.statements->nodes[i];
        if (!s) continue;
        if (s->type != AST_ASSIGN && s->type != AST_VAR_DECL) continue;
        if (s->var_assign.access_path || !s->var_assign.name) continue;
        if (strcmp(s->var_assign.name, name) == 0) {
            if (idx >= 0) return -1;
            idx = i;
        }
    }
    return idx;
}

// classify a single body assignment as SIMPLE, ACC_PLUS, or ACC_MUL
static VarKind classify_assign(CodeGenerator* cg, ASTNode* stmt,
                               const char* name, ASTNode** out_rhs, double* out_c) {
    if (!stmt) return VK_UNKNOWN;
    if (stmt->type == AST_VAR_DECL) {                          // fresh binding: always SIMPLE
        *out_rhs = stmt->var_assign.value;
        return VK_SIMPLE;
    }
    if (stmt->type != AST_ASSIGN) return VK_UNKNOWN;
    ASTNode* rhs = stmt->var_assign.value;
    if (!rhs) return VK_UNKNOWN;

    if (rhs->type == AST_BINARY && rhs->binary.op == TOKEN_PLUS) {     // v = v + expr
        ASTNode* L = rhs->binary.left;
        ASTNode* R = rhs->binary.right;
        bool l_self = L->type == AST_IDENTIFIER && strcmp(L->identifier.name, name) == 0;
        bool r_self = R->type == AST_IDENTIFIER && strcmp(R->identifier.name, name) == 0;
        if ((l_self && !r_self) || (r_self && !l_self)) {
            *out_rhs = l_self ? R : L;
            return VK_ACC_PLUS;
        }
    }

    if (rhs->type == AST_BINARY && rhs->binary.op == TOKEN_STAR) {     // v = v * const
        ASTNode* L = rhs->binary.left;
        ASTNode* R = rhs->binary.right;
        bool l_self = L->type == AST_IDENTIFIER && strcmp(L->identifier.name, name) == 0;
        bool r_self = R->type == AST_IDENTIFIER && strcmp(R->identifier.name, name) == 0;
        double c;
        if (l_self && !r_self && try_fold_number(cg, R, &c)) { *out_c = c; return VK_ACC_MUL; }
        if (r_self && !l_self && try_fold_number(cg, L, &c)) { *out_c = c; return VK_ACC_MUL; }
    }

    *out_rhs = rhs;
    return VK_SIMPLE;
}

// best-effort: resolve value forms for as many vars as possible
static void resolve_forms(CodeGenerator* cg, ASTNode* body, const char* var,
                          VarInfo* vars, int nv, double a_val) {
    for (int pass = 0; pass < nv + 1; pass++) {
        bool progress = false;
        for (int i = 0; i < nv; i++) {
            if (vars[i].form_ready) continue;
            SymBind env[16]; int en = 0;
            for (int k = 0; k < nv; k++) {
                if (k == i || !vars[k].form_ready) continue;
                env[en].name = vars[k].name;
                env[en].sv = vars[k].form;
                en++;
            }
            SymF sv = sv_fail();
            if (vars[i].kind == VK_SIMPLE) {
                sv = sym_from_ast(cg, vars[i].rhs, var, env, en, body);
            } else if (vars[i].kind == VK_ACC_PLUS) {
                SymF d = sym_from_ast(cg, vars[i].rhs, var, env, en, body);
                if (d.ok) {
                    SymF cs = sv_cumsum_incl(d, a_val);
                    if (cs.ok) sv = sv_add(sv_const(vars[i].v0), cs);
                }
            } else if (vars[i].kind == VK_ACC_MUL) {
                sv = sv_geo_val(vars[i].v0, vars[i].c, a_val);
            }
            if (sv.ok) {
                vars[i].form = sv;
                vars[i].form_ready = true;
                progress = true;
            }
        }
        if (!progress) break;
    }
}

// extract var, start value and last-iteration value from a bare condition loop
static bool analyze_cond_loop(CodeGenerator* cg, ASTNode* node, const char** out_var,
                              double* out_a, double* out_last) {
    if (!node->for_stmt.condition) return false;
    ASTNode* c = node->for_stmt.condition;
    if (c->type != AST_BINARY) return false;
    ASTNode* L = c->binary.left;
    ASTNode* R = c->binary.right;
    ApexTokenType op = c->binary.op;
    const char* var = NULL;
    ASTNode* bound = NULL;
    if (L->type == AST_IDENTIFIER) {
        var = L->identifier.name;
        bound = R;
    } else if (R->type == AST_IDENTIFIER) {                     // var on right: flip
        var = R->identifier.name;
        bound = L;
        switch (op) {
            case TOKEN_LESS:          op = TOKEN_GREATER;       break;
            case TOKEN_LESS_EQUAL:    op = TOKEN_GREATER_EQUAL; break;
            case TOKEN_GREATER:       op = TOKEN_LESS;          break;
            case TOKEN_GREATER_EQUAL: op = TOKEN_LESS_EQUAL;    break;
            case TOKEN_NOT_EQUAL:     break;
            default: return false;
        }
    } else return false;
    int slot = find_local_slot(cg, var);
    if (slot < 0) return false;
    if (!cg->locals.const_known[slot]) return false;
    double a = cg->locals.const_value[slot];
    double b;
    if (!try_fold_number(cg, bound, &b)) return false;
    double last;
    switch (op) {
        case TOKEN_LESS:          last = b - 1.0; break;
        case TOKEN_LESS_EQUAL:    last = b;       break;
        case TOKEN_NOT_EQUAL:     last = b - 1.0; break;
        default: return false;
    }
    if (last < a) return false;
    *out_var = var;
    *out_a = a;
    *out_last = last;
    return true;
}

// folds a constant range loop or bare condition loop with straight-line arithmetic
bool try_emit_symbolic_loop(CodeGenerator* cg, ASTNode* node) {
    const char* var = NULL;
    double a_val = 0.0;
    double last_val = 0.0;
    bool is_bare = false;

    if (node->for_stmt.var_name && node->for_stmt.end && !node->for_stmt.condition) {
        if (node->for_stmt.step) {
            double sv;
            if (!try_fold_number(cg, node->for_stmt.step, &sv) || sv != 1.0) return false;
        }
        if (!try_fold_number(cg, node->for_stmt.start, &a_val)) return false;
        if (!try_fold_number(cg, node->for_stmt.end,   &last_val)) return false;
        if (last_val < a_val) return false;
        var = node->for_stmt.var_name;
    } else if (!node->for_stmt.var_name && !node->for_stmt.end && node->for_stmt.condition) {
        if (!analyze_cond_loop(cg, node, &var, &a_val, &last_val)) return false;
        is_bare = true;
    } else return false;

    ASTNode* body = node->for_stmt.body;
    if (!body || (body->type != AST_BLOCK && body->type != AST_PROGRAM)) return false;
    if (body->block.statements->count == 0) return false;
    if (!is_bare && body_assigns_name(body, var)) return false;

    if (is_bare) {                                              // var assigned exactly once (the increment)
        int vc = 0;
        for (int i = 0; i < body->block.statements->count; i++) {
            ASTNode* s = body->block.statements->nodes[i];
            if (s && s->type == AST_ASSIGN && !s->var_assign.access_path &&
                s->var_assign.name && strcmp(s->var_assign.name, var) == 0) vc++;
        }
        if (vc != 1) return false;
    }

    const char* names[16]; int nv = 0;
    for (int i = 0; i < body->block.statements->count; i++) {
        ASTNode* s = body->block.statements->nodes[i];
        if (!s) return false;
        if (s->type != AST_ASSIGN && s->type != AST_VAR_DECL) return false;
        if (s->var_assign.access_path || !s->var_assign.name) return false;
        const char* nm = s->var_assign.name;

        if (is_bare && strcmp(nm, var) == 0) {                  // must be `var = var + 1`
            ASTNode* rhs = s->var_assign.value;
            if (!rhs || rhs->type != AST_BINARY || rhs->binary.op != TOKEN_PLUS) return false;
            ASTNode* L = rhs->binary.left;
            ASTNode* R = rhs->binary.right;
            bool l_self = L->type == AST_IDENTIFIER && strcmp(L->identifier.name, nm) == 0;
            bool r_self = R->type == AST_IDENTIFIER && strcmp(R->identifier.name, nm) == 0;
            if (!l_self && !r_self) return false;
            double step;
            if (!try_fold_number(cg, l_self ? R : L, &step) || step != 1.0) return false;
            continue;
        }

        bool dup = false;
        for (int k = 0; k < nv; k++) if (strcmp(names[k], nm) == 0) { dup = true; break; }
        if (dup) return false;
        if (nv >= 16) return false;
        names[nv++] = nm;
    }

    VarInfo vars[16];
    for (int i = 0; i < nv; i++) {
        vars[i].name = names[i];
        vars[i].kind = VK_UNKNOWN;
        vars[i].form_ready = false;
        vars[i].final_val = 0.0;
        vars[i].rhs = NULL;
        vars[i].c = 0.0;
        vars[i].v0 = 0.0;

        int slot = find_local_slot(cg, names[i]);
        if (slot < 0) return false;

        int ai = find_unique_assign(body, names[i]);
        if (ai < 0) return false;
        ASTNode* stmt = body->block.statements->nodes[ai];
        double c = 0.0; ASTNode* rhs = NULL;
        VarKind k = classify_assign(cg, stmt, names[i], &rhs, &c);
        if (k == VK_UNKNOWN) return false;
        vars[i].kind = k;
        vars[i].rhs = rhs;
        vars[i].c = c;

        if (k != VK_SIMPLE) {                                   // accumulators need const initial value
            if (!cg->locals.const_known[slot]) return false;
            vars[i].v0 = cg->locals.const_value[slot];
        }
    }

    resolve_forms(cg, body, var, vars, nv, a_val);

    for (int i = 0; i < nv; i++) {                              // compute final value of each variable
        if (vars[i].form_ready) {
            vars[i].final_val = sv_eval(vars[i].form, last_val);
            continue;
        }
        bool needed = false;                                    // form not resolved: allow only if unused
        for (int k = 0; k < nv; k++) {
            if (k == i || !vars[k].rhs) continue;
            if (ast_references_local(vars[k].rhs, vars[i].name)) { needed = true; break; }
        }
        if (needed) return false;
        if (vars[i].kind == VK_ACC_PLUS) {                      // evaluate delta numerically
            SymBind env[16]; int en = 0;
            for (int k = 0; k < nv; k++) {
                if (k == i || !vars[k].form_ready) continue;
                env[en].name = vars[k].name;
                env[en].sv = vars[k].form;
                en++;
            }
            SymF d = sym_from_ast(cg, vars[i].rhs, var, env, en, body);
            if (!d.ok) return false;
            vars[i].final_val = vars[i].v0 + sv_sum(d, a_val, last_val);
        } else if (vars[i].kind == VK_ACC_MUL) {
            double N = last_val - a_val + 1.0;
            vars[i].final_val = vars[i].v0 * pow(vars[i].c, N);
        } else {
            return false;                                       // SIMPLE without form: cannot evaluate
        }
    }

    for (int i = 0; i < nv; i++) {                              // emit one load per variable
        int slot = find_local_slot(cg, vars[i].name);
        int reg = cg->locals.registers[slot];
        double v = vars[i].final_val;
        if (v == (int)v && v >= 0 && v <= 65535) {
            emit(cg, INST(OP_LOAD_NUM_IMM, reg, (int)v, 0), node->line);
        } else {
            int ci = bytecode_add_number_constant(cg->chunk, v);
            emit(cg, INST(OP_LOAD_NUM, reg, ci, 0), node->line);
        }
        cg->locals.const_known[slot] = true;
        cg->locals.const_value[slot] = v;
    }

    int vs = find_local_slot(cg, var);                          // post-loop value of loop var
    if (vs >= 0) {
        double vfin = last_val + 1.0;
        int reg = cg->locals.registers[vs];
        if (vfin == (int)vfin && vfin >= 0 && vfin <= 65535) {
            emit(cg, INST(OP_LOAD_NUM_IMM, reg, (int)vfin, 0), node->line);
        } else {
            int ci = bytecode_add_number_constant(cg->chunk, vfin);
            emit(cg, INST(OP_LOAD_NUM, reg, ci, 0), node->line);
        }
        cg->locals.const_known[vs] = true;
        cg->locals.const_value[vs] = vfin;
    }

    return true;
}
#include "generate_ir.h"
#include "tokenizer.h"
#include "parser.h"
#include "codegen.h"
#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_ir(BytecodeChunk* chunk, const char* filename) {
    printf("\n; ==========================================\n");
    printf("; Apex IR: %s\n", filename);
    printf("; ==========================================\n\n");
    
    // FIX 4: Print Constants
    printf(".section .constants\n");
    for (int i = 0; i < chunk->const_count; i++) {
        Constant* c = &chunk->constants[i];
        printf("  C%-4d: ", i);
        switch (c->type) {
            case CONST_NUMBER: printf("%g\n", c->number_value); break;
            case CONST_STRING: printf("\"%s\"\n", c->string_value); break;
            case CONST_BOOL:   printf("%s\n", c->bool_value ? "true" : "false"); break;
            case CONST_FUNCTION: printf("<function %d>\n", c->function_index); break;
            default:           printf("unknown\n"); break;
        }
    }
    printf("\n");

    // FIX 4: Print Globals
    printf(".section .globals\n");
    for (int i = 0; i < chunk->global_count; i++) {
        printf("  G%-4d: %s\n", i, chunk->globals[i].name);
    }
    printf("\n");

    // Print Code
    printf(".section .text\n");
    int current_func_idx = 0;
    bool in_func = false;

    for (int pc = 0; pc < chunk->code_count; pc++) {
        if (current_func_idx < chunk->func_count && pc == chunk->functions[current_func_idx].address) {
            if (in_func) {
                printf(".endfunc\n\n");
            }
            FunctionInfo* func = &chunk->functions[current_func_idx];
            printf(".func %s arity=%d locals=%d\n", func->name, func->arity, func->local_count);
            
            // FIX 3: Print local variable mapping
            if (func->local_names && func->local_count > 0) {
                printf("  ; locals: ");
                for (int i = 0; i < func->local_count; i++) {
                    printf("R%d=%s ", i, func->local_names[i]);
                }
                printf("\n");
            }
            
            in_func = true;
            current_func_idx++;
        }

        Instruction* inst = &chunk->code[pc];
        const char* op_name = opcode_name(inst->opcode);
        
        printf("  %04d: %-15s ", pc, op_name);
        if (inst->opcode == OP_LOAD_CONST || inst->opcode == OP_LOAD_CONST_NUM) {
            printf("R%d, C%d", inst->operands[0], inst->operands[1]);
        } else if (inst->opcode == OP_MOVE || inst->opcode == OP_ADD || inst->opcode == OP_SUB || 
                   inst->opcode == OP_MUL || inst->opcode == OP_DIV || inst->opcode == OP_MOD ||
                   inst->opcode == OP_CMP_EQ || inst->opcode == OP_CMP_NEQ || inst->opcode == OP_CMP_LT ||
                   inst->opcode == OP_CMP_GT || inst->opcode == OP_CMP_LTE || inst->opcode == OP_CMP_GTE) {
            printf("R%d, R%d, R%d", inst->operands[0], inst->operands[1], inst->operands[2]);
        } else {
            printf("%d, %d, %d", inst->operands[0], inst->operands[1], inst->operands[2]);
        }
        printf("\n");
    }

    if (in_func) {
        printf(".endfunc\n");
    }
}

int compile_command(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "\033[31mError: Missing filename for 'compile' command.\033[0m\n");
        return 1;
    }
    const char* filename = argv[2];
    FILE* f_check = fopen(filename, "rb");
    if (!f_check) {
        fprintf(stderr, "\033[31mError: Source file '%s' does not exist.\033[0m\n", filename);
        return 1;
    }
    fclose(f_check);

    // FIX 1: No dependency scanning needed. The parser handles imports natively.
    printf("\033[36mCompiling: %s\033[0m\n", filename);

    FILE* f = fopen(filename, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = (char*)malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    Tokenizer* tokenizer = tokenizer_create(source, filename);
    int token_count;
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);
    
    Parser* parser = parser_create(tokens, token_count, filename, source);
    ASTNode* ast = parser_parse(parser);
    
    if (!parser_had_errors(parser) && ast) {
        BytecodeChunk* chunk = bytecode_create();
        CodeGenerator* cg = codegen_create(chunk);
        if (codegen_generate(cg, ast)) {
            print_ir(chunk, filename);
        }
        codegen_destroy(cg);
        bytecode_destroy(chunk);
    }
    
    ast_free(ast);
    parser_destroy(parser);
    tokenizer_destroy(tokenizer);
    free(source);
    return 0;
}
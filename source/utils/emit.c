// source/utils/emit.c
// Implementation of Bytecode Disassembler for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "emit.h"
#include "execute.h"
#include "tokenizer.h"
#include "parser.h"
#include "ast.h"
#include "bytecode.h"
#include "codegen.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ansi palette matching error.c / execute.c / build.c style
#define C_RESET     "\033[0m"       // reset all colors
#define C_RED       "\033[31m"      // red for errors
#define C_GREEN     "\033[32m"      // green for strings
#define C_YELLOW    "\033[33m"      // yellow for identifiers
#define C_BLUE      "\033[34m"      // blue for opcodes
#define C_MAGENTA   "\033[35m"      // magenta for literals
#define C_CYAN      "\033[36m"      // cyan for registers
#define C_GRAY      "\033[90m"      // gray for addresses and hints
#define C_BOLD      "\033[1m"       // bold for section headers

// max chars of a string constant to display before truncating
#define MAX_STR_DISPLAY 40

// prints a C-string with escape sequences, truncated at MAX_STR_DISPLAY
static void print_escaped_string(const char* s, FILE* out) {      // s: string to print, out: output stream
    fputs(C_GREEN, out);                                          // open green color for strings
    fputc('"', out);                                              // opening quote
    if (s) {                                                      // guard against null string
        int count = 0;                                            // characters printed so far
        while (*s && count < MAX_STR_DISPLAY) {                   // iterate until end or limit
            unsigned char c = (unsigned char)*s++;                // fetch byte and advance
            switch (c) {                                          // escape special characters
                case '"':  fputs("\\\"", out); break;             // escaped double quote
                case '\\': fputs("\\\\", out); break;             // escaped backslash
                case '\n': fputs("\\n",  out); break;             // escaped newline
                case '\r': fputs("\\r",  out); break;             // escaped carriage return
                case '\t': fputs("\\t",  out); break;             // escaped tab
                default:                                          // other characters
                    if (c < 32) fprintf(out, "\\x%02x", c);       // non-printable as hex
                    else        fputc(c, out);                    // printable as-is
                    break;
            }
            count++;                                              // one more char written
        }
        if (*s) fputs("...", out);                                // truncation indicator
    }
    fputc('"', out);                                              // closing quote
    fputs(C_RESET, out);                                          // reset color
}

// prints a constant pool entry in human-readable form with color
static void print_const_value(BytecodeChunk* chunk, int idx, FILE* out) {  // chunk: bytecode, idx: constant index
    if (idx < 0 || idx >= chunk->const_count) {                   // out of range check
        fprintf(out, C_RED "<const#%d?>" C_RESET, idx);           // red error marker
        return;                                                   // nothing else to do
    }
    Constant* c = &chunk->constants[idx];                         // fetch constant entry
    switch (c->type) {                                            // dispatch by type
        case CONST_NUMBER:                                        // numeric literal
            fprintf(out, C_MAGENTA "%g" C_RESET, c->number_value); // magenta number
            break;
        case CONST_STRING:                                        // string literal
            print_escaped_string(c->string_value, out);           // delegate to string printer
            break;
        case CONST_BOOL:                                          // boolean literal
            fputs(C_MAGENTA, out);                                // open magenta
            fputs(c->bool_value ? "true" : "false", out);         // print value
            fputs(C_RESET, out);                                  // reset color
            break;
        case CONST_NONE:                                          // none literal
            fputs(C_MAGENTA "none" C_RESET, out);                 // magenta none
            break;
        case CONST_FUNCTION:                                      // function reference
            fputs(C_YELLOW, out);                                 // open yellow
            if (c->function_index >= 0 && c->function_index < chunk->func_count)  // valid index
                fprintf(out, "<fn %s>", chunk->functions[c->function_index].name); // named function
            else
                fprintf(out, "<fn#%d>", c->function_index);       // fallback index form
            fputs(C_RESET, out);                                  // reset color
            break;
        default:                                                  // unknown type
            fputs(C_RED "<?>" C_RESET, out);                      // red unknown marker
            break;
    }
}

// prints a global variable name for a given index
static void print_global_name(BytecodeChunk* chunk, int idx, FILE* out) {  // chunk: bytecode, idx: global index
    if (idx >= 0 && idx < chunk->global_count)                    // valid index
        fprintf(out, C_YELLOW "%s" C_RESET, chunk->globals[idx].name);  // yellow identifier
    else
        fprintf(out, C_RED "<global#%d>" C_RESET, idx);           // red fallback
}

// prints a function name for a given index
static void print_function_name(BytecodeChunk* chunk, int idx, FILE* out) {  // chunk: bytecode, idx: function index
    if (idx >= 0 && idx < chunk->func_count)                      // valid index
        fprintf(out, C_YELLOW "%s" C_RESET, chunk->functions[idx].name);  // yellow identifier
    else
        fprintf(out, C_RED "<fn#%d>" C_RESET, idx);               // red fallback
}

// register pretty-printer: R0 -> cyan
#define REG(r) fprintf(out, C_CYAN "R%d" C_RESET, (r))            // r: register number

// disassembles one instruction and prints it with meaningful operand info
static void emit_instruction(BytecodeChunk* chunk, int offset, FILE* out) {  // chunk: bytecode, offset: instruction index
    Instruction* inst = &chunk->code[offset];                     // fetch instruction
    Opcode op = inst->opcode;                                     // opcode enum
    int a = inst->operands[0];                                    // operand 1
    int b = inst->operands[1];                                    // operand 2
    int c = inst->operands[2];                                    // operand 3

    // address (gray) + opcode (bold yellow)
    fprintf(out, C_GRAY "%4d" C_RESET "  " C_BOLD C_BLUE "%-18s" C_RESET,  // format address and opcode
            offset, opcode_name(op));                             // values to print

    switch (op) {                                                 // dispatch by opcode
        case OP_MOVE:                                             // move between registers
            REG(a); fputs(" <- ", out); REG(b);                   // dst <- src
            break;
        case OP_LOAD_CONST:                                       // load constant from pool
            REG(a); fputs(" <- ", out);                           // dst <- 
            print_const_value(chunk, b, out);                     // constant value
            break;
        case OP_LOAD_NUM_IMM:                                     // load immediate integer
            REG(a); fprintf(out, " <- " C_MAGENTA "%d" C_RESET, b);  // dst <- number
            break;
        case OP_LOAD_NUM:                                         // load full double
            REG(a); fputs(" <- ", out);                           // dst <-
            print_const_value(chunk, b, out);                     // constant value
            break;
        case OP_LOAD_BOOL:                                        // load boolean literal
            REG(a); fprintf(out, " <- " C_MAGENTA "%s" C_RESET, b ? "true" : "false");  // dst <- bool
            break;
        case OP_LOAD_NONE:                                        // load none
            REG(a); fputs(" <- " C_MAGENTA "none" C_RESET, out);  // dst <- none
            break;

        case OP_ADD: REG(a); fputs(" <- ", out); REG(b); fputs(" + ", out); REG(c); break;   // dst <- a + b
        case OP_SUB: REG(a); fputs(" <- ", out); REG(b); fputs(" - ", out); REG(c); break;   // dst <- a - b
        case OP_MUL: REG(a); fputs(" <- ", out); REG(b); fputs(" * ", out); REG(c); break;   // dst <- a * b
        case OP_DIV: REG(a); fputs(" <- ", out); REG(b); fputs(" / ", out); REG(c); break;   // dst <- a / b
        case OP_MOD: REG(a); fputs(" <- ", out); REG(b); fputs(" % ", out); REG(c); break;   // dst <- a % b

        case OP_NEG:                                              // unary minus
            REG(a); fputs(" <- -", out); REG(b);                  // dst <- -src
            break;

        case OP_INC: REG(a); fputs("++", out); break;             // register increment
        case OP_DEC: REG(a); fputs("--", out); break;             // register decrement

        case OP_JUMP:                                             // unconditional jump
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET, a);  // -> target
            break;

        case OP_JUMP_IF_FALSE:                                    // jump when falsy
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if !", a); REG(b);  // -> target if !cond
            break;
        case OP_JUMP_IF_EQ:                                       // jump when equal
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" == ", out); REG(c);  // -> target if a == b
            break;
        case OP_JUMP_IF_NEQ:                                      // jump when not equal
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" != ", out); REG(c);  // -> target if a != b
            break;
        case OP_JUMP_IF_EQ_NUM:                                   // jump when equal (unboxed)
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b);  // -> target if a == b
            fputs(" == ", out); REG(c); fputs(" " C_GRAY "[num]" C_RESET, out);           // num hint
            break;
        case OP_JUMP_IF_NEQ_NUM:                                  // jump when not equal (unboxed)
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b);  // -> target if a != b
            fputs(" != ", out); REG(c); fputs(" " C_GRAY "[num]" C_RESET, out);           // num hint
            break;
        case OP_JUMP_IF_LT:                                       // jump when less
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" < ", out); REG(c);   // -> target if a < b
            break;
        case OP_JUMP_IF_GT:                                       // jump when greater
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" > ", out); REG(c);   // -> target if a > b
            break;
        case OP_JUMP_IF_LTE:                                      // jump when less or equal
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" <= ", out); REG(c);  // -> target if a <= b
            break;
        case OP_JUMP_IF_GTE:                                      // jump when greater or equal
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" >= ", out); REG(c);  // -> target if a >= b
            break;

        case OP_JUMP_MATCH_NUM:                                   // match number case
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" == ", out);  // -> target if subj ==
            print_const_value(chunk, c, out);                     // number constant
            break;
        case OP_JUMP_MATCH_STR:                                   // match string case
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b); fputs(" == ", out);  // -> target if subj ==
            print_const_value(chunk, c, out);                     // string constant
            break;
        case OP_JUMP_MATCH_BOOL:                                  // match boolean case
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b);  // -> target if subj ==
            fprintf(out, " == " C_MAGENTA "%s" C_RESET, c ? "true" : "false");            // boolean literal
            break;
        case OP_JUMP_MATCH_NONE:                                  // match none case
            fprintf(out, C_BLUE "->" C_RESET " " C_GRAY "%d" C_RESET " if ", a); REG(b);  // -> target if subj ==
            fputs(" == " C_MAGENTA "none" C_RESET, out);          // none literal
            break;

        case OP_CMP_EQ:      REG(a); fputs(" <- (", out); REG(b); fputs(" == ", out); REG(c); fputs(")", out); break;  // dst <- (a == b)
        case OP_CMP_NEQ:     REG(a); fputs(" <- (", out); REG(b); fputs(" != ", out); REG(c); fputs(")", out); break;  // dst <- (a != b)
        case OP_CMP_EQ_NUM:  REG(a); fputs(" <- (", out); REG(b); fputs(" == ", out); REG(c); fputs(")", out);         // dst <- (a == b)
                             fputs(" " C_GRAY "[num]" C_RESET, out); break;                                            // num hint
        case OP_CMP_NEQ_NUM: REG(a); fputs(" <- (", out); REG(b); fputs(" != ", out); REG(c); fputs(")", out);         // dst <- (a != b)
                             fputs(" " C_GRAY "[num]" C_RESET, out); break;                                            // num hint
        case OP_CMP_LT:      REG(a); fputs(" <- (", out); REG(b); fputs(" < ", out);  REG(c); fputs(")", out); break;  // dst <- (a < b)
        case OP_CMP_GT:      REG(a); fputs(" <- (", out); REG(b); fputs(" > ", out);  REG(c); fputs(")", out); break;  // dst <- (a > b)
        case OP_CMP_LTE:     REG(a); fputs(" <- (", out); REG(b); fputs(" <= ", out); REG(c); fputs(")", out); break;  // dst <- (a <= b)
        case OP_CMP_GTE:     REG(a); fputs(" <- (", out); REG(b); fputs(" >= ", out); REG(c); fputs(")", out); break;  // dst <- (a >= b)

        case OP_FOR_INIT:                                         // initialize numeric for
            REG(a); fputs("  end=", out); REG(b); fputs(" step=", out); REG(c);  // var end step
            break;
        case OP_FOR_NEXT:                                         // advance numeric for
            REG(a); fputs("  " C_BLUE "exit" C_RESET "=" C_GRAY, out); fprintf(out, "%d" C_RESET, b);  // var exit=target
            break;
        case OP_TABLE_ITER_INIT:                                  // initialize table iterator
            REG(a);                                               // table register
            break;
        case OP_TABLE_ITER_NEXT:                                  // advance table iterator
            REG(a); fputs("  " C_BLUE "exit" C_RESET "=" C_GRAY, out); fprintf(out, "%d" C_RESET, c);  // var exit=target
            break;
        case OP_POP_ITER:                                         // pop iterator on break
            break;

        case OP_TABLE_GET:                                        // table[key]
            REG(a); fputs(" <- ", out); REG(b); fputc('[', out); REG(c); fputc(']', out);  // dst <- tbl[key]
            break;
        case OP_TABLE_GET_CONST:                                  // table[const_key]
            REG(a); fputs(" <- ", out); REG(b); fputc('[', out);  // dst <- tbl[
            print_const_value(chunk, c, out);                     // constant key
            fputc(']', out);                                      // closing bracket
            break;
        case OP_TABLE_GET_INT:                                    // table[int_key]
            REG(a); fputs(" <- ", out); REG(b);                   // dst <- tbl
            fprintf(out, "[" C_MAGENTA "%d" C_RESET "]", c);      // [int]
            break;
        case OP_TABLE_SET:                                        // table[key] = value
            REG(a); fputc('[', out); REG(b); fputs("] <- ", out); REG(c);  // tbl[key] <- val
            break;
        case OP_TABLE_SET_CONST:                                  // table[const_key] = value
            REG(a); fputc('[', out);                              // tbl[
            print_const_value(chunk, b, out);                     // constant key
            fputs("] <- ", out); REG(c);                          // ] <- val
            break;
        case OP_TABLE_SET_INT:                                    // table[int_key] = value
            REG(a);                                               // tbl
            fprintf(out, "[" C_MAGENTA "%d" C_RESET "] <- ", b);  // [int] <- val
            REG(c);                                               // value register
            break;
        case OP_TABLE_APPEND:                                     // append positional item
            REG(a); fputs(" += ", out); REG(b);                   // tbl += val
            break;
        case OP_NEW_TABLE:                                        // create empty table
            REG(a); fputs(" <- []", out);                         // dst <- []
            break;

        case OP_CONCAT:                                           // string concatenation
            REG(a); fputs(" <- ", out); REG(b); fputs(" .. ", out); REG(c);  // dst <- a .. b
            break;

        case OP_AND: REG(a); fputs(" <- ", out); REG(b); fputs(" and ", out); REG(c); break;  // dst <- a and b
        case OP_OR:  REG(a); fputs(" <- ", out); REG(b); fputs(" or ", out);  REG(c); break;  // dst <- a or b
        case OP_NOT: REG(a); fputs(" <- not ", out); REG(b); break;                           // dst <- not a

        case OP_PUSH_ARG:                                         // push call argument
            REG(a);                                               // argument register
            break;
        case OP_CALL:                                             // general call
            REG(a); fputs(" <- ", out);                           // dst <-
            print_function_name(chunk, b, out);                   // function name
            fprintf(out, "(" C_MAGENTA "%d" C_RESET " args)", c); // (n args)
            break;
        case OP_CALL_BUILTIN:                                     // builtin call
            REG(a); fputs(" <- " C_MAGENTA "builtin" C_RESET " ", out);  // dst <- builtin
            print_const_value(chunk, b, out);                     // builtin name
            fprintf(out, "(" C_MAGENTA "%d" C_RESET " args)", c); // (n args)
            break;
        case OP_CALL_0:                                           // zero-arg fast call
            REG(a); fputs(" <- ", out);                           // dst <-
            print_function_name(chunk, b, out);                   // function name
            fputs("()", out);                                     // ()
            break;
        case OP_CALL_1:                                           // one-arg fast call
            REG(a); fputs(" <- ", out);                           // dst <-
            print_function_name(chunk, b, out);                   // function name
            fputc('(', out); REG(c); fputc(')', out);             // (arg)
            break;
        case OP_CALL_2:                                           // two-arg fast call
            REG(a); fputs(" <- ", out);                           // dst <-
            print_function_name(chunk, b, out);                   // function name
            fputc('(', out); REG(c); fputs(", ", out); REG(c + 1); fputc(')', out);  // (arg1, arg2)
            break;
        case OP_RETURN:                                           // return value
            REG(a);                                               // return register
            break;
        case OP_RETURN_NUM:                                       // return number
            REG(a); fputs(" " C_GRAY "[num]" C_RESET, out);       // register with num hint
            break;
        case OP_RETURN_NONE:                                      // bare return
            break;

        case OP_LOAD_GLOBAL:                                      // load global variable
            REG(a); fputs(" <- ", out);                           // dst <-
            print_global_name(chunk, b, out);                     // global name
            break;
        case OP_STORE_GLOBAL:                                     // store global variable
            REG(a); fputs(" -> ", out);                           // src ->
            print_global_name(chunk, b, out);                     // global name
            break;

        case OP_HALT:                                             // stop execution
            break;

        default:                                                  // unknown opcode
            fprintf(out, "%d, %d, %d", a, b, c);                  // raw operands
            break;
    }
    fputc('\n', out);                                             // end of line
}

// main entry for the `apex emit` command
int emit_command(int argc, char** argv) {                         // argc/argv: cli arguments
    if (argc < 3) {                                               // need at least one filename
        fputs(C_RED "Error: Missing filename.\n"                  // print usage
                    "Usage: apex emit <filename.apex>\n" C_RESET, stderr);
        return 1;                                                 // error exit code
    }

    const char* filename = argv[2];                               // source file path

    FILE* f = fopen(filename, "rb");                              // open source file
    if (!f) {                                                     // check open
        fprintf(stderr, C_RED "Error: Source file '%s' does not exist.\n" C_RESET, filename);  // error message
        return 1;                                                 // error exit code
    }

    fseek(f, 0, SEEK_END);                                        // seek to end
    long size = ftell(f);                                         // get file size
    fseek(f, 0, SEEK_SET);                                        // seek to start

    char* source = (char*)malloc(size + 1);                       // allocate source buffer
    if (!source) {                                                // check allocation
        print_error("Memory allocation failed");                  // print error
        fclose(f);                                                // close file
        return 1;                                                 // error exit code
    }

    if (fread(source, 1, size, f) != (size_t)size) {              // read file content
        print_error("Cannot read file '%s'", filename);           // print error
        fclose(f);                                                // close file
        free(source);                                             // free buffer
        return 1;                                                 // error exit code
    }
    source[size] = '\0';                                          // null terminate
    fclose(f);                                                    // close file

    Tokenizer* tokenizer = tokenizer_create(source, filename);    // create tokenizer
    int token_count;                                              // token count storage
    Token* tokens = tokenizer_tokenize(tokenizer, &token_count);  // tokenize source

    if (!tokens || tokenizer_has_error(tokenizer)) {              // check tokenization errors
        cleanup_all(tokenizer, NULL, NULL, NULL, NULL, NULL, source);  // cleanup tokenizer
        return 1;                                                 // error exit code
    }

    Parser* parser = parser_create(tokens, token_count, filename, source);  // create parser
    ASTNode* ast = parse_program(parser);                         // parse ast

    if (!ast || parser_had_errors(parser)) {                      // check parsing errors
        cleanup_all(tokenizer, parser, ast, NULL, NULL, NULL, source);  // cleanup parser
        return 1;                                                 // error exit code
    }

    BytecodeChunk* chunk = bytecode_create();                     // create bytecode chunk
    CodeGenerator* cg = codegen_create(chunk);                    // create code generator

    if (!codegen_generate(cg, ast)) {                             // generate bytecode
        print_error("Code generation failed for '%s'", filename); // print error
        cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source);  // cleanup all
        return 1;                                                 // error exit code
    }

    // header line
    printf(C_BOLD C_CYAN "Bytecode for %s" C_RESET "\n", filename);  // filename header
    printf(C_GRAY "%d instructions, %d constants, %d globals, %d functions"  // summary line
                  C_RESET "\n\n",
           chunk->code_count, chunk->const_count,                 // instruction and constant counts
           chunk->global_count, chunk->func_count);               // global and function counts

    // linear code stream, marking each named function start as we pass it
    for (int i = 0; i < chunk->code_count; i++) {                 // iterate all instructions
        // skip fi == 0 (the __entry-apex__ wrapper) since it starts at offset 0
        for (int fi = 1; fi < chunk->func_count; fi++) {          // check each non-entry function
            if (chunk->functions[fi].address == i) {              // function begins here
                printf("\n" C_BOLD C_YELLOW "function %s" C_RESET // function name header
                       C_GRAY " (arity=%d, max_regs=%d)" C_RESET "\n",
                       chunk->functions[fi].name,                 // name
                       chunk->functions[fi].arity,                // arity
                       chunk->functions[fi].max_registers);       // max registers
            }
        }
        emit_instruction(chunk, i, stdout);                       // print instruction
    }

    // constants
    if (chunk->const_count > 0) {                                 // only if any constants
        printf("\n" C_BOLD "constants" C_RESET "\n");             // section header
        for (int i = 0; i < chunk->const_count; i++) {            // iterate constants
            Constant* cn = &chunk->constants[i];                  // fetch entry
            fprintf(stdout, "  " C_GRAY "[%2d]" C_RESET " ", i);  // index
            switch (cn->type) {                                   // type label
                case CONST_NUMBER:   fputs(C_MAGENTA "NUMBER" C_RESET " ", stdout); break;  // number label
                case CONST_STRING:   fputs(C_GREEN   "STRING" C_RESET " ", stdout); break;  // string label
                case CONST_BOOL:     fputs(C_MAGENTA "BOOL"   C_RESET "   ", stdout); break;  // bool label
                case CONST_NONE:     fputs(C_MAGENTA "NONE"   C_RESET "   ", stdout); break;  // none label
                case CONST_FUNCTION: fputs(C_YELLOW  "FUNC"   C_RESET "   ", stdout); break;  // function label
                default:             fputs(C_RED     "?"      C_RESET "      ", stdout); break;  // unknown label
            }
            print_const_value(chunk, i, stdout);                  // print value
            fputc('\n', stdout);                                  // newline
        }
    }

    // globals
    if (chunk->global_count > 0) {                                // only if any globals
        printf("\n" C_BOLD "globals" C_RESET "\n");               // section header
        for (int i = 0; i < chunk->global_count; i++) {           // iterate globals
            fprintf(stdout, "  " C_GRAY "[%2d]" C_RESET " " C_YELLOW "%s" C_RESET "\n",  // index and name
                    i, chunk->globals[i].name);                   // values
        }
    }

    // functions summary
    if (chunk->func_count > 0) {                                  // only if any functions
        printf("\n" C_BOLD "functions" C_RESET "\n");             // section header
        for (int i = 0; i < chunk->func_count; i++) {             // iterate functions
            FunctionInfo* fn = &chunk->functions[i];              // fetch entry
            fprintf(stdout, "  " C_GRAY "[%2d]" C_RESET " " C_YELLOW "%-30s" C_RESET  // index and name
                            " " C_BLUE "addr" C_RESET "=" C_GRAY "%-5d" C_RESET        // address
                            " " C_BLUE "arity" C_RESET "=" C_MAGENTA "%d" C_RESET      // arity
                            " " C_BLUE "max_regs" C_RESET "=" C_MAGENTA "%d" C_RESET "\n",  // max registers
                    i, fn->name, fn->address, fn->arity, fn->max_registers);  // values
        }
    }

    printf("\n");                                                 // trailing blank line

    cleanup_all(tokenizer, parser, ast, cg, chunk, NULL, source); // cleanup all resources
    return 0;                                                     // success exit code
}
// source/compiler/codegen_scope.c
// Locals table and register pool management
// https://github.com/is-nobody/apex-lang
// MIT license

#include "codegen_internal.h"
#include <stdlib.h>
#include <string.h>

// allocates a new virtual register for temporary values
int alloc_register(CodeGenerator* cg) {
    if (cg->next_register < cg->cache_floor) {  // never allocate inside pinned cache
        cg->next_register = cg->cache_floor;    // bump up to the cache floor
    }
    int reg = cg->next_register++;   // allocate next register
    if (reg >= cg->max_registers) {  // track max used
        cg->max_registers = reg + 1;
    }
    return reg;                      // return register index
}

// releases a register if it's not a local variable and is the last allocated temp
void free_register(CodeGenerator* cg, int reg) {
    for (int i = 0; i < cg->locals.count; i++) {  // check if local var
        if (cg->locals.registers[i] == reg) {
            return;                               // don't free local vars
        }
    }
    if (reg < cg->cache_floor) {                  // pinned by the numeric constant cache
        return;                                   // keep cached registers alive
    }
    for (int i = 0; i < cg->imm_lvn.count; i++) { // pinned by the LVN cache?
        if (cg->imm_lvn.entries[i].result_reg == reg) {
            return;                               // keep LVN-cached value alive
        }
    }
    if (reg == cg->next_register - 1) {           // only free last temp
        cg->next_register--;                      // decrement register count
    }
}

// finds the register holding a local variable by name, returns -1 if not found
int find_local(CodeGenerator* cg, const char* name) {
    if (!name) return -1;                              // guard against null
    for (int i = 0; i < cg->locals.count; i++) {       // iterate locals
        if (strcmp(cg->locals.names[i], name) == 0) {  // compare names
            return cg->locals.registers[i];            // return register
        }
    }
    return -1;                                         // not found
}

// returns the slot index into cg->locals for the given name, or -1 if not found
int find_local_slot(CodeGenerator* cg, const char* name) {
    if (!name) return -1;                              // guard against null
    for (int i = 0; i < cg->locals.count; i++) {       // iterate locals
        if (strcmp(cg->locals.names[i], name) == 0) {  // compare names
            return i;                                  // return slot index
        }
    }
    return -1;                                         // not found
}

// adds a local variable and assigns it a register, returns the register
int add_local(CodeGenerator* cg, const char* name) {
    int existing = find_local(cg, name);                                   // check if already exists
    if (existing >= 0) return existing;                                    // return existing register

    if (cg->locals.count >= cg->locals.capacity) {                                      // need more space
        cg->locals.capacity = cg->locals.capacity == 0 ? 16 : cg->locals.capacity * 2;  // double capacity
        cg->locals.names = (char**)realloc(cg->locals.names,                            // resize names array
                                           sizeof(char*) * cg->locals.capacity);
        cg->locals.registers = (int*)realloc(cg->locals.registers,                      // resize registers array
                                             sizeof(int) * cg->locals.capacity);
        cg->locals.is_number = (bool*)realloc(cg->locals.is_number,                     // resize numeric-ness array
                                             sizeof(bool) * cg->locals.capacity);
        cg->locals.is_integer = (bool*)realloc(cg->locals.is_integer,                   // resize integer-ness array
                                              sizeof(bool) * cg->locals.capacity);
        cg->locals.const_known = (bool*)realloc(cg->locals.const_known,                 // resize const-known array
                                              sizeof(bool) * cg->locals.capacity);
        cg->locals.const_value = (double*)realloc(cg->locals.const_value,               // resize const-value array
                                              sizeof(double) * cg->locals.capacity);
    }
    int reg = alloc_register(cg);                                          // allocate new register
    cg->locals.names[cg->locals.count] = strdup(name);                     // copy name
    cg->locals.registers[cg->locals.count] = reg;                          // store register
    cg->locals.is_number[cg->locals.count] = false;                        // unknown until assigned
    cg->locals.is_integer[cg->locals.count] = false;                       // unknown until assigned
    cg->locals.const_known[cg->locals.count] = false;                      // not a known constant yet
    cg->locals.const_value[cg->locals.count] = 0.0;                        // placeholder
    cg->locals.count++;                                                    // increment count
    return reg;                                                            // return register
}

// returns next_register value with all locals preserved and no temps
int locals_high_water(CodeGenerator* cg) {
    int highest = -1;                                          // no locals yet
    for (int i = 0; i < cg->locals.count; i++) {               // scan local slots
        if (cg->locals.registers[i] > highest) {               // track max
            highest = cg->locals.registers[i];
        }
    }
    return highest + 1;                                        // next free index
}
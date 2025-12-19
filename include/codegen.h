#ifndef CODEGEN_H
#define CODEGEN_H

#include "common.h"
#include "parser.h"
#include "symbol.h"
#include "target.h"

/* Code generator structure */
typedef struct {
    const char* output_file;
    target_output_t output_handle;
    
    symbol_table_t* global_symbols;
    symbol_table_t* current_scope;
    
    int label_counter;
    int string_counter;
    int temp_counter;
    
    /* Z80 register tracking */
    bool reg_a_used;
    bool reg_hl_used;
    bool reg_de_used;
    bool reg_bc_used;
    
    int stack_offset;
} codegen_t;

/* Code generator functions */
codegen_t* codegen_create(const char* output_file, symbol_table_t* symbols);
void codegen_destroy(codegen_t* gen);
cc_error_t codegen_generate(codegen_t* gen, ast_node_t* ast);
cc_error_t codegen_write_output(codegen_t* gen);

/* Helper functions */
void codegen_emit(codegen_t* gen, const char* fmt, ...);
void codegen_emit_comment(codegen_t* gen, const char* fmt, ...);
char* codegen_new_label(codegen_t* gen);
char* codegen_new_string_label(codegen_t* gen);
void codegen_emit_prologue(codegen_t* gen, const char* func_name);
void codegen_emit_epilogue(codegen_t* gen);

#endif /* CODEGEN_H */

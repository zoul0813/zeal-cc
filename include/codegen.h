#ifndef CODEGEN_H
#define CODEGEN_H

#include "common.h"
#include "ast_reader.h"
#include "symbol.h"
#include "target.h"

/* Keep codegen counters consistent across host/ZOS to expose limits early. */
typedef uint8_t codegen_local_count_t;
typedef uint8_t codegen_param_count_t;
typedef uint8_t codegen_function_count_t;
typedef uint8_t codegen_global_count_t;
typedef uint8_t codegen_string_count_t;

/* Code generator structure */
typedef struct {
    output_t output_handle;
    
    uint16_t label_counter;

    int16_t stack_offset;
    const char* local_vars[64];
    int16_t local_offsets[64];
    bool local_is_16[64];
    bool local_is_pointer[64];
    bool local_is_array[64];
    uint8_t local_elem_size[64];
    codegen_local_count_t local_var_count;
    const char* param_names[8];
    int16_t param_offsets[8];
    bool param_is_16[8];
    bool param_is_pointer[8];
    uint8_t param_elem_size[8];
    codegen_param_count_t param_count;
    char* function_end_label;
    bool return_direct;
    bool use_function_end_label;
    bool function_return_is_16;
    uint16_t function_return_flags[64];
    codegen_function_count_t function_count;

    const char* global_names[64];
    bool global_is_16[64];
    bool global_is_pointer[64];
    bool global_is_array[64];
    uint8_t global_elem_size[64];
    codegen_global_count_t global_count;

    const char* string_labels[64];
    char* string_literals[64];
    codegen_string_count_t string_count;
} codegen_t;

/* Code generator functions */
codegen_t* codegen_create(const char* output_file);
void codegen_destroy(codegen_t* gen);
cc_error_t codegen_generate_stream(codegen_t* gen, ast_reader_t* ast);

/* Helper functions */
void codegen_emit(codegen_t* gen, const char* fmt);
char* codegen_new_label(codegen_t* gen);
char* codegen_new_string_label(codegen_t* gen);
#endif /* CODEGEN_H */

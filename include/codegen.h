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

enum {
    CG_FLAG_IS_16 = 0x01,
    CG_FLAG_IS_SIGNED = 0x02,
    CG_FLAG_IS_POINTER = 0x04,
    CG_FLAG_IS_ARRAY = 0x08,
    CG_FLAG_ELEM_SIGNED = 0x10
};

typedef struct {
    const char* name;
    int16_t offset;
    uint8_t elem_size;
    uint8_t flags;
} codegen_local_t;

typedef struct {
    const char* name;
    int16_t offset;
    uint8_t elem_size;
    uint8_t flags;
} codegen_param_t;

typedef struct {
    const char* name;
    uint8_t elem_size;
    uint8_t flags;
} codegen_global_t;

/* Code generator structure */
typedef struct {
    output_t output_handle;
    
    uint16_t label_counter;

    const char* current_function_name;
    int16_t stack_offset;
    codegen_local_t locals[64];
    codegen_local_count_t local_var_count;
    codegen_param_t params[8];
    codegen_param_count_t param_count;
    char* function_end_label;
    bool function_return_is_16;
    uint16_t function_return_flags[64];
    codegen_function_count_t function_count;

    codegen_global_t globals[64];
    codegen_global_count_t global_count;

    const char* string_labels[64];
    char* string_literals[64];
    codegen_string_count_t string_count;

    char* loop_break_labels[8];
    char* loop_continue_labels[8];
    uint8_t loop_depth;
} codegen_t;

/* Code generator functions */
codegen_t* codegen_create(const char* output_file);
void codegen_destroy(codegen_t* gen);
cc_error_t codegen_generate_stream(void);

/* Helper functions */
void codegen_emit(const char* fmt);
char* codegen_new_label(void);
char* codegen_new_string_label(void);
#endif /* CODEGEN_H */

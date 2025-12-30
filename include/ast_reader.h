#ifndef AST_READER_H
#define AST_READER_H

#include <stdint.h>

#include "parser.h"
#include "target.h"

typedef struct {
    reader_t* reader;
    uint16_t node_count;
    uint16_t string_count;
    uint32_t string_table_offset;
    uint8_t format_version;
    char** strings;
    uint16_t decl_count;
    uint16_t decl_index;
    uint8_t program_started;
} ast_reader_t;

int8_t ast_reader_init(ast_reader_t* ast, reader_t* reader);
int8_t ast_reader_load_strings(ast_reader_t* ast);
const char* ast_reader_string(ast_reader_t* ast, uint16_t index);
int8_t ast_reader_read_type_info(ast_reader_t* ast, uint8_t* base, uint8_t* depth,
                                 uint16_t* array_len);
int8_t ast_reader_begin_program(ast_reader_t* ast, uint16_t* decl_count);
int8_t ast_reader_skip_node(ast_reader_t* ast);
int8_t ast_reader_skip_tag(ast_reader_t* ast, uint8_t tag);
void ast_reader_destroy(ast_reader_t* ast);

#endif /* AST_READER_H */

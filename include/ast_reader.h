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
    char** strings;
} ast_reader_t;

int8_t ast_reader_init(ast_reader_t* ast, reader_t* reader);
int8_t ast_reader_load_strings(ast_reader_t* ast);
ast_node_t* ast_reader_read_root(ast_reader_t* ast);
void ast_reader_destroy(ast_reader_t* ast);
void ast_tree_destroy(ast_node_t* node);

#endif /* AST_READER_H */

#include "ast_reader.h"

#include "ast_format.h"
#include "ast_io.h"

int8_t ast_reader_skip_tag(ast_reader_t* ast, uint8_t tag) {
    if (!ast || !ast->reader) return -1;
    switch (tag) {
        case AST_TAG_PROGRAM: {
            uint16_t decl_count = 0;
            ast_read_u16(ast->reader, &decl_count);
            for (uint16_t i = 0; i < decl_count; i++) {
                if (ast_reader_skip_node(ast) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_FUNCTION: {
            uint16_t name_index = 0;
            uint8_t param_count = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            ast_read_u16(ast->reader, &name_index);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            ast_read_u8(ast->reader, &param_count);
            (void)array_len;
            for (uint8_t i = 0; i < param_count; i++) {
                if (ast_reader_skip_node(ast) < 0) return -1;
            }
            return ast_reader_skip_node(ast);
        }
        case AST_TAG_VAR_DECL: {
            uint16_t name_index = 0;
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            ast_read_u16(ast->reader, &name_index);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            ast_read_u8(ast->reader, &has_init);
            (void)array_len;
            if (has_init) return ast_reader_skip_node(ast);
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = 0;
            ast_read_u16(ast->reader, &stmt_count);
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (ast_reader_skip_node(ast) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = 0;
            ast_read_u8(ast->reader, &has_expr);
            if (has_expr) return ast_reader_skip_node(ast);
            return 0;
        }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = 0;
            ast_read_u8(ast->reader, &has_else);
            if (ast_reader_skip_node(ast) < 0) return -1;
            if (ast_reader_skip_node(ast) < 0) return -1;
            if (has_else) return ast_reader_skip_node(ast);
            return 0;
        }
        case AST_TAG_WHILE_STMT:
            if (ast_reader_skip_node(ast) < 0) return -1;
            return ast_reader_skip_node(ast);
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = 0;
            uint8_t has_cond = 0;
            uint8_t has_inc = 0;
            ast_read_u8(ast->reader, &has_init);
            ast_read_u8(ast->reader, &has_cond);
            ast_read_u8(ast->reader, &has_inc);
            if (has_init && ast_reader_skip_node(ast) < 0) return -1;
            if (has_cond && ast_reader_skip_node(ast) < 0) return -1;
            if (has_inc && ast_reader_skip_node(ast) < 0) return -1;
            return ast_reader_skip_node(ast);
        }
        case AST_TAG_ASSIGN:
            if (ast_reader_skip_node(ast) < 0) return -1;
            return ast_reader_skip_node(ast);
        case AST_TAG_CALL: {
            uint16_t name_index = 0;
            uint8_t arg_count = 0;
            ast_read_u16(ast->reader, &name_index);
            ast_read_u8(ast->reader, &arg_count);
            for (uint8_t i = 0; i < arg_count; i++) {
                if (ast_reader_skip_node(ast) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = 0;
            ast_read_u8(ast->reader, &op);
            if (ast_reader_skip_node(ast) < 0) return -1;
            return ast_reader_skip_node(ast);
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = 0;
            ast_read_u8(ast->reader, &op);
            return ast_reader_skip_node(ast);
        }
        case AST_TAG_IDENTIFIER: {
            uint16_t name_index = 0;
            ast_read_u16(ast->reader, &name_index);
            return 0;
        }
        case AST_TAG_CONSTANT: {
            int16_t value = 0;
            ast_read_i16(ast->reader, &value);
            return 0;
        }
        case AST_TAG_STRING_LITERAL: {
            uint16_t value_index = 0;
            ast_read_u16(ast->reader, &value_index);
            return 0;
        }
        case AST_TAG_ARRAY_ACCESS:
            if (ast_reader_skip_node(ast) < 0) return -1;
            return ast_reader_skip_node(ast);
        default:
            return -1;
    }
}

#include "semantic.h"

#include "ast_format.h"
#include "ast_io.h"
#include "target.h"

static const char SEM_ERR_BREAK_OUTSIDE_LOOP[] = "break not within loop\n";
static const char SEM_ERR_CONTINUE_OUTSIDE_LOOP[] = "continue not within loop\n";

static int8_t semantic_check_node(ast_reader_t* ast, uint8_t loop_depth) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;

    tag = ast_read_u8(ast->reader);
    switch (tag) {
        case AST_TAG_FUNCTION: {
            uint8_t param_count = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            ast_read_u16(ast->reader);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            param_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < param_count; i++) {
                if (semantic_check_node(ast, loop_depth) < 0) return -1;
            }
            return semantic_check_node(ast, loop_depth);
        }
        case AST_TAG_VAR_DECL: {
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            ast_read_u16(ast->reader);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            has_init = ast_read_u8(ast->reader);
            if (has_init) {
                return semantic_check_node(ast, loop_depth);
            }
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = ast_read_u16(ast->reader);
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (semantic_check_node(ast, loop_depth) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = ast_read_u8(ast->reader);
            if (has_expr) {
                return semantic_check_node(ast, loop_depth);
            }
            return 0;
        }
        case AST_TAG_BREAK_STMT:
            if (loop_depth == 0) {
                log_error(SEM_ERR_BREAK_OUTSIDE_LOOP);
                return -1;
            }
            return 0;
        case AST_TAG_CONTINUE_STMT:
            if (loop_depth == 0) {
                log_error(SEM_ERR_CONTINUE_OUTSIDE_LOOP);
                return -1;
            }
            return 0;
        case AST_TAG_GOTO_STMT:
        case AST_TAG_LABEL_STMT:
            ast_read_u16(ast->reader);
            return 0;
        case AST_TAG_IF_STMT: {
            uint8_t has_else = ast_read_u8(ast->reader);
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            if (has_else) {
                return semantic_check_node(ast, loop_depth);
            }
            return 0;
        }
        case AST_TAG_WHILE_STMT:
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            return semantic_check_node(ast, (uint8_t)(loop_depth + 1));
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = ast_read_u8(ast->reader);
            uint8_t has_cond = ast_read_u8(ast->reader);
            uint8_t has_inc = ast_read_u8(ast->reader);
            if (has_init && semantic_check_node(ast, loop_depth) < 0) return -1;
            if (has_cond && semantic_check_node(ast, loop_depth) < 0) return -1;
            if (has_inc && semantic_check_node(ast, loop_depth) < 0) return -1;
            return semantic_check_node(ast, (uint8_t)(loop_depth + 1));
        }
        case AST_TAG_ASSIGN:
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            return semantic_check_node(ast, loop_depth);
        case AST_TAG_CALL: {
            uint8_t arg_count = 0;
            ast_read_u16(ast->reader);
            arg_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < arg_count; i++) {
                if (semantic_check_node(ast, loop_depth) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_BINARY_OP:
            ast_read_u8(ast->reader);
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            return semantic_check_node(ast, loop_depth);
        case AST_TAG_UNARY_OP:
            ast_read_u8(ast->reader);
            return semantic_check_node(ast, loop_depth);
        case AST_TAG_IDENTIFIER:
            ast_read_u16(ast->reader);
            return 0;
        case AST_TAG_CONSTANT:
            ast_read_i16(ast->reader);
            return 0;
        case AST_TAG_STRING_LITERAL:
            ast_read_u16(ast->reader);
            return 0;
        case AST_TAG_ARRAY_ACCESS:
            if (semantic_check_node(ast, loop_depth) < 0) return -1;
            return semantic_check_node(ast, loop_depth);
        default:
            return -1;
    }
}

cc_error_t semantic_validate(ast_reader_t* ast) {
    uint16_t decl_count = 0;
    if (!ast) return CC_ERROR_INVALID_ARG;
    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_SEMANTIC;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        if (semantic_check_node(ast, 0) < 0) {
            return CC_ERROR_SEMANTIC;
        }
    }
    return CC_OK;
}

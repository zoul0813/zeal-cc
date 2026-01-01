#include "semantic.h"

#include "ast_format.h"
#include "ast_io.h"
#include "cc_compat.h"
#include "target.h"

#define SEM_MAX_LABELS 64
#define SEM_MAX_GOTOS 64

static const char SEM_ERR_BREAK_OUTSIDE_LOOP[] = "break not within loop\n";
static const char SEM_ERR_CONTINUE_OUTSIDE_LOOP[] = "continue not within loop\n";
static const char SEM_ERR_LABEL_DUPLICATE[] = "Duplicate label: ";
static const char SEM_ERR_GOTO_UNDEFINED[] = "Undefined label: ";
static const char SEM_ERR_LABEL_OVERFLOW[] = "Too many labels in function\n";
static const char SEM_ERR_GOTO_OVERFLOW[] = "Too many gotos in function\n";
static const char SEM_ERR_LABEL_INVALID[] = "Invalid label\n";

typedef struct {
    const char* labels[SEM_MAX_LABELS];
    uint8_t label_count;
    const char* gotos[SEM_MAX_GOTOS];
    uint8_t goto_count;
} semantic_ctx_t;

static int8_t semantic_add_label(semantic_ctx_t* ctx, const char* label) {
    if (!ctx || !label || !*label) {
        log_error(SEM_ERR_LABEL_INVALID);
        return -1;
    }
    for (uint8_t i = 0; i < ctx->label_count; i++) {
        if (str_cmp(ctx->labels[i], label) == 0) {
            log_error(SEM_ERR_LABEL_DUPLICATE);
            log_error(label);
            log_error("\n");
            return -1;
        }
    }
    if (ctx->label_count >= SEM_MAX_LABELS) {
        log_error(SEM_ERR_LABEL_OVERFLOW);
        return -1;
    }
    ctx->labels[ctx->label_count++] = label;
    return 0;
}

static int8_t semantic_add_goto(semantic_ctx_t* ctx, const char* label) {
    if (!ctx || !label || !*label) {
        log_error(SEM_ERR_LABEL_INVALID);
        return -1;
    }
    if (ctx->goto_count >= SEM_MAX_GOTOS) {
        log_error(SEM_ERR_GOTO_OVERFLOW);
        return -1;
    }
    ctx->gotos[ctx->goto_count++] = label;
    return 0;
}

static int8_t semantic_check_gotos(const semantic_ctx_t* ctx) {
    if (!ctx) return -1;
    for (uint8_t i = 0; i < ctx->goto_count; i++) {
        const char* label = ctx->gotos[i];
        uint8_t found = 0;
        for (uint8_t j = 0; j < ctx->label_count; j++) {
            if (str_cmp(ctx->labels[j], label) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            log_error(SEM_ERR_GOTO_UNDEFINED);
            log_error(label);
            log_error("\n");
            return -1;
        }
    }
    return 0;
}

static int8_t semantic_check_node(ast_reader_t* ast, uint8_t loop_depth, semantic_ctx_t* ctx) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;

    tag = ast_read_u8(ast->reader);
    switch (tag) {
        case AST_TAG_FUNCTION: {
            uint8_t param_count = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            semantic_ctx_t local_ctx;
            mem_set(&local_ctx, 0, sizeof(local_ctx));
            ast_read_u16(ast->reader);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            param_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < param_count; i++) {
                if (semantic_check_node(ast, loop_depth, &local_ctx) < 0) return -1;
            }
            if (semantic_check_node(ast, loop_depth, &local_ctx) < 0) return -1;
            return semantic_check_gotos(&local_ctx);
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
                return semantic_check_node(ast, loop_depth, ctx);
            }
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = ast_read_u16(ast->reader);
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = ast_read_u8(ast->reader);
            if (has_expr) {
                return semantic_check_node(ast, loop_depth, ctx);
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
            if (!ctx) return -1;
            {
                uint16_t name_index = ast_read_u16(ast->reader);
                const char* label = ast_reader_string(ast, name_index);
                return semantic_add_goto(ctx, label);
            }
        case AST_TAG_LABEL_STMT:
            if (!ctx) return -1;
            {
                uint16_t name_index = ast_read_u16(ast->reader);
                const char* label = ast_reader_string(ast, name_index);
                return semantic_add_label(ctx, label);
            }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = ast_read_u8(ast->reader);
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            if (has_else) {
                return semantic_check_node(ast, loop_depth, ctx);
            }
            return 0;
        }
        case AST_TAG_WHILE_STMT:
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            return semantic_check_node(ast, (uint8_t)(loop_depth + 1), ctx);
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = ast_read_u8(ast->reader);
            uint8_t has_cond = ast_read_u8(ast->reader);
            uint8_t has_inc = ast_read_u8(ast->reader);
            if (has_init && semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            if (has_cond && semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            if (has_inc && semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            return semantic_check_node(ast, (uint8_t)(loop_depth + 1), ctx);
        }
        case AST_TAG_ASSIGN:
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            return semantic_check_node(ast, loop_depth, ctx);
        case AST_TAG_CALL: {
            uint8_t arg_count = 0;
            ast_read_u16(ast->reader);
            arg_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < arg_count; i++) {
                if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_BINARY_OP:
            ast_read_u8(ast->reader);
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            return semantic_check_node(ast, loop_depth, ctx);
        case AST_TAG_UNARY_OP:
            ast_read_u8(ast->reader);
            return semantic_check_node(ast, loop_depth, ctx);
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
            if (semantic_check_node(ast, loop_depth, ctx) < 0) return -1;
            return semantic_check_node(ast, loop_depth, ctx);
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
        if (semantic_check_node(ast, 0, NULL) < 0) {
            return CC_ERROR_SEMANTIC;
        }
    }
    return CC_OK;
}

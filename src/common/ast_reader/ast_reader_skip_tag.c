#include "ast_reader.h"
#include "cc_compat.h"
#include "ast_format.h"
#include "ast_io.h"

typedef int8_t (*ast_skip_fn)(void);

static int8_t ast_skip_u8(void) {
    (void)ast_read_u8();
    return 0;
}

static int8_t ast_skip_u16(void) {
    (void)ast_read_u16();
    return 0;
}

static int8_t ast_skip_i16(void) {
    (void)ast_read_i16();
    return 0;
}

static int8_t ast_skip_type_info(void) {
    uint8_t base = 0;
    uint8_t depth = 0;
    uint16_t array_len = 0;
    return ast_reader_read_type_info(&base, &depth, &array_len);
}

static int8_t ast_skip_nodes(uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        if (ast_reader_skip_node() < 0) return -1;
    }
    return 0;
}

static int8_t ast_skip_optional(uint8_t has_node) {
    if (!has_node) return 0;
    return ast_reader_skip_node();
}

static int8_t ast_skip_two_nodes(void) {
    if (ast_reader_skip_node() < 0) return -1;
    return ast_reader_skip_node();
}

static int8_t ast_skip_program(void) {
    uint16_t decl_count = 0;
    decl_count = ast_read_u16();
    return ast_skip_nodes(decl_count);
}

static int8_t ast_skip_function(void) {
    uint8_t param_count = 0;
    if (ast_skip_u16() < 0) return -1;
    if (ast_skip_type_info() < 0) return -1;
    param_count = ast_read_u8();
    if (ast_skip_nodes(param_count) < 0) return -1;
    return ast_reader_skip_node();
}

static int8_t ast_skip_var_decl(void) {
    uint8_t has_init = 0;
    if (ast_skip_u16() < 0) return -1;
    if (ast_skip_type_info() < 0) return -1;
    has_init = ast_read_u8();
    return ast_skip_optional(has_init);
}

static int8_t ast_skip_compound(void) {
    uint16_t stmt_count = 0;
    stmt_count = ast_read_u16();
    return ast_skip_nodes(stmt_count);
}

static int8_t ast_skip_return(void) {
    uint8_t has_expr = 0;
    has_expr = ast_read_u8();
    return ast_skip_optional(has_expr);
}

static int8_t ast_skip_if(void) {
    uint8_t has_else = 0;
    has_else = ast_read_u8();
    if (ast_skip_two_nodes() < 0) return -1;
    return ast_skip_optional(has_else);
}

static int8_t ast_skip_for(void) {
    uint8_t has_init = 0;
    uint8_t has_cond = 0;
    uint8_t has_inc = 0;
    has_init = ast_read_u8();
    has_cond = ast_read_u8();
    has_inc = ast_read_u8();
    if (ast_skip_optional(has_init) < 0) return -1;
    if (ast_skip_optional(has_cond) < 0) return -1;
    if (ast_skip_optional(has_inc) < 0) return -1;
    return ast_reader_skip_node();
}

static int8_t ast_skip_call(void) {
    uint8_t arg_count = 0;
    if (ast_skip_u16() < 0) return -1;
    arg_count = ast_read_u8();
    return ast_skip_nodes(arg_count);
}

static int8_t ast_skip_binary(void) {
    if (ast_skip_u8() < 0) return -1;
    return ast_skip_two_nodes();
}

static int8_t ast_skip_unary(void) {
    if (ast_skip_u8() < 0) return -1;
    return ast_reader_skip_node();
}

#define AST_TAG_COUNT (AST_TAG_ARRAY_ACCESS + 1)

static const ast_skip_fn g_ast_skip_handlers[AST_TAG_COUNT] = {
    NULL,                /* 0 */
    ast_skip_program,    /* AST_TAG_PROGRAM */
    ast_skip_function,   /* AST_TAG_FUNCTION */
    ast_skip_var_decl,   /* AST_TAG_VAR_DECL */
    ast_skip_compound,   /* AST_TAG_COMPOUND_STMT */
    ast_skip_return,     /* AST_TAG_RETURN_STMT */
    NULL,                /* AST_TAG_BREAK_STMT */
    NULL,                /* AST_TAG_CONTINUE_STMT */
    ast_skip_u16,        /* AST_TAG_GOTO_STMT */
    ast_skip_u16,        /* AST_TAG_LABEL_STMT */
    ast_skip_if,         /* AST_TAG_IF_STMT */
    ast_skip_two_nodes,  /* AST_TAG_WHILE_STMT */
    ast_skip_for,        /* AST_TAG_FOR_STMT */
    ast_skip_two_nodes,  /* AST_TAG_ASSIGN */
    ast_skip_call,       /* AST_TAG_CALL */
    ast_skip_binary,     /* AST_TAG_BINARY_OP */
    ast_skip_unary,      /* AST_TAG_UNARY_OP */
    ast_skip_u16,        /* AST_TAG_IDENTIFIER */
    ast_skip_i16,        /* AST_TAG_CONSTANT */
    ast_skip_u16,        /* AST_TAG_STRING_LITERAL */
    ast_skip_two_nodes   /* AST_TAG_ARRAY_ACCESS */
};

int8_t ast_reader_skip_tag(uint8_t tag) {
    if (!ast) return -1;
    if (tag >= AST_TAG_COUNT) return -1;
    ast_skip_fn fn = g_ast_skip_handlers[tag];
    if (!fn) return 0;
    return fn();
}

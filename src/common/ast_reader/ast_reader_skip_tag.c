#include "ast_reader.h"

#include "ast_format.h"
#include "ast_io.h"

typedef int8_t (*ast_skip_fn)(ast_reader_t* ast);

static int8_t ast_skip_u8(ast_reader_t* ast) {
    (void)ast_read_u8(ast->reader);
    return 0;
}

static int8_t ast_skip_u16(ast_reader_t* ast) {
    (void)ast_read_u16(ast->reader);
    return 0;
}

static int8_t ast_skip_i16(ast_reader_t* ast) {
    (void)ast_read_i16(ast->reader);
    return 0;
}

static int8_t ast_skip_type_info(ast_reader_t* ast) {
    uint8_t base = 0;
    uint8_t depth = 0;
    uint16_t array_len = 0;
    return ast_reader_read_type_info(ast, &base, &depth, &array_len);
}

static int8_t ast_skip_nodes(ast_reader_t* ast, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        if (ast_reader_skip_node(ast) < 0) return -1;
    }
    return 0;
}

static int8_t ast_skip_optional(ast_reader_t* ast, uint8_t has_node) {
    if (!has_node) return 0;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_program(ast_reader_t* ast) {
    uint16_t decl_count = 0;
    decl_count = ast_read_u16(ast->reader);
    return ast_skip_nodes(ast, decl_count);
}

static int8_t ast_skip_function(ast_reader_t* ast) {
    uint8_t param_count = 0;
    if (ast_skip_u16(ast) < 0) return -1;
    if (ast_skip_type_info(ast) < 0) return -1;
    param_count = ast_read_u8(ast->reader);
    if (ast_skip_nodes(ast, param_count) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_var_decl(ast_reader_t* ast) {
    uint8_t has_init = 0;
    if (ast_skip_u16(ast) < 0) return -1;
    if (ast_skip_type_info(ast) < 0) return -1;
    has_init = ast_read_u8(ast->reader);
    return ast_skip_optional(ast, has_init);
}

static int8_t ast_skip_compound(ast_reader_t* ast) {
    uint16_t stmt_count = 0;
    stmt_count = ast_read_u16(ast->reader);
    return ast_skip_nodes(ast, stmt_count);
}

static int8_t ast_skip_return(ast_reader_t* ast) {
    uint8_t has_expr = 0;
    has_expr = ast_read_u8(ast->reader);
    return ast_skip_optional(ast, has_expr);
}

static int8_t ast_skip_if(ast_reader_t* ast) {
    uint8_t has_else = 0;
    has_else = ast_read_u8(ast->reader);
    if (ast_reader_skip_node(ast) < 0) return -1;
    if (ast_reader_skip_node(ast) < 0) return -1;
    return ast_skip_optional(ast, has_else);
}

static int8_t ast_skip_while(ast_reader_t* ast) {
    if (ast_reader_skip_node(ast) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_for(ast_reader_t* ast) {
    uint8_t has_init = 0;
    uint8_t has_cond = 0;
    uint8_t has_inc = 0;
    has_init = ast_read_u8(ast->reader);
    has_cond = ast_read_u8(ast->reader);
    has_inc = ast_read_u8(ast->reader);
    if (ast_skip_optional(ast, has_init) < 0) return -1;
    if (ast_skip_optional(ast, has_cond) < 0) return -1;
    if (ast_skip_optional(ast, has_inc) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_assign(ast_reader_t* ast) {
    if (ast_reader_skip_node(ast) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_call(ast_reader_t* ast) {
    uint8_t arg_count = 0;
    if (ast_skip_u16(ast) < 0) return -1;
    arg_count = ast_read_u8(ast->reader);
    return ast_skip_nodes(ast, arg_count);
}

static int8_t ast_skip_binary(ast_reader_t* ast) {
    if (ast_skip_u8(ast) < 0) return -1;
    if (ast_reader_skip_node(ast) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_unary(ast_reader_t* ast) {
    if (ast_skip_u8(ast) < 0) return -1;
    return ast_reader_skip_node(ast);
}

static int8_t ast_skip_identifier(ast_reader_t* ast) {
    return ast_skip_u16(ast);
}

static int8_t ast_skip_constant(ast_reader_t* ast) {
    return ast_skip_i16(ast);
}

static int8_t ast_skip_string(ast_reader_t* ast) {
    return ast_skip_u16(ast);
}

static int8_t ast_skip_array_access(ast_reader_t* ast) {
    if (ast_reader_skip_node(ast) < 0) return -1;
    return ast_reader_skip_node(ast);
}

#define AST_TAG_COUNT (AST_TAG_ARRAY_ACCESS + 1)

static const ast_skip_fn g_ast_skip_handlers[AST_TAG_COUNT] = {
    NULL,                /* 0 */
    ast_skip_program,    /* AST_TAG_PROGRAM */
    ast_skip_function,   /* AST_TAG_FUNCTION */
    ast_skip_var_decl,   /* AST_TAG_VAR_DECL */
    ast_skip_compound,   /* AST_TAG_COMPOUND_STMT */
    ast_skip_return,     /* AST_TAG_RETURN_STMT */
    ast_skip_if,         /* AST_TAG_IF_STMT */
    ast_skip_while,      /* AST_TAG_WHILE_STMT */
    ast_skip_for,        /* AST_TAG_FOR_STMT */
    ast_skip_assign,     /* AST_TAG_ASSIGN */
    ast_skip_call,       /* AST_TAG_CALL */
    ast_skip_binary,     /* AST_TAG_BINARY_OP */
    ast_skip_unary,      /* AST_TAG_UNARY_OP */
    ast_skip_identifier, /* AST_TAG_IDENTIFIER */
    ast_skip_constant,   /* AST_TAG_CONSTANT */
    ast_skip_string,     /* AST_TAG_STRING_LITERAL */
    ast_skip_array_access /* AST_TAG_ARRAY_ACCESS */
};

int8_t ast_reader_skip_tag(ast_reader_t* ast, uint8_t tag) {
    if (!ast || !ast->reader) return -1;
    if (tag >= AST_TAG_COUNT) return -1;
    ast_skip_fn fn = g_ast_skip_handlers[tag];
    if (!fn) return -1;
    return fn(ast);
}

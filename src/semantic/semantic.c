#include "semantic.h"

#include "ast_format.h"
#include "ast_io.h"
#include "cc_compat.h"
#include "target.h"

#define SEM_MAX_LABELS 64
#define SEM_MAX_GOTOS 64
#define SEM_MAX_SCOPES 8
#define SEM_MAX_SYMBOLS 32

static const char SEM_ERR_BREAK_OUTSIDE_LOOP[] = "break not within loop\n";
static const char SEM_ERR_CONTINUE_OUTSIDE_LOOP[] = "continue not within loop\n";
static const char SEM_ERR_LABEL_DUPLICATE[] = "Duplicate label: ";
static const char SEM_ERR_GOTO_UNDEFINED[] = "Undefined label: ";
static const char SEM_ERR_LABEL_OVERFLOW[] = "Too many labels in function\n";
static const char SEM_ERR_GOTO_OVERFLOW[] = "Too many gotos in function\n";
static const char SEM_ERR_LABEL_INVALID[] = "Invalid label\n";
static const char SEM_ERR_IDENT_DUPLICATE[] = "Duplicate identifier: ";
static const char SEM_ERR_IDENT_UNDEFINED[] = "Undefined identifier: ";
static const char SEM_ERR_FUNC_UNDEFINED[] = "Undefined function: ";
static const char SEM_ERR_SCOPE_OVERFLOW[] = "Too many scopes\n";
static const char SEM_ERR_SYMBOL_OVERFLOW[] = "Too many symbols in scope\n";
static const char SEM_ERR_EXPECT_LVALUE[] = "Expected lvalue\n";
static const char SEM_ERR_RETURN_VALUE_VOID[] = "Return value in void function\n";
static const char SEM_ERR_RETURN_MISSING_VALUE[] = "Missing return value\n";
static const char SEM_ERR_GOTO_SCOPE_JUMP[] = "Goto jumps into deeper scope\n";

typedef struct {
    const char* labels[SEM_MAX_LABELS];
    uint8_t label_count;
    const char* gotos[SEM_MAX_GOTOS];
    uint8_t goto_count;
    uint8_t label_scopes[SEM_MAX_LABELS];
    uint8_t goto_scopes[SEM_MAX_GOTOS];
} semantic_ctx_t;

typedef enum {
    SEM_SYMBOL_VAR = 0,
    SEM_SYMBOL_FUNC = 1
} semantic_symbol_kind_t;

typedef struct {
    const char* name;
    semantic_symbol_kind_t kind;
} semantic_symbol_t;

typedef struct {
    semantic_symbol_t symbols[SEM_MAX_SYMBOLS];
    uint8_t count;
} semantic_scope_t;

typedef struct {
    semantic_scope_t scopes[SEM_MAX_SCOPES];
    uint8_t scope_depth;
    uint8_t in_function;
    semantic_ctx_t* label_ctx;
    uint8_t return_base;
    uint8_t return_is_void;
} semantic_state_t;

static semantic_state_t g_semantic_state;

static const char* builtin_funcs[] = {
    "putchar",
    "fflush_stdout",
    "open",
    "read",
    "close",
    "exit",
    NULL
};

static uint8_t semantic_is_builtin_function(const char* name) {
    if (!name || !*name) return 0;
    for (uint8_t i = 0; builtin_funcs[i]; i++) {
        if (str_cmp(builtin_funcs[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int8_t semantic_scope_push(semantic_state_t* state) {
    if (!state || state->scope_depth >= SEM_MAX_SCOPES) {
        log_error(SEM_ERR_SCOPE_OVERFLOW);
        return -1;
    }
    mem_set(&state->scopes[state->scope_depth], 0, sizeof(state->scopes[state->scope_depth]));
    state->scope_depth++;
    return 0;
}

static void semantic_scope_pop(semantic_state_t* state) {
    if (!state || state->scope_depth == 0) return;
    state->scope_depth--;
}

static int8_t semantic_scope_add(semantic_state_t* state, const char* name,
                                 semantic_symbol_kind_t kind) {
    if (!state || !name || !*name || state->scope_depth == 0) {
        log_error(SEM_ERR_LABEL_INVALID);
        return -1;
    }
    semantic_scope_t* scope = &state->scopes[state->scope_depth - 1];
    for (uint8_t i = 0; i < scope->count; i++) {
        if (str_cmp(scope->symbols[i].name, name) == 0) {
            log_error(SEM_ERR_IDENT_DUPLICATE);
            log_error(name);
            log_error("\n");
            return -1;
        }
    }
    if (scope->count >= SEM_MAX_SYMBOLS) {
        log_error(SEM_ERR_SYMBOL_OVERFLOW);
        return -1;
    }
    scope->symbols[scope->count].name = name;
    scope->symbols[scope->count].kind = kind;
    scope->count++;
    return 0;
}

static const semantic_symbol_t* semantic_scope_lookup(const semantic_state_t* state,
                                                      const char* name) {
    if (!state || !name || !*name) return NULL;
    for (int8_t depth = (int8_t)state->scope_depth - 1; depth >= 0; depth--) {
        const semantic_scope_t* scope = &state->scopes[depth];
        for (uint8_t i = 0; i < scope->count; i++) {
            if (str_cmp(scope->symbols[i].name, name) == 0) {
                return &scope->symbols[i];
            }
        }
    }
    return NULL;
}

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

static int8_t semantic_add_label_scoped(semantic_ctx_t* ctx, const char* label, uint8_t scope_depth) {
    uint8_t index = ctx->label_count;
    if (semantic_add_label(ctx, label) < 0) return -1;
    ctx->label_scopes[index] = scope_depth;
    return 0;
}

static int8_t semantic_add_goto_scoped(semantic_ctx_t* ctx, const char* label, uint8_t scope_depth) {
    uint8_t index = ctx->goto_count;
    if (semantic_add_goto(ctx, label) < 0) return -1;
    ctx->goto_scopes[index] = scope_depth;
    return 0;
}

static int8_t semantic_check_gotos(const semantic_ctx_t* ctx) {
    if (!ctx) return -1;
    for (uint8_t i = 0; i < ctx->goto_count; i++) {
        const char* label = ctx->gotos[i];
        uint8_t found = 0;
        uint8_t label_depth = 0;
        for (uint8_t j = 0; j < ctx->label_count; j++) {
            if (str_cmp(ctx->labels[j], label) == 0) {
                found = 1;
                label_depth = ctx->label_scopes[j];
                break;
            }
        }
        if (!found) {
            log_error(SEM_ERR_GOTO_UNDEFINED);
            log_error(label);
            log_error("\n");
            return -1;
        }
        if (ctx->goto_scopes[i] < label_depth) {
            log_error(SEM_ERR_GOTO_SCOPE_JUMP);
            return -1;
        }
    }
    return 0;
}

static int8_t semantic_check_node_with_lvalue(
    ast_reader_t* ast,
    uint8_t loop_depth,
    semantic_state_t* state,
    uint8_t* out_lvalue
) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;
    if (out_lvalue) *out_lvalue = 0;

    tag = ast_read_u8(ast->reader);
    switch (tag) {
        case AST_TAG_FUNCTION: {
            uint8_t param_count = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            semantic_ctx_t local_ctx;
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            uint8_t prev_in_function = state ? state->in_function : 0;
            uint8_t prev_return_base = state ? state->return_base : 0;
            uint8_t prev_return_is_void = state ? state->return_is_void : 0;
            semantic_ctx_t* prev_label_ctx = state ? state->label_ctx : NULL;
            mem_set(&local_ctx, 0, sizeof(local_ctx));
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            param_count = ast_read_u8(ast->reader);
            if (state && semantic_scope_lookup(state, name) == NULL) {
                if (semantic_scope_add(state, name, SEM_SYMBOL_FUNC) < 0) return -1;
            }
            if (state) {
                if (semantic_scope_push(state) < 0) return -1;
                state->in_function = 1;
                state->label_ctx = &local_ctx;
                state->return_base = base;
                state->return_is_void = ((base & AST_BASE_MASK) == AST_BASE_VOID);
            }
            for (uint8_t i = 0; i < param_count; i++) {
                if (semantic_check_node_with_lvalue(ast, 0, state, NULL) < 0) return -1;
            }
            if (semantic_check_node_with_lvalue(ast, 0, state, NULL) < 0) return -1;
            if (state) {
                semantic_scope_pop(state);
                state->in_function = prev_in_function;
                state->label_ctx = prev_label_ctx;
                state->return_base = prev_return_base;
                state->return_is_void = prev_return_is_void;
            }
            return semantic_check_gotos(&local_ctx);
        }
        case AST_TAG_VAR_DECL: {
            uint8_t has_init = 0;
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            if (state && state->in_function) {
                if (semantic_scope_add(state, name, SEM_SYMBOL_VAR) < 0) return -1;
            }
            has_init = ast_read_u8(ast->reader);
            if (has_init) {
                return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
            }
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = ast_read_u16(ast->reader);
            if (state && semantic_scope_push(state) < 0) return -1;
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) {
                    if (state) semantic_scope_pop(state);
                    return -1;
                }
            }
            if (state) semantic_scope_pop(state);
            return 0;
        }
        case AST_TAG_RETURN_STMT: {
            uint8_t has_expr = ast_read_u8(ast->reader);
            if (state && state->return_is_void && has_expr) {
                log_error(SEM_ERR_RETURN_VALUE_VOID);
                return -1;
            }
            if (state && !state->return_is_void && !has_expr) {
                log_error(SEM_ERR_RETURN_MISSING_VALUE);
                return -1;
            }
            if (has_expr) {
                return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
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
            if (!state || !state->label_ctx) return -1;
            {
                uint16_t name_index = ast_read_u16(ast->reader);
                const char* label = ast_reader_string(ast, name_index);
                uint8_t depth = state ? state->scope_depth : 0;
                return semantic_add_goto_scoped(state->label_ctx, label, depth);
            }
        case AST_TAG_LABEL_STMT:
            if (!state || !state->label_ctx) return -1;
            {
                uint16_t name_index = ast_read_u16(ast->reader);
                const char* label = ast_reader_string(ast, name_index);
                uint8_t depth = state ? state->scope_depth : 0;
                return semantic_add_label_scoped(state->label_ctx, label, depth);
            }
        case AST_TAG_IF_STMT: {
            uint8_t has_else = ast_read_u8(ast->reader);
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (has_else) {
                return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
            }
            return 0;
        }
        case AST_TAG_WHILE_STMT:
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            return semantic_check_node_with_lvalue(ast, (uint8_t)(loop_depth + 1), state, NULL);
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = ast_read_u8(ast->reader);
            uint8_t has_cond = ast_read_u8(ast->reader);
            uint8_t has_inc = ast_read_u8(ast->reader);
            if (has_init && semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (has_cond && semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (has_inc && semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            return semantic_check_node_with_lvalue(ast, (uint8_t)(loop_depth + 1), state, NULL);
        }
        case AST_TAG_ASSIGN: {
            uint8_t is_lvalue = 0;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, &is_lvalue) < 0) return -1;
            if (!is_lvalue) {
                log_error(SEM_ERR_EXPECT_LVALUE);
                return -1;
            }
            return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
        }
        case AST_TAG_CALL: {
            uint8_t arg_count = 0;
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (state) {
                const semantic_symbol_t* sym = semantic_scope_lookup(state, name);
                if (!sym) {
                    if (!semantic_is_builtin_function(name)) {
                        log_error(SEM_ERR_FUNC_UNDEFINED);
                        log_error(name);
                        log_error("\n");
                        return -1;
                    }
                } else if (sym->kind != SEM_SYMBOL_FUNC) {
                    log_error(SEM_ERR_FUNC_UNDEFINED);
                    log_error(name);
                    log_error("\n");
                    return -1;
                }
            }
            arg_count = ast_read_u8(ast->reader);
            for (uint8_t i = 0; i < arg_count; i++) {
                if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            }
            return 0;
        }
        case AST_TAG_BINARY_OP:
            ast_read_u8(ast->reader);
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
        case AST_TAG_UNARY_OP: {
            uint8_t op = ast_read_u8(ast->reader);
            uint8_t is_lvalue = 0;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, &is_lvalue) < 0) return -1;
            if (op == OP_PREINC || op == OP_PREDEC || op == OP_POSTINC || op == OP_POSTDEC) {
                if (!is_lvalue) {
                    log_error(SEM_ERR_EXPECT_LVALUE);
                    return -1;
                }
                return 0;
            }
            if (out_lvalue && op == OP_DEREF) {
                *out_lvalue = 1;
            }
            return 0;
        }
        case AST_TAG_IDENTIFIER:
        {
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (state) {
                const semantic_symbol_t* sym = semantic_scope_lookup(state, name);
                if (!sym) {
                    log_error(SEM_ERR_IDENT_UNDEFINED);
                    log_error(name);
                    log_error("\n");
                    return -1;
                }
            }
            if (out_lvalue) *out_lvalue = 1;
            return 0;
        }
        case AST_TAG_CONSTANT:
            ast_read_i16(ast->reader);
            return 0;
        case AST_TAG_STRING_LITERAL:
            ast_read_u16(ast->reader);
            return 0;
        case AST_TAG_ARRAY_ACCESS:
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL) < 0) return -1;
            if (out_lvalue) *out_lvalue = 1;
            return 0;
        default:
            return -1;
    }
}

static int8_t semantic_check_node(ast_reader_t* ast, uint8_t loop_depth, semantic_state_t* state) {
    return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL);
}

cc_error_t semantic_validate(ast_reader_t* ast) {
    uint16_t decl_count = 0;
    if (!ast) return CC_ERROR_INVALID_ARG;
    mem_set(&g_semantic_state, 0, sizeof(g_semantic_state));
    if (semantic_scope_push(&g_semantic_state) < 0) return CC_ERROR_SEMANTIC;

    if (ast_reader_begin_program(ast, &decl_count) < 0) return CC_ERROR_SEMANTIC;
    for (uint16_t i = 0; i < decl_count; i++) {
        uint8_t tag = ast_read_u8(ast->reader);
        if (tag == AST_TAG_FUNCTION) {
            uint16_t name_index = ast_read_u16(ast->reader);
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            uint8_t param_count = 0;
            const char* name = ast_reader_string(ast, name_index);
            if (semantic_scope_add(&g_semantic_state, name, SEM_SYMBOL_FUNC) < 0) return CC_ERROR_SEMANTIC;
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_SEMANTIC;
            param_count = ast_read_u8(ast->reader);
            for (uint8_t p = 0; p < param_count; p++) {
                if (ast_reader_skip_node(ast) < 0) return CC_ERROR_SEMANTIC;
            }
            if (ast_reader_skip_node(ast) < 0) return CC_ERROR_SEMANTIC;
        } else if (tag == AST_TAG_VAR_DECL) {
            uint16_t name_index = ast_read_u16(ast->reader);
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            uint8_t has_init = 0;
            const char* name = ast_reader_string(ast, name_index);
            if (semantic_scope_add(&g_semantic_state, name, SEM_SYMBOL_VAR) < 0) return CC_ERROR_SEMANTIC;
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_SEMANTIC;
            has_init = ast_read_u8(ast->reader);
            if (has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_SEMANTIC;
        } else {
            if (ast_reader_skip_tag(ast, tag) < 0) return CC_ERROR_SEMANTIC;
        }
    }

    if (ast_reader_begin_program(ast, &decl_count) < 0) return CC_ERROR_SEMANTIC;
    for (uint16_t i = 0; i < decl_count; i++) {
        if (semantic_check_node(ast, 0, &g_semantic_state) < 0) return CC_ERROR_SEMANTIC;
    }
    return CC_OK;
}

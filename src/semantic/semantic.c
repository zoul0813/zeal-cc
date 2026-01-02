#include "semantic.h"

#include "ast_format.h"
#include "ast_io.h"
#include "cc_compat.h"
#include "target.h"

#define SEM_MAX_LABELS 64
#define SEM_MAX_GOTOS 64
#define SEM_MAX_SCOPES 8
#define SEM_MAX_SYMBOLS 32
#define SEM_MAX_PARAMS 8

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
static const char SEM_ERR_VAR_INIT_NO_STRING[] = "Array init must be string literal\n";
static const char SEM_ERR_ASSIGN_TO_ARRAY[] = "Cannot assign to array\n";
static const char SEM_ERR_TYPE_MISMATCH[] = "Type mismatch\n";
static const char SEM_ERR_INVALID_CONDITION[] = "Condition must be scalar\n";
static const char SEM_ERR_INVALID_ARRAY_BASE[] =
    "Array access base must be identifier or string literal\n";
static const char SEM_ERR_INVALID_ARRAY_INDEX[] = "Array index must be integer\n";
static const char SEM_ERR_INVALID_ADDR[] = "Address-of requires identifier\n";
static const char SEM_ERR_INVALID_DEREF[] = "Dereference requires pointer identifier\n";
static const char SEM_ERR_INVALID_INCDEC[] = "Increment/decrement requires identifier\n";
static const char SEM_ERR_RETURN_TYPE[] = "Return type mismatch\n";
static const char SEM_ERR_CALL_ARG_COUNT[] = "Argument count mismatch\n";
static const char SEM_ERR_CALL_ARG_TYPE[] = "Argument type mismatch\n";
static const char SEM_ERR_VOID_VALUE[] = "Void value not allowed\n";
static const char SEM_ERR_PARAM_OVERFLOW[] = "Too many parameters\n";

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
    uint8_t base;
    uint8_t depth;
    uint16_t array_len;
} semantic_type_t;

typedef struct {
    const char* name;
    semantic_symbol_kind_t kind;
    semantic_type_t type;
    uint8_t param_count;
    semantic_type_t params[SEM_MAX_PARAMS];
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
    semantic_type_t return_type;
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

static void semantic_type_clear(semantic_type_t* type) {
    if (!type) return;
    mem_set(type, 0, sizeof(*type));
}

static semantic_type_t semantic_type_make(uint8_t base, uint8_t depth, uint16_t array_len) {
    semantic_type_t type;
    type.base = base;
    type.depth = depth;
    type.array_len = array_len;
    return type;
}

static uint8_t semantic_type_base_kind(const semantic_type_t* type) {
    if (!type) return 0;
    return (uint8_t)(type->base & AST_BASE_MASK);
}

static uint8_t semantic_type_is_numeric(const semantic_type_t* type) {
    uint8_t base = semantic_type_base_kind(type);
    return type && type->depth == 0 && type->array_len == 0 &&
           (base == AST_BASE_INT || base == AST_BASE_CHAR);
}

static uint8_t semantic_type_is_pointer(const semantic_type_t* type) {
    return type && type->array_len == 0 && type->depth > 0;
}

static uint8_t semantic_type_is_array(const semantic_type_t* type) {
    return type && type->array_len > 0;
}

static uint8_t semantic_type_is_void_scalar(const semantic_type_t* type) {
    return type && type->depth == 0 && type->array_len == 0 &&
           semantic_type_base_kind(type) == AST_BASE_VOID;
}

static uint8_t semantic_type_is_scalar(const semantic_type_t* type) {
    return semantic_type_is_numeric(type) || semantic_type_is_pointer(type);
}

static semantic_type_t semantic_type_decay_array(semantic_type_t type) {
    if (type.array_len > 0) {
        type.array_len = 0;
        type.depth++;
    }
    return type;
}

static semantic_type_t semantic_type_numeric_result(const semantic_type_t* left,
                                                    const semantic_type_t* right) {
    semantic_type_t out;
    out = semantic_type_make(AST_BASE_INT, 0, 0);
    uint8_t left_kind = semantic_type_base_kind(left);
    uint8_t right_kind = semantic_type_base_kind(right);
    uint8_t left_unsigned = left && (left->base & AST_BASE_FLAG_UNSIGNED);
    uint8_t right_unsigned = right && (right->base & AST_BASE_FLAG_UNSIGNED);
    if (left_kind == AST_BASE_CHAR && right_kind == AST_BASE_CHAR) {
        out.base = AST_BASE_CHAR;
    } else {
        out.base = AST_BASE_INT;
    }
    if (left_unsigned || right_unsigned) {
        out.base |= AST_BASE_FLAG_UNSIGNED;
    }
    return out;
}

static uint8_t semantic_type_compatible_pointer(const semantic_type_t* dst,
                                                const semantic_type_t* src) {
    if (!semantic_type_is_pointer(dst) || !semantic_type_is_pointer(src)) return 0;
    uint8_t dst_base = semantic_type_base_kind(dst);
    uint8_t src_base = semantic_type_base_kind(src);
    if (dst_base == AST_BASE_VOID || src_base == AST_BASE_VOID) {
        return dst->depth == src->depth;
    }
    return dst_base == src_base && dst->depth == src->depth;
}

static uint8_t semantic_type_can_convert(const semantic_type_t* dst,
                                         semantic_type_t src,
                                         uint8_t src_const_zero) {
    if (!dst) return 0;
    if (semantic_type_is_array(dst)) return 0;
    if (semantic_type_is_numeric(dst)) {
        return semantic_type_is_numeric(&src);
    }
    if (semantic_type_is_pointer(dst)) {
        if (semantic_type_is_array(&src)) {
            src = semantic_type_decay_array(src);
        }
        if (semantic_type_is_pointer(&src)) {
            return semantic_type_compatible_pointer(dst, &src);
        }
        if (semantic_type_is_numeric(&src) && src_const_zero) {
            return 1;
        }
        return 0;
    }
    return 0;
}

static int8_t semantic_check_node_with_lvalue(
    ast_reader_t* ast,
    uint8_t loop_depth,
    semantic_state_t* state,
    semantic_type_t* out_type,
    uint8_t* out_lvalue,
    uint8_t* out_const_zero
);

static int8_t semantic_check_tag_with_lvalue(
    ast_reader_t* ast,
    uint8_t tag,
    uint8_t loop_depth,
    semantic_state_t* state,
    semantic_type_t* out_type,
    uint8_t* out_lvalue,
    uint8_t* out_const_zero
);

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
                                 semantic_symbol_kind_t kind,
                                 const semantic_type_t* type,
                                 uint8_t param_count,
                                 const semantic_type_t* params) {
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
    if (type) {
        scope->symbols[scope->count].type = *type;
    } else {
        semantic_type_clear(&scope->symbols[scope->count].type);
    }
    scope->symbols[scope->count].param_count = 0;
    if (kind == SEM_SYMBOL_FUNC && param_count > 0) {
        if (param_count > SEM_MAX_PARAMS) {
            log_error(SEM_ERR_PARAM_OVERFLOW);
            return -1;
        }
        scope->symbols[scope->count].param_count = param_count;
        for (uint8_t i = 0; i < param_count; i++) {
            scope->symbols[scope->count].params[i] = params[i];
        }
    }
    scope->count++;
    return 0;
}

static int8_t semantic_scope_add_var(semantic_state_t* state, const char* name,
                                     const semantic_type_t* type) {
    return semantic_scope_add(state, name, SEM_SYMBOL_VAR, type, 0, NULL);
}

static int8_t semantic_scope_add_func(semantic_state_t* state, const char* name,
                                      const semantic_type_t* return_type,
                                      uint8_t param_count,
                                      const semantic_type_t* params) {
    return semantic_scope_add(state, name, SEM_SYMBOL_FUNC, return_type,
                              param_count, params);
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

static int8_t semantic_check_tag_with_lvalue(
    ast_reader_t* ast,
    uint8_t tag,
    uint8_t loop_depth,
    semantic_state_t* state,
    semantic_type_t* out_type,
    uint8_t* out_lvalue,
    uint8_t* out_const_zero
) {
    if (!ast || !ast->reader) return -1;
    if (out_lvalue) *out_lvalue = 0;
    if (out_const_zero) *out_const_zero = 0;
    if (out_type) semantic_type_clear(out_type);

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
            semantic_type_t prev_return_type;
            uint8_t prev_return_is_void = state ? state->return_is_void : 0;
            semantic_ctx_t* prev_label_ctx = state ? state->label_ctx : NULL;
            mem_set(&local_ctx, 0, sizeof(local_ctx));
            if (state) {
                prev_return_type = state->return_type;
            } else {
                semantic_type_clear(&prev_return_type);
            }
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return -1;
            param_count = ast_read_u8(ast->reader);
            if (state && semantic_scope_lookup(state, name) == NULL) {
                semantic_type_t return_type;
                return_type = semantic_type_make(base, depth, array_len);
                if (semantic_scope_add_func(state, name, &return_type, 0, NULL) < 0) return -1;
            }
            if (state) {
                if (semantic_scope_push(state) < 0) return -1;
                state->in_function = 1;
                state->label_ctx = &local_ctx;
                state->return_type = semantic_type_make(base, depth, array_len);
                state->return_is_void = semantic_type_is_void_scalar(&state->return_type);
            }
            for (uint8_t i = 0; i < param_count; i++) {
                if (semantic_check_node_with_lvalue(ast, 0, state, NULL, NULL, NULL) < 0) return -1;
            }
            if (semantic_check_node_with_lvalue(ast, 0, state, NULL, NULL, NULL) < 0) return -1;
            if (state) {
                semantic_scope_pop(state);
                state->in_function = prev_in_function;
                state->label_ctx = prev_label_ctx;
                state->return_type = prev_return_type;
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
            semantic_type_t var_type;
            var_type = semantic_type_make(base, depth, array_len);
            if (state && state->in_function) {
                if (semantic_scope_add_var(state, name, &var_type) < 0) return -1;
            }
            has_init = ast_read_u8(ast->reader);
            if (has_init) {
                if (array_len > 0) {
                    uint8_t init_tag = ast_read_u8(ast->reader);
                    if (init_tag != AST_TAG_STRING_LITERAL) {
                        log_error(SEM_ERR_VAR_INIT_NO_STRING);
                        return -1;
                    }
                    if (semantic_type_base_kind(&var_type) != AST_BASE_CHAR || var_type.depth != 0) {
                        log_error(SEM_ERR_TYPE_MISMATCH);
                        return -1;
                    }
                    ast_read_u16(ast->reader);
                    return 0;
                }
                semantic_type_t init_type;
                uint8_t init_const_zero = 0;
                if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                    &init_type, NULL, &init_const_zero) < 0) return -1;
                init_type = semantic_type_decay_array(init_type);
                if (semantic_type_is_void_scalar(&init_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (!semantic_type_can_convert(&var_type, init_type, init_const_zero)) {
                    log_error(SEM_ERR_TYPE_MISMATCH);
                    return -1;
                }
            }
            return 0;
        }
        case AST_TAG_COMPOUND_STMT: {
            uint16_t stmt_count = ast_read_u16(ast->reader);
            if (state && semantic_scope_push(state) < 0) return -1;
            for (uint16_t i = 0; i < stmt_count; i++) {
                if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL, NULL, NULL) < 0) {
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
                semantic_type_t expr_type;
                uint8_t expr_const_zero = 0;
                if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                    &expr_type, NULL, &expr_const_zero) < 0) return -1;
                expr_type = semantic_type_decay_array(expr_type);
                if (semantic_type_is_void_scalar(&expr_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (state && !semantic_type_can_convert(&state->return_type,
                                                        expr_type, expr_const_zero)) {
                    log_error(SEM_ERR_RETURN_TYPE);
                    return -1;
                }
                return 0;
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
            semantic_type_t cond_type;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &cond_type, NULL, NULL) < 0) return -1;
            cond_type = semantic_type_decay_array(cond_type);
            if (!semantic_type_is_scalar(&cond_type)) {
                log_error(SEM_ERR_INVALID_CONDITION);
                return -1;
            }
            if (semantic_check_node_with_lvalue(ast, loop_depth, state, NULL, NULL, NULL) < 0) return -1;
            if (has_else) {
                return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL, NULL, NULL);
            }
            return 0;
        }
        case AST_TAG_WHILE_STMT: {
            semantic_type_t cond_type;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &cond_type, NULL, NULL) < 0) return -1;
            cond_type = semantic_type_decay_array(cond_type);
            if (!semantic_type_is_scalar(&cond_type)) {
                log_error(SEM_ERR_INVALID_CONDITION);
                return -1;
            }
            return semantic_check_node_with_lvalue(ast, (uint8_t)(loop_depth + 1),
                                                   state, NULL, NULL, NULL);
        }
        case AST_TAG_FOR_STMT: {
            uint8_t has_init = ast_read_u8(ast->reader);
            uint8_t has_cond = ast_read_u8(ast->reader);
            uint8_t has_inc = ast_read_u8(ast->reader);
            if (has_init && semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                            NULL, NULL, NULL) < 0) return -1;
            if (has_cond) {
                semantic_type_t cond_type;
                if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                    &cond_type, NULL, NULL) < 0) return -1;
                cond_type = semantic_type_decay_array(cond_type);
                if (!semantic_type_is_scalar(&cond_type)) {
                    log_error(SEM_ERR_INVALID_CONDITION);
                    return -1;
                }
            }
            if (has_inc && semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                           NULL, NULL, NULL) < 0) return -1;
            return semantic_check_node_with_lvalue(ast, (uint8_t)(loop_depth + 1),
                                                   state, NULL, NULL, NULL);
        }
        case AST_TAG_ASSIGN: {
            semantic_type_t left_type;
            semantic_type_t right_type;
            uint8_t is_lvalue = 0;
            uint8_t right_const_zero = 0;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &left_type, &is_lvalue, NULL) < 0) return -1;
            if (!is_lvalue) {
                log_error(SEM_ERR_EXPECT_LVALUE);
                return -1;
            }
            if (semantic_type_is_array(&left_type)) {
                log_error(SEM_ERR_ASSIGN_TO_ARRAY);
                return -1;
            }
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &right_type, NULL, &right_const_zero) < 0) return -1;
            right_type = semantic_type_decay_array(right_type);
            if (semantic_type_is_void_scalar(&right_type)) {
                log_error(SEM_ERR_VOID_VALUE);
                return -1;
            }
            if (!semantic_type_can_convert(&left_type, right_type, right_const_zero)) {
                log_error(SEM_ERR_TYPE_MISMATCH);
                return -1;
            }
            if (out_type) *out_type = left_type;
            return 0;
        }
        case AST_TAG_CALL: {
            uint8_t arg_count = 0;
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            const semantic_symbol_t* sym = state ? semantic_scope_lookup(state, name) : NULL;
            uint8_t is_builtin = semantic_is_builtin_function(name);
            if (state && !sym && !is_builtin) {
                log_error(SEM_ERR_FUNC_UNDEFINED);
                log_error(name);
                log_error("\n");
                return -1;
            }
            if (sym && sym->kind != SEM_SYMBOL_FUNC) {
                log_error(SEM_ERR_FUNC_UNDEFINED);
                log_error(name);
                log_error("\n");
                return -1;
            }
            arg_count = ast_read_u8(ast->reader);
            if (sym && sym->param_count != arg_count) {
                log_error(SEM_ERR_CALL_ARG_COUNT);
                return -1;
            }
            for (uint8_t i = 0; i < arg_count; i++) {
                semantic_type_t arg_type;
                uint8_t arg_const_zero = 0;
                if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                    &arg_type, NULL, &arg_const_zero) < 0) return -1;
                arg_type = semantic_type_decay_array(arg_type);
                if (semantic_type_is_void_scalar(&arg_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (sym && i < sym->param_count) {
                    if (!semantic_type_can_convert(&sym->params[i], arg_type, arg_const_zero)) {
                        log_error(SEM_ERR_CALL_ARG_TYPE);
                        return -1;
                    }
                }
            }
            if (out_type) {
                if (sym) {
                    *out_type = sym->type;
                } else {
                    *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                }
            }
            return 0;
        }
        case AST_TAG_BINARY_OP: {
            uint8_t op = ast_read_u8(ast->reader);
            semantic_type_t left_type;
            semantic_type_t right_type;
            uint8_t left_const_zero = 0;
            uint8_t right_const_zero = 0;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &left_type, NULL, &left_const_zero) < 0) return -1;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &right_type, NULL, &right_const_zero) < 0) return -1;
            left_type = semantic_type_decay_array(left_type);
            right_type = semantic_type_decay_array(right_type);
            if (semantic_type_is_void_scalar(&left_type) ||
                semantic_type_is_void_scalar(&right_type)) {
                log_error(SEM_ERR_VOID_VALUE);
                return -1;
            }

            {
                uint8_t left_numeric = semantic_type_is_numeric(&left_type);
                uint8_t right_numeric = semantic_type_is_numeric(&right_type);
                uint8_t left_pointer = semantic_type_is_pointer(&left_type);
                uint8_t right_pointer = semantic_type_is_pointer(&right_type);

                if (op == OP_ADD || op == OP_SUB) {
                    if (left_pointer && right_numeric) {
                        if (out_type) *out_type = left_type;
                        return 0;
                    }
                    if (op == OP_ADD && right_pointer && left_numeric) {
                        if (out_type) *out_type = right_type;
                        return 0;
                    }
                    if (left_numeric && right_numeric) {
                        if (out_type) *out_type = semantic_type_numeric_result(&left_type, &right_type);
                        return 0;
                    }
                    log_error(SEM_ERR_TYPE_MISMATCH);
                    return -1;
                }
                if (op == OP_MUL || op == OP_DIV || op == OP_MOD ||
                    op == OP_AND || op == OP_OR || op == OP_XOR ||
                    op == OP_SHL || op == OP_SHR) {
                    if (!left_numeric || !right_numeric) {
                        log_error(SEM_ERR_TYPE_MISMATCH);
                        return -1;
                    }
                    if (out_type) *out_type = semantic_type_numeric_result(&left_type, &right_type);
                    return 0;
                }
                if (op == OP_LAND || op == OP_LOR) {
                    if (!semantic_type_is_scalar(&left_type) ||
                        !semantic_type_is_scalar(&right_type)) {
                        log_error(SEM_ERR_TYPE_MISMATCH);
                        return -1;
                    }
                    if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                    return 0;
                }
                if (op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE ||
                    op == OP_GT || op == OP_GE) {
                    if (left_pointer || right_pointer) {
                        if (left_pointer && right_pointer &&
                            semantic_type_compatible_pointer(&left_type, &right_type)) {
                            if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                            return 0;
                        }
                        if (left_pointer && right_numeric && right_const_zero) {
                            if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                            return 0;
                        }
                        if (right_pointer && left_numeric && left_const_zero) {
                            if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                            return 0;
                        }
                        log_error(SEM_ERR_TYPE_MISMATCH);
                        return -1;
                    }
                    if (!left_numeric || !right_numeric) {
                        log_error(SEM_ERR_TYPE_MISMATCH);
                        return -1;
                    }
                    if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                    return 0;
                }
            }
            return -1;
        }
        case AST_TAG_UNARY_OP: {
            uint8_t op = ast_read_u8(ast->reader);
            uint8_t child_tag = ast_read_u8(ast->reader);
            semantic_type_t child_type;
            semantic_type_t child_raw;
            uint8_t child_lvalue = 0;
            if (semantic_check_tag_with_lvalue(ast, child_tag, loop_depth, state,
                                               &child_type, &child_lvalue, NULL) < 0) return -1;
            child_raw = child_type;
            child_type = semantic_type_decay_array(child_type);

            if (op == OP_PREINC || op == OP_PREDEC || op == OP_POSTINC || op == OP_POSTDEC) {
                if (child_tag != AST_TAG_IDENTIFIER) {
                    log_error(SEM_ERR_INVALID_INCDEC);
                    return -1;
                }
                if (!child_lvalue || semantic_type_is_array(&child_raw) ||
                    !semantic_type_is_scalar(&child_type)) {
                    log_error(SEM_ERR_EXPECT_LVALUE);
                    return -1;
                }
                if (out_type) *out_type = child_type;
                return 0;
            }
            if (op == OP_ADDR) {
                if (child_tag != AST_TAG_IDENTIFIER) {
                    log_error(SEM_ERR_INVALID_ADDR);
                    return -1;
                }
                if (!child_lvalue) {
                    log_error(SEM_ERR_EXPECT_LVALUE);
                    return -1;
                }
                child_raw.array_len = 0;
                child_raw.depth++;
                if (out_type) *out_type = child_raw;
                return 0;
            }
            if (op == OP_DEREF) {
                if (child_tag != AST_TAG_IDENTIFIER) {
                    log_error(SEM_ERR_INVALID_DEREF);
                    return -1;
                }
                if (semantic_type_is_array(&child_raw) || !semantic_type_is_pointer(&child_type)) {
                    log_error(SEM_ERR_INVALID_DEREF);
                    return -1;
                }
                child_type.depth--;
                if (semantic_type_is_void_scalar(&child_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (out_type) *out_type = child_type;
                if (out_lvalue) *out_lvalue = 1;
                return 0;
            }
            if (op == OP_NEG || op == OP_NOT) {
                if (!semantic_type_is_numeric(&child_type)) {
                    log_error(SEM_ERR_TYPE_MISMATCH);
                    return -1;
                }
                if (out_type) *out_type = child_type;
                return 0;
            }
            if (op == OP_LNOT) {
                if (!semantic_type_is_scalar(&child_type)) {
                    log_error(SEM_ERR_TYPE_MISMATCH);
                    return -1;
                }
                if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
                return 0;
            }
            return -1;
        }
        case AST_TAG_IDENTIFIER: {
            uint16_t name_index = ast_read_u16(ast->reader);
            const char* name = ast_reader_string(ast, name_index);
            if (state) {
                const semantic_symbol_t* sym = semantic_scope_lookup(state, name);
                if (!sym || sym->kind != SEM_SYMBOL_VAR) {
                    log_error(SEM_ERR_IDENT_UNDEFINED);
                    log_error(name);
                    log_error("\n");
                    return -1;
                }
                if (out_type) *out_type = sym->type;
            }
            if (out_lvalue) *out_lvalue = 1;
            return 0;
        }
        case AST_TAG_CONSTANT: {
            int16_t value = ast_read_i16(ast->reader);
            if (out_type) *out_type = semantic_type_make(AST_BASE_INT, 0, 0);
            if (out_const_zero && value == 0) *out_const_zero = 1;
            return 0;
        }
        case AST_TAG_STRING_LITERAL:
            ast_read_u16(ast->reader);
            if (out_type) *out_type = semantic_type_make(AST_BASE_CHAR, 1, 0);
            return 0;
        case AST_TAG_ARRAY_ACCESS: {
            uint8_t base_tag = ast_read_u8(ast->reader);
            semantic_type_t base_type;
            semantic_type_t index_type;
            if (base_tag != AST_TAG_IDENTIFIER && base_tag != AST_TAG_STRING_LITERAL) {
                log_error(SEM_ERR_INVALID_ARRAY_BASE);
            }
            if (semantic_check_tag_with_lvalue(ast, base_tag, loop_depth, state,
                                               &base_type, NULL, NULL) < 0) return -1;
            if (semantic_check_node_with_lvalue(ast, loop_depth, state,
                                                &index_type, NULL, NULL) < 0) return -1;
            index_type = semantic_type_decay_array(index_type);
            if (!semantic_type_is_numeric(&index_type)) {
                log_error(SEM_ERR_INVALID_ARRAY_INDEX);
                return -1;
            }
            if (semantic_type_is_array(&base_type)) {
                base_type.array_len = 0;
                if (semantic_type_is_void_scalar(&base_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (out_type) *out_type = base_type;
            } else if (semantic_type_is_pointer(&base_type)) {
                if (base_type.depth == 0) {
                    log_error(SEM_ERR_INVALID_ARRAY_BASE);
                    return -1;
                }
                base_type.depth--;
                if (semantic_type_is_void_scalar(&base_type)) {
                    log_error(SEM_ERR_VOID_VALUE);
                    return -1;
                }
                if (out_type) *out_type = base_type;
            } else {
                log_error(SEM_ERR_INVALID_ARRAY_BASE);
                return -1;
            }
            if (out_lvalue) *out_lvalue = 1;
            return 0;
        }
        default:
            return -1;
    }
}

static int8_t semantic_check_node_with_lvalue(
    ast_reader_t* ast,
    uint8_t loop_depth,
    semantic_state_t* state,
    semantic_type_t* out_type,
    uint8_t* out_lvalue,
    uint8_t* out_const_zero
) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;
    tag = ast_read_u8(ast->reader);
    return semantic_check_tag_with_lvalue(ast, tag, loop_depth, state,
                                          out_type, out_lvalue, out_const_zero);
}

static int8_t semantic_check_node(ast_reader_t* ast, uint8_t loop_depth, semantic_state_t* state) {
    return semantic_check_node_with_lvalue(ast, loop_depth, state, NULL, NULL, NULL);
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
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_SEMANTIC;
            semantic_type_t return_type;
            return_type = semantic_type_make(base, depth, array_len);
            param_count = ast_read_u8(ast->reader);
            semantic_type_t params[SEM_MAX_PARAMS];
            if (param_count > SEM_MAX_PARAMS) {
                log_error(SEM_ERR_PARAM_OVERFLOW);
                return CC_ERROR_SEMANTIC;
            }
            for (uint8_t p = 0; p < param_count; p++) {
                uint8_t ptag = ast_read_u8(ast->reader);
                uint16_t param_name_index = 0;
                uint8_t param_base = 0;
                uint8_t param_depth = 0;
                uint16_t param_array_len = 0;
                uint8_t param_has_init = 0;
                if (ptag != AST_TAG_VAR_DECL) return CC_ERROR_SEMANTIC;
                param_name_index = ast_read_u16(ast->reader);
                (void)param_name_index;
                if (ast_reader_read_type_info(ast, &param_base, &param_depth,
                                              &param_array_len) < 0) return CC_ERROR_SEMANTIC;
                params[p] = semantic_type_make(param_base, param_depth, param_array_len);
                param_has_init = ast_read_u8(ast->reader);
                if (param_has_init && ast_reader_skip_node(ast) < 0) return CC_ERROR_SEMANTIC;
            }
            if (semantic_scope_add_func(&g_semantic_state, name, &return_type,
                                        param_count, params) < 0) return CC_ERROR_SEMANTIC;
            if (ast_reader_skip_node(ast) < 0) return CC_ERROR_SEMANTIC;
        } else if (tag == AST_TAG_VAR_DECL) {
            uint16_t name_index = ast_read_u16(ast->reader);
            uint8_t base = 0;
            uint8_t depth = 0;
            uint16_t array_len = 0;
            uint8_t has_init = 0;
            const char* name = ast_reader_string(ast, name_index);
            if (ast_reader_read_type_info(ast, &base, &depth, &array_len) < 0) return CC_ERROR_SEMANTIC;
            semantic_type_t var_type;
            var_type = semantic_type_make(base, depth, array_len);
            if (semantic_scope_add_var(&g_semantic_state, name, &var_type) < 0) return CC_ERROR_SEMANTIC;
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

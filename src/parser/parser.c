#include "cc_compat.h"
#include "parser.h"

#include "common.h"
#include "symbol.h"

/* Forward declarations */
static ast_node_t* parse_primary(parser_t* parser);
static ast_node_t* parse_expression(parser_t* parser);
static ast_node_t* parse_statement(parser_t* parser);
static ast_node_t* parse_declaration(parser_t* parser);
static ast_node_t* parse_parameter(parser_t* parser);
static ast_node_t* parse_variable_decl_after_name(
    parser_t* parser,
    type_t* var_type,
    char* name,
    const char* semicolon_msg);
static token_t* parser_current(parser_t* parser);
static bool parser_match(parser_t* parser, token_type_t type);
static ast_node_t* ast_node_create(ast_node_type_t type);
static ast_node_t* parse_function_after_name(parser_t* parser, char* name, type_t* return_type);
static type_t* parse_type(parser_t* parser);

static void parser_report_line(token_t* tok) {
    if (tok) {
        put_s(" at line ");
        put_hex(tok->line);
        put_s(", column ");
        put_hex(tok->column);
    }
    put_s("\n");
}

static void parser_report_expected(const char expect, const char* msg, token_t* tok) {
    put_s("[Parse error] Expected '");
    put_c(expect);
    put_s("' after ");
    if (msg && *msg) {
        put_s(" ");
        put_s(msg);
    }
    parser_report_line(tok);
}

static void parser_report_error(const char* msg, token_t* tok) {
    put_s("[Parse error] ");
    put_s(msg);
    parser_report_line(tok);
}

static void parser_error(parser_t* parser, const char* msg, token_t* tok) {
    parser_report_error(msg, tok);
    parser->error_count++;
}

static void parser_error_current(parser_t* parser, const char* msg) {
    parser_error(parser, msg, parser_current(parser));
}

static const char ERR_SIGNED_UNSIGNED[] = "Cannot combine signed and unsigned";
static const char ERR_VOID_SIGN[] = "Void type cannot be signed or unsigned";
static const char ERR_EXPECT_ARRAY_LEN[] = "Expected array length";
static const char ERR_ARRAY_POS[] = "Array length must be positive";
static const char ERR_ARRAY_VOID[] = "Array element type cannot be void";
static const char ERR_UNEXPECTED_EXPR[] = "Unexpected token in expression";
static const char ERR_EXPECT_IDENT[] = "Expected variable name";
static const char ERR_EXPECT_PARAM_TYPE[] = "Expected parameter type";
static const char ERR_EXPECT_PARAM_NAME[] = "Expected parameter name";
static const char ERR_EXPECT_FUNC_OR_VAR[] = "Expected function or variable name";
static const char ERR_EXPECT_DECL[] = "Expected declaration";
static const char ERR_AFTER_IF[] = "'if'";
static const char ERR_AFTER_WHILE[] = "'while'";
static const char ERR_AFTER_FOR[] = "'for'";
static const char ERR_AFTER_VAR_DECL[] = "variable declaration";
static const char ERR_AFTER_GLOBAL_DECL[] = "global declaration";
static const char ERR_AFTER_FOR_COND[] = "for condition";
static const char ERR_AFTER_EXPR[] = "expression";
static const char ERR_AFTER_INDEX[] = "index confirmation";
static const char ERR_AFTER_ARRAY_LEN[] = "array length";

typedef enum {
    SIGN_NONE = 0,
    SIGN_SIGNED,
    SIGN_UNSIGNED
} sign_state_t;

typedef struct {
    token_type_t tok;
    sign_state_t sign;
} sign_token_t;

typedef struct {
    token_type_t tok;
    type_kind_t kind;
} type_token_t;

static const sign_token_t k_sign_tokens[] = {
    { TOK_SIGNED, SIGN_SIGNED },
    { TOK_UNSIGNED, SIGN_UNSIGNED }
};

static const type_token_t k_type_tokens[] = {
    { TOK_INT, TYPE_INT },
    { TOK_CHAR_KW, TYPE_CHAR },
    { TOK_VOID, TYPE_VOID }
};

static int8_t parse_sign_state(parser_t* parser, sign_state_t* sign_state) {
    for (uint8_t i = 0; i < (uint8_t)(sizeof(k_sign_tokens) / sizeof(k_sign_tokens[0])); i++) {
        if (parser_match(parser, k_sign_tokens[i].tok)) {
            if (*sign_state != SIGN_NONE && *sign_state != k_sign_tokens[i].sign) {
                parser_error_current(parser, ERR_SIGNED_UNSIGNED);
                return -1;
            }
            *sign_state = k_sign_tokens[i].sign;
            return 1;
        }
    }
    return 0;
}

#define AST_NODE_TYPE_COUNT ((uint8_t)AST_ARRAY_ACCESS + 1)

parser_t* parser_create(lexer_t* lexer) {
    parser_t* parser = (parser_t*)cc_malloc(sizeof(parser_t));
    if (!parser) return NULL;

    parser->lexer = lexer;
    parser->current = lexer_next_token(lexer);
    parser->next = lexer_next_token(lexer);
    parser->error_count = 0;

    return parser;
}

void parser_destroy(parser_t* parser) {
    if (parser) {
        if (parser->current) {
            token_destroy(parser->current);
        }
        if (parser->next) {
            token_destroy(parser->next);
        }
        cc_free(parser);
    }
}

static token_t* parser_current(parser_t* parser) {
    return parser->current;
}

static token_type_t parser_peek_type(parser_t* parser) {
    if (!parser->next) return TOK_EOF;
    return parser->next->type;
}

static void parser_advance(parser_t* parser) {
    if (!parser->current) return;

    token_destroy(parser->current);
    parser->current = parser->next;
    parser->next = lexer_next_token(parser->lexer);
}

static bool parser_check(parser_t* parser, token_type_t type) {
    return parser_current(parser)->type == type;
}

static bool parser_match(parser_t* parser, token_type_t type) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static bool parser_consume(parser_t* parser, token_type_t type, const char* msg) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }

    parser_error_current(parser, msg);
    return false;
}

static bool parser_consume_expected(
    parser_t* parser,
    token_type_t type,
    const char* msg) {
    if (parser_check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    parser_report_expected(type, msg, parser_current(parser));
    parser->error_count++;
    return false;
}

static ast_node_t* ast_node_create(ast_node_type_t type) {
    ast_node_t* node = (ast_node_t*)cc_malloc(sizeof(ast_node_t));
    if (!node) return NULL;

    node->type = type;
    return node;
}

static type_t* parse_type(parser_t* parser) {
    sign_state_t sign_state = SIGN_NONE;
    type_t* type = NULL;

    if (parse_sign_state(parser, &sign_state) < 0) {
        return NULL;
    }

    for (uint8_t i = 0; i < (uint8_t)(sizeof(k_type_tokens) / sizeof(k_type_tokens[0])); i++) {
        if (parser_match(parser, k_type_tokens[i].tok)) {
            type = type_create(k_type_tokens[i].kind);
            break;
        }
    }
    if (!type && sign_state != SIGN_NONE) {
        type = type_create(TYPE_INT);
    }
    if (!type) return NULL;

    if (parse_sign_state(parser, &sign_state) < 0) {
        type_destroy(type);
        return NULL;
    }

    if (type->kind == TYPE_VOID) {
        if (sign_state != SIGN_NONE) {
            parser_error_current(parser, ERR_VOID_SIGN);
            type_destroy(type);
            return NULL;
        }
        return type;
    }

    if (type->kind == TYPE_INT || type->kind == TYPE_CHAR) {
        type->is_signed = (sign_state == SIGN_SIGNED);
    }

    return type;
}

static int8_t parse_array_suffix(parser_t* parser, uint16_t* out_len, bool allow_unsized) {
    if (!out_len) return -1;
    if (!parser_match(parser, TOK_LBRACKET)) return 0;
    if (parser_check(parser, TOK_RBRACKET)) {
        if (!allow_unsized) {
            parser_error_current(parser, ERR_EXPECT_ARRAY_LEN);
            return -1;
        }
        parser_advance(parser);
        *out_len = 0;
        return 1;
    }
    if (!parser_check(parser, TOK_NUMBER)) {
        parser_error_current(parser, ERR_EXPECT_ARRAY_LEN);
        return -1;
    }
    token_t* len_tok = parser_current(parser);
    int16_t len = len_tok->int_val;
    parser_advance(parser);
    if (len <= 0) {
        parser_error(parser, ERR_ARRAY_POS, len_tok);
        return -1;
    }
    if (!parser_consume_expected(parser, TOK_RBRACKET, ERR_AFTER_ARRAY_LEN)) {
        return -1;
    }
    *out_len = (uint16_t)len;
    return 1;
}

static type_t* parse_array_type(
    parser_t* parser,
    type_t* base,
    uint16_t array_len,
    int8_t array_suffix,
    bool as_pointer) {
    if (array_suffix <= 0) return base;
    if (parser_check(parser, TOK_LBRACKET)) {
        parser_error_current(parser, "Only single-dimension arrays supported");
        type_destroy(base);
        return NULL;
    }
    if (base->kind == TYPE_VOID) {
        parser_error_current(parser, ERR_ARRAY_VOID);
        type_destroy(base);
        return NULL;
    }
    if (as_pointer) {
        base = type_create_pointer(base);
    } else {
        base = type_create_array(base, array_len);
    }
    return base;
}

static ast_node_t* parse_variable_decl_after_name(
    parser_t* parser,
    type_t* var_type,
    char* name,
    const char* semicolon_msg) {
    uint16_t array_len = 0;
    int8_t array_suffix = parse_array_suffix(parser, &array_len, false);
    if (array_suffix < 0) {
        type_destroy(var_type);
        cc_free(name);
        return NULL;
    }
    var_type = parse_array_type(parser, var_type, array_len, array_suffix, false);
    if (!var_type) {
        cc_free(name);
        return NULL;
    }

    ast_node_t* node = ast_node_create(AST_VAR_DECL);
    if (!node) {
        type_destroy(var_type);
        cc_free(name);
        return NULL;
    }
    node->data.var_decl.name = name;
    node->data.var_decl.var_type = var_type;
    node->data.var_decl.initializer = NULL;

    if (parser_match(parser, TOK_ASSIGN)) {
        node->data.var_decl.initializer = parse_expression(parser);
        if (!node->data.var_decl.initializer) {
            ast_node_destroy(node);
            return NULL;
        }
    }

    parser_consume_expected(parser, TOK_SEMICOLON, semicolon_msg);
    return node;
}

static ast_node_t* parse_primary(parser_t* parser) {
    token_t* tok = parser_current(parser);
    ast_node_t* base = NULL;

    if (tok->type == TOK_IDENTIFIER) {
        char* name = tok->value;
        tok->value = NULL;
        parser_advance(parser);

        /* Check for function call: identifier '(' args ')' */
        if (parser_check(parser, TOK_LPAREN)) {
            parser_advance(parser); /* consume '(' */

            ast_node_t* call = ast_node_create(AST_CALL);
            if (!call) {
                cc_free(name);
                return NULL;
            }

            call->data.call.name = name;
            call->data.call.args = NULL;
            call->data.call.arg_count = 0;

            /* Parse arguments */
            if (!parser_check(parser, TOK_RPAREN)) {
                ast_node_t* args_tmp[8];
                uint16_t arg_count = 0;
                do {
                    ast_node_t* arg = parse_expression(parser);
                    if (!arg) {
                        for (uint16_t i = 0; i < arg_count; i++) {
                            ast_node_destroy(args_tmp[i]);
                        }
                        ast_node_destroy(call);
                        return NULL;
                    }
                    if (arg_count >= 8) {
                        cc_error("Too many call arguments");
                        parser->error_count++;
                        ast_node_destroy(arg);
                        break;
                    }
                    args_tmp[arg_count++] = arg;

                    if (parser_check(parser, TOK_COMMA)) {
                        parser_advance(parser);
                    } else {
                        break;
                    }
                } while (!parser_check(parser, TOK_RPAREN) && !parser_check(parser, TOK_EOF));

                if (arg_count > 0) {
                    ast_node_t** args = (ast_node_t**)cc_malloc(sizeof(ast_node_t*) * arg_count);
                    if (!args) {
                        for (uint16_t i = 0; i < arg_count; i++) {
                            ast_node_destroy(args_tmp[i]);
                        }
                        ast_node_destroy(call);
                        return NULL;
                    }
                    for (uint16_t i = 0; i < arg_count; i++) {
                        args[i] = args_tmp[i];
                    }
                    call->data.call.args = args;
                    call->data.call.arg_count = arg_count;
                }
            }

            if (!parser_consume_expected(parser, TOK_RPAREN, NULL)) {
                ast_node_destroy(call);
                return NULL;
            }

            base = call;
        } else {
            /* Just an identifier */
            ast_node_t* node = ast_node_create(AST_IDENTIFIER);
            if (node) {
                node->data.identifier.name = name;
            } else {
                cc_free(name);
            }
            base = node;
        }
    } else if (tok->type == TOK_NUMBER) {
        int16_t value = tok->int_val;
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_CONSTANT);
        if (node) {
            node->data.constant.int_value = value;
        }
        base = node;
    } else if (tok->type == TOK_CHAR) {
        int16_t value = tok->int_val;
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_CONSTANT);
        if (node) {
            node->data.constant.int_value = value;
        }
        base = node;
    } else if (tok->type == TOK_STRING) {
        char* value = tok->value;
        tok->value = NULL;
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_STRING_LITERAL);
        if (node) {
            node->data.string_literal.value = value;
        } else {
            cc_free(value);
        }
        base = node;
    } else if (parser_match(parser, TOK_LPAREN)) {
        ast_node_t* expr = parse_expression(parser);
        parser_consume_expected(parser, TOK_RPAREN, NULL);
        base = expr;
    } else {
        parser_error_current(parser, ERR_UNEXPECTED_EXPR);
        parser_advance(parser);
        return NULL;
    }

    while (base && parser_match(parser, TOK_LBRACKET)) {
        ast_node_t* index = parse_expression(parser);
        if (!index) {
            ast_node_destroy(base);
            return NULL;
        }
        if (!parser_consume_expected(parser, TOK_RBRACKET, ERR_AFTER_INDEX)) {
            ast_node_destroy(base);
            ast_node_destroy(index);
            return NULL;
        }
        ast_node_t* access = ast_node_create(AST_ARRAY_ACCESS);
        if (!access) {
            ast_node_destroy(base);
            ast_node_destroy(index);
            return NULL;
        }
        access->data.array_access.base = base;
        access->data.array_access.index = index;
        base = access;
    }

    while (base && (parser_check(parser, TOK_PLUS_PLUS) || parser_check(parser, TOK_MINUS_MINUS))) {
        token_type_t op = parser_current(parser)->type;
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (!node) {
            ast_node_destroy(base);
            return NULL;
        }
        node->data.unary_op.op = (op == TOK_PLUS_PLUS) ? OP_POSTINC : OP_POSTDEC;
        node->data.unary_op.operand = base;
        base = node;
    }

    return base;
}

/* Operator precedence parsing:
 * factor: primary (*, /, %) factor | primary
 * term: factor (+, -) term | factor
 * expression: term
 */
static ast_node_t* parse_unary(parser_t* parser);
static ast_node_t* parse_factor(parser_t* parser);
static ast_node_t* parse_term(parser_t* parser);
static ast_node_t* parse_shift(parser_t* parser);
static ast_node_t* parse_comparison(parser_t* parser);
static ast_node_t* parse_bitwise_and(parser_t* parser);
static ast_node_t* parse_bitwise_xor(parser_t* parser);
static ast_node_t* parse_bitwise_or(parser_t* parser);
static ast_node_t* parse_logical_and(parser_t* parser);
static ast_node_t* parse_logical_or(parser_t* parser);

typedef struct {
    token_type_t token;
    binary_op_t op;
} binop_map_t;

typedef struct {
    token_type_t token;
    unary_op_t op;
} unary_op_map_t;

static ast_node_t* parse_binary_left(parser_t* parser,
                                     ast_node_t* (*next)(parser_t*),
                                     const binop_map_t* ops,
                                     uint8_t op_count) {
    ast_node_t* left = next(parser);
    if (!left) return NULL;

    while (1) {
        token_type_t tok = parser_current(parser)->type;
        bool matched = false;
        binary_op_t op = OP_ADD;
        for (uint8_t i = 0; i < op_count; i++) {
            if (tok == ops[i].token) {
                matched = true;
                op = ops[i].op;
                break;
            }
        }
        if (!matched) break;

        parser_advance(parser);
        ast_node_t* right = next(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }

        ast_node_t* binop = ast_node_create(AST_BINARY_OP);
        if (!binop) {
            ast_node_destroy(left);
            ast_node_destroy(right);
            return NULL;
        }

        binop->data.binary_op.op = op;
        binop->data.binary_op.left = left;
        binop->data.binary_op.right = right;
        left = binop;
    }

    return left;
}

static ast_node_t* parse_unary(parser_t* parser) {
    if (parser_match(parser, TOK_PLUS)) {
        return parse_unary(parser);
    }
    {
        static const unary_op_map_t unary_ops[] = {
            { TOK_MINUS, OP_NEG },
            { TOK_EXCLAIM, OP_LNOT },
            { TOK_TILDE, OP_NOT },
            { TOK_PLUS_PLUS, OP_PREINC },
            { TOK_MINUS_MINUS, OP_PREDEC },
            { TOK_STAR, OP_DEREF },
            { TOK_AMPERSAND, OP_ADDR },
        };
        for (uint8_t i = 0; i < (uint8_t)(sizeof(unary_ops) / sizeof(unary_ops[0])); i++) {
            if (parser_match(parser, unary_ops[i].token)) {
                ast_node_t* operand = parse_unary(parser);
                if (!operand) return NULL;
                ast_node_t* node = ast_node_create(AST_UNARY_OP);
                if (!node) {
                    ast_node_destroy(operand);
                    return NULL;
                }
                node->data.unary_op.op = unary_ops[i].op;
                node->data.unary_op.operand = operand;
                return node;
            }
        }
    }
    return parse_primary(parser);
}

static ast_node_t* parse_factor(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_STAR, OP_MUL },
        { TOK_SLASH, OP_DIV },
        { TOK_PERCENT, OP_MOD },
    };
    return parse_binary_left(parser, parse_unary, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

static ast_node_t* parse_term(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_PLUS, OP_ADD },
        { TOK_MINUS, OP_SUB },
    };
    return parse_binary_left(parser, parse_factor, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Shift operators: <<, >> */
static ast_node_t* parse_shift(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_LSHIFT, OP_SHL },
        { TOK_RSHIFT, OP_SHR },
    };
    return parse_binary_left(parser, parse_term, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Comparison operators: ==, !=, <, >, <=, >= */
static ast_node_t* parse_comparison(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_LT, OP_LT },
        { TOK_GT, OP_GT },
        { TOK_LE, OP_LE },
        { TOK_GE, OP_GE },
        { TOK_EQ, OP_EQ },
        { TOK_NE, OP_NE },
    };
    return parse_binary_left(parser, parse_shift, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Bitwise AND: & */
static ast_node_t* parse_bitwise_and(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_AMPERSAND, OP_AND },
    };
    return parse_binary_left(parser, parse_comparison, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Bitwise XOR: ^ */
static ast_node_t* parse_bitwise_xor(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_CARET, OP_XOR },
    };
    return parse_binary_left(parser, parse_bitwise_and, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Bitwise OR: | */
static ast_node_t* parse_bitwise_or(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_PIPE, OP_OR },
    };
    return parse_binary_left(parser, parse_bitwise_xor, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Logical AND: && */
static ast_node_t* parse_logical_and(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_AND, OP_LAND },
    };
    return parse_binary_left(parser, parse_bitwise_or, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

/* Logical OR: || */
static ast_node_t* parse_logical_or(parser_t* parser) {
    static const binop_map_t ops[] = {
        { TOK_OR, OP_LOR },
    };
    return parse_binary_left(parser, parse_logical_and, ops,
                             (uint8_t)(sizeof(ops) / sizeof(ops[0])));
}

static ast_node_t* parse_expression(parser_t* parser) {
    ast_node_t* left = parse_logical_or(parser);
    if (!left) return NULL;

    /* Assignment is right-associative: a = b = c */
    if (parser_check(parser, TOK_ASSIGN)) {
        parser_advance(parser);
        ast_node_t* right = parse_expression(parser); /* Recursive for right-associativity */
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }

        ast_node_t* assign = ast_node_create(AST_ASSIGN);
        if (!assign) {
            ast_node_destroy(left);
            ast_node_destroy(right);
            return NULL;
        }

        assign->data.assign.lvalue = left;
        assign->data.assign.rvalue = right;
        return assign;
    }

    return left;
}

static ast_node_t* parse_statement(parser_t* parser) {
    /* Variable declaration: int x; or int x = 5; */
    type_t* var_type = parse_type(parser);
    if (var_type) {
        while (parser_match(parser, TOK_STAR)) {
            var_type = type_create_pointer(var_type);
        }

        token_t* name_tok = parser_current(parser);
        char* name = name_tok->value;
        name_tok->value = NULL;
        if (!parser_consume(parser, TOK_IDENTIFIER, ERR_EXPECT_IDENT)) {
            type_destroy(var_type);
            cc_free(name);
            return NULL;
        }
        return parse_variable_decl_after_name(
            parser, var_type, name, ERR_AFTER_VAR_DECL);
    }

    if (parser_match(parser, TOK_IF)) {
        ast_node_t* node = ast_node_create(AST_IF_STMT);
        if (!node) return NULL;

        if (!parser_consume_expected(parser, TOK_LPAREN, ERR_AFTER_IF)) {
            cc_free(node);
            return NULL;
        }

        node->data.if_stmt.condition = parse_expression(parser);
        if (!node->data.if_stmt.condition) {
            cc_free(node);
            return NULL;
        }

        if (!parser_consume_expected(parser, TOK_RPAREN, ERR_AFTER_IF)) {
            ast_node_destroy(node);
            return NULL;
        }

        node->data.if_stmt.then_branch = parse_statement(parser);
        if (!node->data.if_stmt.then_branch) {
            ast_node_destroy(node);
            return NULL;
        }

        if (parser_match(parser, TOK_ELSE)) {
            node->data.if_stmt.else_branch = parse_statement(parser);
        } else {
            node->data.if_stmt.else_branch = NULL;
        }

        return node;
    }

    if (parser_match(parser, TOK_WHILE)) {
        ast_node_t* node = ast_node_create(AST_WHILE_STMT);
        if (!node) return NULL;

        if (!parser_consume_expected(parser, TOK_LPAREN, ERR_AFTER_WHILE)) {
            cc_free(node);
            return NULL;
        }

        node->data.while_stmt.condition = parse_expression(parser);
        if (!node->data.while_stmt.condition) {
            cc_free(node);
            return NULL;
        }

        if (!parser_consume_expected(parser, TOK_RPAREN, ERR_AFTER_WHILE)) {
            ast_node_destroy(node);
            return NULL;
        }

        node->data.while_stmt.body = parse_statement(parser);
        if (!node->data.while_stmt.body) {
            ast_node_destroy(node);
            return NULL;
        }

        return node;
    }

    if (parser_match(parser, TOK_FOR)) {
        ast_node_t* node = ast_node_create(AST_FOR_STMT);
        if (!node) return NULL;

        if (!parser_consume_expected(parser, TOK_LPAREN, ERR_AFTER_FOR)) {
            cc_free(node);
            return NULL;
        }

        /* Init expression (or declaration) */
        if (!parser_check(parser, TOK_SEMICOLON)) {
            node->data.for_stmt.init = parse_statement(parser);
        } else {
            node->data.for_stmt.init = NULL;
            parser_advance(parser); /* consume ; */
        }

        /* Condition */
        if (!parser_check(parser, TOK_SEMICOLON)) {
            node->data.for_stmt.condition = parse_expression(parser);
        } else {
            node->data.for_stmt.condition = NULL;
        }
        if (!parser_consume_expected(parser, TOK_SEMICOLON, ERR_AFTER_FOR_COND)) {
            ast_node_destroy(node);
            return NULL;
        }

        /* Increment */
        if (!parser_check(parser, TOK_RPAREN)) {
            node->data.for_stmt.increment = parse_expression(parser);
        } else {
            node->data.for_stmt.increment = NULL;
        }

        if (!parser_consume_expected(parser, TOK_RPAREN, ERR_AFTER_FOR)) {
            ast_node_destroy(node);
            return NULL;
        }

        /* Body */
        node->data.for_stmt.body = parse_statement(parser);
        if (!node->data.for_stmt.body) {
            ast_node_destroy(node);
            return NULL;
        }

        return node;
    }

    if (parser_match(parser, TOK_RETURN)) {
        ast_node_t* node = ast_node_create(AST_RETURN_STMT);
        if (!parser_check(parser, TOK_SEMICOLON)) {
            node->data.return_stmt.expr = parse_expression(parser);
        } else {
            node->data.return_stmt.expr = NULL;
        }
        parser_consume_expected(parser, TOK_SEMICOLON, NULL);
        return node;
    }

    if (parser_match(parser, TOK_LBRACE)) {
        ast_node_t* node = ast_node_create(AST_COMPOUND_STMT);
        if (!node) return NULL;

        node->data.compound.statements = NULL;
        node->data.compound.stmt_count = 0;
        ast_node_t* stmts_tmp[32];

        while (!parser_check(parser, TOK_RBRACE) && !parser_check(parser, TOK_EOF) &&
               node->data.compound.stmt_count < 32) {
            ast_node_t* stmt = parse_statement(parser);
            if (stmt && node->data.compound.stmt_count < 32) {
                stmts_tmp[node->data.compound.stmt_count++] = stmt;
            }
        }
        if (node->data.compound.stmt_count > 0) {
            node->data.compound.statements = (ast_node_t**)cc_malloc(
                sizeof(ast_node_t*) * node->data.compound.stmt_count);
            if (!node->data.compound.statements) {
                for (ast_stmt_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                    ast_node_destroy(stmts_tmp[i]);
                }
                cc_free(node);
                return NULL;
            }
            for (ast_stmt_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                node->data.compound.statements[i] = stmts_tmp[i];
            }
        }
        parser_consume_expected(parser, TOK_RBRACE, NULL);
        return node;
    }

    /* Expression statement (including assignments): x = 5; */
    ast_node_t* expr = parse_expression(parser);
    if (expr) {
        parser_consume_expected(parser, TOK_SEMICOLON, ERR_AFTER_EXPR);
        return expr;
    }

    parser_advance(parser);
    return NULL;
}

static ast_node_t* parse_parameter(parser_t* parser) {
    ast_node_t* param = ast_node_create(AST_VAR_DECL);
    if (!param) return NULL;

    type_t* var_type = parse_type(parser);
    if (!var_type) {
        parser_error_current(parser, ERR_EXPECT_PARAM_TYPE);
        ast_node_destroy(param);
        return NULL;
    }
    while (parser_match(parser, TOK_STAR)) {
        var_type = type_create_pointer(var_type);
    }

    token_t* name_tok = parser_current(parser);
    char* name = name_tok->value;
    name_tok->value = NULL;
    if (!parser_consume(parser, TOK_IDENTIFIER, ERR_EXPECT_PARAM_NAME)) {
        type_destroy(var_type);
        cc_free(name);
        ast_node_destroy(param);
        return NULL;
    }

    uint16_t array_len = 0;
    int8_t array_suffix = parse_array_suffix(parser, &array_len, true);
    if (array_suffix < 0) {
        type_destroy(var_type);
        cc_free(name);
        ast_node_destroy(param);
        return NULL;
    }
    var_type = parse_array_type(parser, var_type, array_len, array_suffix, true);
    if (!var_type) {
        cc_free(name);
        ast_node_destroy(param);
        return NULL;
    }

    param->data.var_decl.name = name;
    param->data.var_decl.var_type = var_type;
    param->data.var_decl.initializer = NULL;
    return param;
}

static ast_node_t* parse_declaration(parser_t* parser) {
    type_t* decl_type = parse_type(parser);
    if (decl_type) {
        while (parser_match(parser, TOK_STAR)) {
            decl_type = type_create_pointer(decl_type);
        }

        token_t* name_tok = parser_current(parser);
        char* name = name_tok->value;
        name_tok->value = NULL;
        if (!parser_consume(parser, TOK_IDENTIFIER, ERR_EXPECT_FUNC_OR_VAR)) {
            type_destroy(decl_type);
            cc_free(name);
            return NULL;
        }

        if (parser_check(parser, TOK_LPAREN)) {
            return parse_function_after_name(parser, name, decl_type);
        }
        return parse_variable_decl_after_name(
            parser, decl_type, name, ERR_AFTER_GLOBAL_DECL);
    }

    token_t* tok = parser_current(parser);
    parser_error(parser, ERR_EXPECT_DECL, tok);
    return NULL;
}

static ast_node_t* parse_function_after_name(parser_t* parser, char* name, type_t* return_type) {
    ast_node_t* node = ast_node_create(AST_FUNCTION);
    if (!node) {
        type_destroy(return_type);
        cc_free(name);
        return NULL;
    }

    node->data.function.name = name;
    node->data.function.return_type = return_type;
    node->data.function.params = NULL;
    node->data.function.param_count = 0;

    if (!parser_consume_expected(parser, TOK_LPAREN, NULL)) {
        ast_node_destroy(node);
        return NULL;
    }

    /* Parse parameter list */
    if (parser_check(parser, TOK_VOID) && parser_peek_type(parser) == TOK_RPAREN) {
        parser_advance(parser); /* consume 'void' in (void) */
    } else {
        ast_node_t* params_tmp[8];
        while (!parser_check(parser, TOK_RPAREN) && !parser_check(parser, TOK_EOF)) {
            if (node->data.function.param_count >= 8) {
                cc_error("Too many function parameters");
                parser->error_count++;
                break;
            }

            ast_node_t* param = parse_parameter(parser);
            if (!param) {
                while (!parser_check(parser, TOK_RPAREN) && !parser_check(parser, TOK_EOF)) {
                    parser_advance(parser);
                }
                break;
            }

            params_tmp[node->data.function.param_count++] = param;

            if (parser_check(parser, TOK_COMMA)) {
                parser_advance(parser);
                continue;
            }

            if (parser_check(parser, TOK_RPAREN)) {
                break;
            }

            cc_error("Expected ',' or ')' in parameter list");
            parser->error_count++;
            parser_advance(parser);
        }
        if (node->data.function.param_count > 0) {
            node->data.function.params = (ast_node_t**)cc_malloc(
                sizeof(ast_node_t*) * node->data.function.param_count);
            if (!node->data.function.params) {
                for (ast_param_count_t i = 0; i < node->data.function.param_count; i++) {
                    ast_node_destroy(params_tmp[i]);
                }
                ast_node_destroy(node);
                return NULL;
            }
            for (ast_param_count_t i = 0; i < node->data.function.param_count; i++) {
                node->data.function.params[i] = params_tmp[i];
            }
        }
    }

    if (!parser_consume_expected(parser, TOK_RPAREN, NULL)) {
        ast_node_destroy(node);
        return NULL;
    }

    node->data.function.body = parse_statement(parser);
    return node;
}

ast_node_t* parser_parse(parser_t* parser) {
    ast_node_t* program = ast_node_create(AST_PROGRAM);
    if (!program) return NULL;

    program->data.program.declarations = NULL;
    program->data.program.decl_count = 0;
    ast_node_t* decls_tmp[32];

    while (!parser_check(parser, TOK_EOF)) {
        ast_node_t* decl = parse_declaration(parser);
        if (!decl) {
            if (parser->error_count == 0) {
                parser->error_count++;
            }
            break;
        }

        /* Store declaration in program node */
        if (program->data.program.decl_count < 32) {
            decls_tmp[program->data.program.decl_count++] = decl;
        }
    }

    if (program->data.program.decl_count > 0) {
        program->data.program.declarations = (ast_node_t**)cc_malloc(
            sizeof(ast_node_t*) * program->data.program.decl_count);
        if (!program->data.program.declarations) {
            for (ast_decl_count_t i = 0; i < program->data.program.decl_count; i++) {
                ast_node_destroy(decls_tmp[i]);
            }
            cc_free(program);
            return NULL;
        }
        for (ast_decl_count_t i = 0; i < program->data.program.decl_count; i++) {
            program->data.program.declarations[i] = decls_tmp[i];
        }
    }

    return program;
}

ast_node_t* parser_parse_next(parser_t* parser) {
    if (!parser || parser_check(parser, TOK_EOF)) return NULL;
    return parse_declaration(parser);
}

typedef void (*ast_destroy_fn)(ast_node_t* node);

static void ast_destroy_program(ast_node_t* node) {
    if (node->data.program.declarations) {
        for (ast_decl_count_t i = 0; i < node->data.program.decl_count; i++) {
            ast_node_destroy(node->data.program.declarations[i]);
        }
        cc_free(node->data.program.declarations);
    }
}

static void ast_destroy_compound(ast_node_t* node) {
    if (node->data.compound.statements) {
        for (ast_stmt_count_t i = 0; i < node->data.compound.stmt_count; i++) {
            ast_node_destroy(node->data.compound.statements[i]);
        }
        cc_free(node->data.compound.statements);
    }
}

static void ast_destroy_if(ast_node_t* node) {
    ast_node_destroy(node->data.if_stmt.condition);
    ast_node_destroy(node->data.if_stmt.then_branch);
    if (node->data.if_stmt.else_branch) {
        ast_node_destroy(node->data.if_stmt.else_branch);
    }
}

static void ast_destroy_while(ast_node_t* node) {
    ast_node_destroy(node->data.while_stmt.condition);
    ast_node_destroy(node->data.while_stmt.body);
}

static void ast_destroy_for(ast_node_t* node) {
    if (node->data.for_stmt.init) {
        ast_node_destroy(node->data.for_stmt.init);
    }
    if (node->data.for_stmt.condition) {
        ast_node_destroy(node->data.for_stmt.condition);
    }
    if (node->data.for_stmt.increment) {
        ast_node_destroy(node->data.for_stmt.increment);
    }
    ast_node_destroy(node->data.for_stmt.body);
}

static void ast_destroy_identifier(ast_node_t* node) {
    if (node->data.identifier.name) {
        cc_free(node->data.identifier.name);
    }
}

static void ast_destroy_function(ast_node_t* node) {
    if (node->data.function.name) {
        cc_free(node->data.function.name);
    }
    if (node->data.function.return_type) {
        type_destroy(node->data.function.return_type);
    }
    if (node->data.function.params) {
        for (ast_param_count_t i = 0; i < node->data.function.param_count; i++) {
            ast_node_destroy(node->data.function.params[i]);
        }
        cc_free(node->data.function.params);
    }
    if (node->data.function.body) {
        ast_node_destroy(node->data.function.body);
    }
}

static void ast_destroy_string(ast_node_t* node) {
    if (node->data.string_literal.value) {
        cc_free(node->data.string_literal.value);
    }
}

static void ast_destroy_binary(ast_node_t* node) {
    ast_node_destroy(node->data.binary_op.left);
    ast_node_destroy(node->data.binary_op.right);
}

static void ast_destroy_unary(ast_node_t* node) {
    ast_node_destroy(node->data.unary_op.operand);
}

static void ast_destroy_return(ast_node_t* node) {
    if (node->data.return_stmt.expr) {
        ast_node_destroy(node->data.return_stmt.expr);
    }
}

static void ast_destroy_var_decl(ast_node_t* node) {
    if (node->data.var_decl.name) {
        cc_free(node->data.var_decl.name);
    }
    if (node->data.var_decl.var_type) {
        type_destroy(node->data.var_decl.var_type);
    }
    if (node->data.var_decl.initializer) {
        ast_node_destroy(node->data.var_decl.initializer);
    }
}

static void ast_destroy_assign(ast_node_t* node) {
    ast_node_destroy(node->data.assign.lvalue);
    ast_node_destroy(node->data.assign.rvalue);
}

static void ast_destroy_call(ast_node_t* node) {
    if (node->data.call.name) {
        cc_free(node->data.call.name);
    }
    if (node->data.call.args) {
        for (ast_arg_count_t i = 0; i < node->data.call.arg_count; i++) {
            ast_node_destroy(node->data.call.args[i]);
        }
        cc_free(node->data.call.args);
    }
}

static void ast_destroy_array_access(ast_node_t* node) {
    ast_node_destroy(node->data.array_access.base);
    ast_node_destroy(node->data.array_access.index);
}

static const ast_destroy_fn g_ast_destroy_handlers[AST_NODE_TYPE_COUNT] = {
    ast_destroy_program,      /* AST_PROGRAM */
    ast_destroy_function,     /* AST_FUNCTION */
    ast_destroy_var_decl,     /* AST_VAR_DECL */
    ast_destroy_compound,     /* AST_COMPOUND_STMT */
    ast_destroy_if,           /* AST_IF_STMT */
    ast_destroy_while,        /* AST_WHILE_STMT */
    ast_destroy_for,          /* AST_FOR_STMT */
    ast_destroy_return,       /* AST_RETURN_STMT */
    ast_destroy_assign,       /* AST_ASSIGN */
    ast_destroy_call,         /* AST_CALL */
    ast_destroy_binary,       /* AST_BINARY_OP */
    ast_destroy_unary,        /* AST_UNARY_OP */
    ast_destroy_identifier,   /* AST_IDENTIFIER */
    NULL,                     /* AST_CONSTANT */
    ast_destroy_string,       /* AST_STRING_LITERAL */
    ast_destroy_array_access  /* AST_ARRAY_ACCESS */
};

void ast_node_destroy(ast_node_t* node) {
    if (!node) return;

    if (node->type < AST_NODE_TYPE_COUNT) {
        ast_destroy_fn fn = g_ast_destroy_handlers[node->type];
        if (fn) {
            fn(node);
        }
    }

    cc_free(node);
}

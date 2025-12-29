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
static ast_node_t* ast_node_create(ast_node_type_t type);
static ast_node_t* parse_function_after_name(parser_t* parser, char* name, type_t* return_type);
static type_t* parse_type(parser_t* parser);

static void parser_report_error(const char* msg, token_t* tok) {
    put_s("[Parse error] ");
    put_s(msg);
    if (tok) {
        put_s(" at line ");
        put_hex(tok->line);
        put_s(", column ");
        put_hex(tok->column);
    }
    put_s("\n");
}

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

    token_t* tok = parser_current(parser);
    parser_report_error(msg, tok);
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
    if (parser_match(parser, TOK_INT)) {
        return type_create(TYPE_INT);
    }
    if (parser_match(parser, TOK_CHAR_KW)) {
        return type_create(TYPE_CHAR);
    }
    if (parser_match(parser, TOK_VOID)) {
        return type_create(TYPE_VOID);
    }
    return NULL;
}

static int8_t parse_array_suffix(parser_t* parser, uint16_t* out_len, bool allow_unsized) {
    if (!out_len) return -1;
    if (!parser_match(parser, TOK_LBRACKET)) return 0;
    if (parser_check(parser, TOK_RBRACKET)) {
        if (!allow_unsized) {
            token_t* tok = parser_current(parser);
            parser_report_error("Expected array length", tok);
            parser->error_count++;
            return -1;
        }
        parser_advance(parser);
        *out_len = 0;
        return 1;
    }
    if (!parser_check(parser, TOK_NUMBER)) {
        token_t* tok = parser_current(parser);
        parser_report_error("Expected array length", tok);
        parser->error_count++;
        return -1;
    }
    token_t* len_tok = parser_current(parser);
    int16_t len = len_tok->int_val;
    parser_advance(parser);
    if (len <= 0) {
        parser_report_error("Array length must be positive", len_tok);
        parser->error_count++;
        return -1;
    }
    if (!parser_consume(parser, TOK_RBRACKET, "Expected ']' after array length")) {
        return -1;
    }
    *out_len = (uint16_t)len;
    return 1;
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

            if (!parser_consume(parser, TOK_RPAREN, "Expected ')' after function arguments")) {
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
        parser_consume(parser, TOK_RPAREN, "Expected ')'");
        base = expr;
    } else {
        token_t* tok = parser_current(parser);
        parser_report_error("Unexpected token in expression", tok);
        parser->error_count++;
        parser_advance(parser);
        return NULL;
    }

    while (base && parser_match(parser, TOK_LBRACKET)) {
        ast_node_t* index = parse_expression(parser);
        if (!index) {
            ast_node_destroy(base);
            return NULL;
        }
        if (!parser_consume(parser, TOK_RBRACKET, "Expected ']' after index confirmation")) {
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

static ast_node_t* parse_unary(parser_t* parser) {
    if (parser_match(parser, TOK_STAR)) {
        ast_node_t* operand = parse_unary(parser);
        if (!operand) return NULL;
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (!node) {
            ast_node_destroy(operand);
            return NULL;
        }
        node->data.unary_op.op = OP_DEREF;
        node->data.unary_op.operand = operand;
        return node;
    }
    if (parser_match(parser, TOK_AMPERSAND)) {
        ast_node_t* operand = parse_unary(parser);
        if (!operand) return NULL;
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (!node) {
            ast_node_destroy(operand);
            return NULL;
        }
        node->data.unary_op.op = OP_ADDR;
        node->data.unary_op.operand = operand;
        return node;
    }
    return parse_primary(parser);
}

static ast_node_t* parse_factor(parser_t* parser) {
    ast_node_t* left = parse_unary(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOK_STAR) ||
           parser_check(parser, TOK_SLASH) ||
           parser_check(parser, TOK_PERCENT)) {
        token_type_t op = parser_current(parser)->type;
        parser_advance(parser);

        ast_node_t* right = parse_unary(parser);
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

        /* Map token type to operator */
        if (op == TOK_STAR)
            binop->data.binary_op.op = OP_MUL;
        else if (op == TOK_SLASH)
            binop->data.binary_op.op = OP_DIV;
        else
            binop->data.binary_op.op = OP_MOD;

        binop->data.binary_op.left = left;
        binop->data.binary_op.right = right;
        left = binop;
    }

    return left;
}

static ast_node_t* parse_term(parser_t* parser) {
    ast_node_t* left = parse_factor(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOK_PLUS) || parser_check(parser, TOK_MINUS)) {
        token_type_t op = parser_current(parser)->type;
        parser_advance(parser);

        ast_node_t* right = parse_factor(parser);
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

        binop->data.binary_op.op = (op == TOK_PLUS) ? OP_ADD : OP_SUB;
        binop->data.binary_op.left = left;
        binop->data.binary_op.right = right;
        left = binop;
    }

    return left;
}

/* Comparison operators: ==, !=, <, >, <=, >= */
static ast_node_t* parse_comparison(parser_t* parser) {
    ast_node_t* left = parse_term(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOK_LT) || parser_check(parser, TOK_GT) ||
           parser_check(parser, TOK_LE) || parser_check(parser, TOK_GE) ||
           parser_check(parser, TOK_EQ) || parser_check(parser, TOK_NE)) {
        token_type_t op = parser_current(parser)->type;
        parser_advance(parser);

        ast_node_t* right = parse_term(parser);
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

        /* Map token type to operator */
        if (op == TOK_LT)
            binop->data.binary_op.op = OP_LT;
        else if (op == TOK_GT)
            binop->data.binary_op.op = OP_GT;
        else if (op == TOK_LE)
            binop->data.binary_op.op = OP_LE;
        else if (op == TOK_GE)
            binop->data.binary_op.op = OP_GE;
        else if (op == TOK_EQ)
            binop->data.binary_op.op = OP_EQ;
        else
            binop->data.binary_op.op = OP_NE;

        binop->data.binary_op.left = left;
        binop->data.binary_op.right = right;
        left = binop;
    }

    return left;
}

static ast_node_t* parse_expression(parser_t* parser) {
    ast_node_t* left = parse_comparison(parser);
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
        ast_node_t* node = ast_node_create(AST_VAR_DECL);
        if (!node) {
            type_destroy(var_type);
            return NULL;
        }

        while (parser_match(parser, TOK_STAR)) {
            var_type = type_create_pointer(var_type);
        }

        token_t* name_tok = parser_current(parser);
        char* name = name_tok->value;
        name_tok->value = NULL;
        if (!parser_consume(parser, TOK_IDENTIFIER, "Expected variable name")) {
            type_destroy(var_type);
            cc_free(name);
            ast_node_destroy(node);
            return NULL;
        }

        uint16_t array_len = 0;
        int8_t array_suffix = parse_array_suffix(parser, &array_len, false);
        if (array_suffix < 0) {
            type_destroy(var_type);
            cc_free(name);
            ast_node_destroy(node);
            return NULL;
        }
        if (array_suffix > 0) {
            if (parser_check(parser, TOK_LBRACKET)) {
                token_t* tok = parser_current(parser);
                parser_report_error("Only single-dimension arrays supported", tok);
                parser->error_count++;
                type_destroy(var_type);
                cc_free(name);
                ast_node_destroy(node);
                return NULL;
            }
            if (var_type->kind == TYPE_VOID) {
                token_t* tok = parser_current(parser);
                parser_report_error("Array element type cannot be void", tok);
                parser->error_count++;
                type_destroy(var_type);
                cc_free(name);
                ast_node_destroy(node);
                return NULL;
            }
            var_type = type_create_array(var_type, array_len);
            if (!var_type) {
                cc_free(name);
                ast_node_destroy(node);
                return NULL;
            }
        }

        node->data.var_decl.name = name;
        node->data.var_decl.var_type = var_type;
        node->data.var_decl.initializer = NULL;

        /* Check for initializer: int x = 5; */
        if (parser_match(parser, TOK_ASSIGN)) {
            node->data.var_decl.initializer = parse_expression(parser);
            if (!node->data.var_decl.initializer) {
                ast_node_destroy(node);
                return NULL;
            }
        }

        parser_consume(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

    if (parser_match(parser, TOK_IF)) {
        ast_node_t* node = ast_node_create(AST_IF_STMT);
        if (!node) return NULL;

        if (!parser_consume(parser, TOK_LPAREN, "Expected '(' after 'if'")) {
            cc_free(node);
            return NULL;
        }

        node->data.if_stmt.condition = parse_expression(parser);
        if (!node->data.if_stmt.condition) {
            cc_free(node);
            return NULL;
        }

        if (!parser_consume(parser, TOK_RPAREN, "Expected ')' after if condition")) {
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

        if (!parser_consume(parser, TOK_LPAREN, "Expected '(' after 'while'")) {
            cc_free(node);
            return NULL;
        }

        node->data.while_stmt.condition = parse_expression(parser);
        if (!node->data.while_stmt.condition) {
            cc_free(node);
            return NULL;
        }

        if (!parser_consume(parser, TOK_RPAREN, "Expected ')' after while condition")) {
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

        if (!parser_consume(parser, TOK_LPAREN, "Expected '(' after 'for'")) {
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
        if (!parser_consume(parser, TOK_SEMICOLON, "Expected ';' after for condition")) {
            ast_node_destroy(node);
            return NULL;
        }

        /* Increment */
        if (!parser_check(parser, TOK_RPAREN)) {
            node->data.for_stmt.increment = parse_expression(parser);
        } else {
            node->data.for_stmt.increment = NULL;
        }

        if (!parser_consume(parser, TOK_RPAREN, "Expected ')' after for clauses")) {
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
        parser_consume(parser, TOK_SEMICOLON, "Expected ';'");
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
                for (ast_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                    ast_node_destroy(stmts_tmp[i]);
                }
                cc_free(node);
                return NULL;
            }
            for (ast_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                node->data.compound.statements[i] = stmts_tmp[i];
            }
        }
        parser_consume(parser, TOK_RBRACE, "Expected '}'");
        return node;
    }

    /* Expression statement (including assignments): x = 5; */
    ast_node_t* expr = parse_expression(parser);
    if (expr) {
        parser_consume(parser, TOK_SEMICOLON, "Expected ';' after expression");
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
        token_t* tok = parser_current(parser);
        parser_report_error("Expected parameter type", tok);
        parser->error_count++;
        ast_node_destroy(param);
        return NULL;
    }
    while (parser_match(parser, TOK_STAR)) {
        var_type = type_create_pointer(var_type);
    }

    token_t* name_tok = parser_current(parser);
    char* name = name_tok->value;
    name_tok->value = NULL;
    if (!parser_consume(parser, TOK_IDENTIFIER, "Expected parameter name")) {
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
    if (array_suffix > 0) {
        if (parser_check(parser, TOK_LBRACKET)) {
            token_t* tok = parser_current(parser);
            parser_report_error("Only single-dimension arrays supported", tok);
            parser->error_count++;
            type_destroy(var_type);
            cc_free(name);
            ast_node_destroy(param);
            return NULL;
        }
        if (var_type->kind == TYPE_VOID) {
            token_t* tok = parser_current(parser);
            parser_report_error("Array element type cannot be void", tok);
            parser->error_count++;
            type_destroy(var_type);
            cc_free(name);
            ast_node_destroy(param);
            return NULL;
        }
        var_type = type_create_pointer(var_type);
        if (!var_type) {
            cc_free(name);
            ast_node_destroy(param);
            return NULL;
        }
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
        if (!parser_consume(parser, TOK_IDENTIFIER, "Expected function or variable name")) {
            type_destroy(decl_type);
            cc_free(name);
            return NULL;
        }

        if (parser_check(parser, TOK_LPAREN)) {
            return parse_function_after_name(parser, name, decl_type);
        }

        uint16_t array_len = 0;
        int8_t array_suffix = parse_array_suffix(parser, &array_len, false);
        if (array_suffix < 0) {
            type_destroy(decl_type);
            cc_free(name);
            return NULL;
        }
        if (array_suffix > 0) {
            if (parser_check(parser, TOK_LBRACKET)) {
                token_t* tok = parser_current(parser);
                parser_report_error("Only single-dimension arrays supported", tok);
                parser->error_count++;
                type_destroy(decl_type);
                cc_free(name);
                return NULL;
            }
            if (decl_type->kind == TYPE_VOID) {
                token_t* tok = parser_current(parser);
                parser_report_error("Array element type cannot be void", tok);
                parser->error_count++;
                type_destroy(decl_type);
                cc_free(name);
                return NULL;
            }
            decl_type = type_create_array(decl_type, array_len);
            if (!decl_type) {
                cc_free(name);
                return NULL;
            }
        }

        ast_node_t* node = ast_node_create(AST_VAR_DECL);
        if (!node) {
            type_destroy(decl_type);
            cc_free(name);
            return NULL;
        }
        node->data.var_decl.name = name;
        node->data.var_decl.var_type = decl_type;
        node->data.var_decl.initializer = NULL;

        if (parser_match(parser, TOK_ASSIGN)) {
            node->data.var_decl.initializer = parse_expression(parser);
            if (!node->data.var_decl.initializer) {
                ast_node_destroy(node);
                return NULL;
            }
        }
        parser_consume(parser, TOK_SEMICOLON, "Expected ';' after global declaration");
        return node;
    }

    token_t* tok = parser_current(parser);
    parser_report_error("Expected declaration", tok);
    parser->error_count++;
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

    if (!parser_consume(parser, TOK_LPAREN, "Expected '('")) {
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
                for (ast_count_t i = 0; i < node->data.function.param_count; i++) {
                    ast_node_destroy(params_tmp[i]);
                }
                ast_node_destroy(node);
                return NULL;
            }
            for (ast_count_t i = 0; i < node->data.function.param_count; i++) {
                node->data.function.params[i] = params_tmp[i];
            }
        }
    }

    if (!parser_consume(parser, TOK_RPAREN, "Expected ')'")) {
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
            for (ast_count_t i = 0; i < program->data.program.decl_count; i++) {
                ast_node_destroy(decls_tmp[i]);
            }
            cc_free(program);
            return NULL;
        }
        for (ast_count_t i = 0; i < program->data.program.decl_count; i++) {
            program->data.program.declarations[i] = decls_tmp[i];
        }
    }

    return program;
}

ast_node_t* parser_parse_next(parser_t* parser) {
    if (!parser || parser_check(parser, TOK_EOF)) return NULL;
    return parse_declaration(parser);
}

void ast_node_destroy(ast_node_t* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            if (node->data.program.declarations) {
                for (ast_count_t i = 0; i < node->data.program.decl_count; i++) {
                    ast_node_destroy(node->data.program.declarations[i]);
                }
                cc_free(node->data.program.declarations);
            }
            break;
        case AST_COMPOUND_STMT:
            if (node->data.compound.statements) {
                for (ast_count_t i = 0; i < node->data.compound.stmt_count; i++) {
                    ast_node_destroy(node->data.compound.statements[i]);
                }
                cc_free(node->data.compound.statements);
            }
            break;
        case AST_IF_STMT:
            ast_node_destroy(node->data.if_stmt.condition);
            ast_node_destroy(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                ast_node_destroy(node->data.if_stmt.else_branch);
            }
            break;
        case AST_WHILE_STMT:
            ast_node_destroy(node->data.while_stmt.condition);
            ast_node_destroy(node->data.while_stmt.body);
            break;
        case AST_FOR_STMT:
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
            break;
        case AST_IDENTIFIER:
            if (node->data.identifier.name) {
                cc_free(node->data.identifier.name);
            }
            break;
        case AST_FUNCTION:
            if (node->data.function.name) {
                cc_free(node->data.function.name);
            }
            if (node->data.function.return_type) {
                type_destroy(node->data.function.return_type);
            }
            if (node->data.function.params) {
                for (ast_count_t i = 0; i < node->data.function.param_count; i++) {
                    ast_node_destroy(node->data.function.params[i]);
                }
                cc_free(node->data.function.params);
            }
            if (node->data.function.body) {
                ast_node_destroy(node->data.function.body);
            }
            break;
        case AST_STRING_LITERAL:
            if (node->data.string_literal.value) {
                cc_free(node->data.string_literal.value);
            }
            break;
        case AST_BINARY_OP:
            ast_node_destroy(node->data.binary_op.left);
            ast_node_destroy(node->data.binary_op.right);
            break;
        case AST_RETURN_STMT:
            if (node->data.return_stmt.expr) {
                ast_node_destroy(node->data.return_stmt.expr);
            }
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.name) {
                cc_free(node->data.var_decl.name);
            }
            if (node->data.var_decl.var_type) {
                type_destroy(node->data.var_decl.var_type);
            }
            if (node->data.var_decl.initializer) {
                ast_node_destroy(node->data.var_decl.initializer);
            }
            break;
        case AST_ASSIGN:
            ast_node_destroy(node->data.assign.lvalue);
            ast_node_destroy(node->data.assign.rvalue);
            break;
        case AST_CALL:
            if (node->data.call.name) {
                cc_free(node->data.call.name);
            }
            if (node->data.call.args) {
                for (ast_count_t i = 0; i < node->data.call.arg_count; i++) {
                    ast_node_destroy(node->data.call.args[i]);
                }
                cc_free(node->data.call.args);
            }
            break;
        case AST_ARRAY_ACCESS:
            ast_node_destroy(node->data.array_access.base);
            ast_node_destroy(node->data.array_access.index);
            break;
        default:
            break;
    }

    cc_free(node);
}

const char* ast_node_type_to_string(ast_node_type_t type) {
    switch (type) {
        case AST_PROGRAM:
            return "PROGRAM";
        case AST_FUNCTION:
            return "FUNCTION";
        case AST_IDENTIFIER:
            return "IDENTIFIER";
        case AST_CONSTANT:
            return "CONSTANT";
        case AST_RETURN_STMT:
            return "RETURN";
        case AST_BINARY_OP:
            return "BINARY_OP";
        case AST_VAR_DECL:
            return "VAR_DECL";
        case AST_ASSIGN:
            return "ASSIGN";
        case AST_CALL:
            return "CALL";
        default:
            return "UNKNOWN";
    }
}

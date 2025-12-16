#include "parser.h"

#include "common.h"
#include "symbol.h"

/* Forward declarations */
static ast_node_t* parse_primary(parser_t* parser);
static ast_node_t* parse_expression(parser_t* parser);
static ast_node_t* parse_statement(parser_t* parser);
static ast_node_t* parse_function(parser_t* parser);
static ast_node_t* parse_declaration(parser_t* parser);
static ast_node_t* ast_node_create(ast_node_type_t type);

parser_t* parser_create(token_t* tokens) {
    parser_t* parser = (parser_t*)cc_malloc(sizeof(parser_t));
    if (!parser) return NULL;

    parser->tokens = tokens;
    parser->current = tokens;
    parser->error_count = 0;

    return parser;
}

void parser_destroy(parser_t* parser) {
    if (parser) {
        cc_free(parser);
    }
}

static token_t* parser_current(parser_t* parser) {
    return parser->current;
}

static void parser_advance(parser_t* parser) {
    if (parser->current && parser->current->type != TOK_EOF) {
        if (parser->current->next) {
            parser->current = parser->current->next;
        }
    }
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

    cc_error(msg);
    parser->error_count++;
    return false;
}

static ast_node_t* ast_node_create(ast_node_type_t type) {
    ast_node_t* node = (ast_node_t*)cc_malloc(sizeof(ast_node_t));
    if (!node) return NULL;

    node->type = type;
    node->line = 0;
    node->column = 0;
    node->expr_type = NULL;
    node->next = NULL;

    return node;
}

static ast_node_t* parse_primary(parser_t* parser) {
    token_t* tok = parser_current(parser);

    if (tok->type == TOK_IDENTIFIER) {
        char* name = cc_strdup(tok->value);
        int line = tok->line;
        int column = tok->column;
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
            call->line = line;
            call->column = column;

            /* Parse arguments */
            if (!parser_check(parser, TOK_RPAREN)) {
                /* Simple arg parsing - just count for now */
                int arg_count = 0;
                do {
                    ast_node_t* arg = parse_expression(parser);
                    if (!arg) {
                        ast_node_destroy(call);
                        return NULL;
                    }
                    ast_node_destroy(arg); /* Not storing args yet */
                    arg_count++;

                    if (parser_check(parser, TOK_COMMA)) {
                        parser_advance(parser);
                    } else {
                        break;
                    }
                } while (!parser_check(parser, TOK_RPAREN) && !parser_check(parser, TOK_EOF));

                call->data.call.arg_count = arg_count;
            }

            if (!parser_consume(parser, TOK_RPAREN, "Expected ')' after function arguments")) {
                ast_node_destroy(call);
                return NULL;
            }

            return call;
        }

        /* Just an identifier */
        ast_node_t* node = ast_node_create(AST_IDENTIFIER);
        if (node) {
            node->data.identifier.name = name;
            node->line = line;
            node->column = column;
        } else {
            cc_free(name);
        }
        return node;
    }

    if (tok->type == TOK_NUMBER) {
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_CONSTANT);
        if (node) {
            node->data.constant.int_value = tok->data.int_val;
            node->data.constant.is_float = false;
            node->line = tok->line;
            node->column = tok->column;
        }
        return node;
    }

    if (tok->type == TOK_STRING) {
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_STRING_LITERAL);
        if (node) {
            node->data.string_literal.value = cc_strdup(tok->value);
            node->line = tok->line;
            node->column = tok->column;
        }
        return node;
    }

    if (parser_match(parser, TOK_LPAREN)) {
        ast_node_t* expr = parse_expression(parser);
        parser_consume(parser, TOK_RPAREN, "Expected ')'");
        return expr;
    }

    cc_error("Unexpected token in expression");
    parser->error_count++;
    parser_advance(parser);
    return NULL;
}

/* Operator precedence parsing:
 * factor: primary (*, /, %) factor | primary
 * term: factor (+, -) term | factor
 * expression: term
 */
static ast_node_t* parse_factor(parser_t* parser);
static ast_node_t* parse_term(parser_t* parser);

static ast_node_t* parse_factor(parser_t* parser) {
    ast_node_t* left = parse_primary(parser);
    if (!left) return NULL;

    while (parser_check(parser, TOK_STAR) ||
           parser_check(parser, TOK_SLASH) ||
           parser_check(parser, TOK_PERCENT)) {
        token_type_t op = parser_current(parser)->type;
        parser_advance(parser);

        ast_node_t* right = parse_primary(parser);
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
    if (parser_check(parser, TOK_INT) || parser_check(parser, TOK_VOID) ||
        parser_check(parser, TOK_CHAR)) {
        ast_node_t* node = ast_node_create(AST_VAR_DECL);
        if (!node) return NULL;

        parser_advance(parser); /* consume type */

        token_t* name_tok = parser_current(parser);
        if (!parser_consume(parser, TOK_IDENTIFIER, "Expected variable name")) {
            ast_node_destroy(node);
            return NULL;
        }

        node->data.var_decl.name = cc_strdup(name_tok->value);
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

        /* Allocate array for statements (max 128 for now) */
        node->data.compound.statements = (ast_node_t**)cc_malloc(sizeof(ast_node_t*) * 128);
        if (!node->data.compound.statements) {
            cc_free(node);
            return NULL;
        }
        node->data.compound.stmt_count = 0;

        while (!parser_check(parser, TOK_RBRACE) && !parser_check(parser, TOK_EOF)) {
            ast_node_t* stmt = parse_statement(parser);
            if (stmt && node->data.compound.stmt_count < 128) {
                node->data.compound.statements[node->data.compound.stmt_count++] = stmt;
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

static ast_node_t* parse_function(parser_t* parser) {
    ast_node_t* node = ast_node_create(AST_FUNCTION);
    if (!node) return NULL;

    if (parser_match(parser, TOK_INT) || parser_match(parser, TOK_VOID)) {
        /* Got type */
    }

    token_t* name_tok = parser_current(parser);
    if (!parser_consume(parser, TOK_IDENTIFIER, "Expected function name")) {
        cc_free(node);
        return NULL;
    }

    node->data.function.name = cc_strdup(name_tok->value);

    if (!parser_consume(parser, TOK_LPAREN, "Expected '('")) {
        cc_free(node);
        return NULL;
    }

    while (!parser_check(parser, TOK_RPAREN) && !parser_check(parser, TOK_EOF)) {
        parser_advance(parser);
    }

    if (!parser_consume(parser, TOK_RPAREN, "Expected ')'")) {
        cc_free(node);
        return NULL;
    }

    node->data.function.body = parse_statement(parser);

    return node;
}

static ast_node_t* parse_declaration(parser_t* parser) {
    return parse_function(parser);
}

ast_node_t* parser_parse(parser_t* parser) {
    ast_node_t* program = ast_node_create(AST_PROGRAM);
    if (!program) return NULL;

    /* Allocate array for declarations (max 128 for now) */
    program->data.program.declarations = (ast_node_t**)cc_malloc(sizeof(ast_node_t*) * 128);
    if (!program->data.program.declarations) {
        cc_free(program);
        return NULL;
    }
    program->data.program.decl_count = 0;

    while (!parser_check(parser, TOK_EOF)) {
        ast_node_t* decl = parse_declaration(parser);
        if (!decl) {
            if (parser->error_count == 0) {
                parser->error_count++;
            }
            break;
        }

        /* Store declaration in program node */
        if (program->data.program.decl_count < 128) {
            program->data.program.declarations[program->data.program.decl_count++] = decl;
        }
    }

    return program;
}

void ast_node_destroy(ast_node_t* node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            if (node->data.program.declarations) {
                for (size_t i = 0; i < node->data.program.decl_count; i++) {
                    ast_node_destroy(node->data.program.declarations[i]);
                }
                cc_free(node->data.program.declarations);
            }
            break;
        case AST_COMPOUND_STMT:
            if (node->data.compound.statements) {
                for (size_t i = 0; i < node->data.compound.stmt_count; i++) {
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
                /* Would need to free arg array if we stored it */
            }
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
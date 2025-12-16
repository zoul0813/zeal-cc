#include "parser.h"
#include "common.h"
#include "symbol.h"

/* Forward declarations of parsing functions */
static ast_node_t* parse_declaration(parser_t* parser);
static ast_node_t* parse_function(parser_t* parser);
static ast_node_t* parse_var_declaration(parser_t* parser);
static ast_node_t* parse_statement(parser_t* parser);
static ast_node_t* parse_expression(parser_t* parser);
static ast_node_t* parse_assignment(parser_t* parser);
static ast_node_t* parse_logical_or(parser_t* parser);
static ast_node_t* parse_logical_and(parser_t* parser);
static ast_node_t* parse_equality(parser_t* parser);
static ast_node_t* parse_comparison(parser_t* parser);
static ast_node_t* parse_term(parser_t* parser);
static ast_node_t* parse_factor(parser_t* parser);
static ast_node_t* parse_unary(parser_t* parser);
static ast_node_t* parse_postfix(parser_t* parser);
static ast_node_t* parse_primary(parser_t* parser);
static type_t* parse_type(parser_t* parser);

/* Helper functions */
static bool parser_match(parser_t* parser, token_type_t type);
static bool parser_consume(parser_t* parser, token_type_t type, const char* msg);
static token_t* parser_current(parser_t* parser);
static void parser_advance(parser_t* parser);
static bool parser_check(parser_t* parser, token_type_t type);
static bool is_type_keyword(token_type_t type);

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

static bool is_type_keyword(token_type_t type) {
    return type == TOK_INT || type == TOK_CHAR_KW || type == TOK_VOID ||
           type == TOK_SHORT || type == TOK_LONG || type == TOK_FLOAT ||
           type == TOK_DOUBLE || type == TOK_SIGNED || type == TOK_UNSIGNED;
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

/* Parse type specifier */
static type_t* parse_type(parser_t* parser) {
    type_t* type = NULL;
    
    if (parser_match(parser, TOK_INT)) {
        type = type_create(TYPE_INT);
    } else if (parser_match(parser, TOK_CHAR_KW)) {
        type = type_create(TYPE_CHAR);
    } else if (parser_match(parser, TOK_VOID)) {
        type = type_create(TYPE_VOID);
    } else if (parser_match(parser, TOK_SHORT)) {
        type = type_create(TYPE_SHORT);
    } else if (parser_match(parser, TOK_LONG)) {
        type = type_create(TYPE_LONG);
    } else {
        cc_error("Expected type specifier");
        parser->error_count++;
        return NULL;
    }
    
    /* Handle pointer types */
    while (parser_match(parser, TOK_STAR)) {
        type = type_create_pointer(type);
    }
    
    return type;
}

/* Parse primary expression: identifier, number, string, (expr) */
static ast_node_t* parse_primary(parser_t* parser) {
    token_t* tok = parser_current(parser);
    
    if (tok->type == TOK_IDENTIFIER) {
        parser_advance(parser);
        ast_node_t* node = ast_node_create(AST_IDENTIFIER);
        if (node) {
            node->data.identifier.name = cc_strdup(tok->value);
            node->line = tok->line;
            node->column = tok->column;
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
    
/* Parse variable declaration: int x; or int x = 5; */
static ast_node_t* parse_var_declaration(parser_t* parser) {
    type_t* var_type = parse_type(parser);
    if (!var_type) return NULL;
    
    token_t* name_tok = parser_current(parser);
    if (!parser_consume(parser, TOK_IDENTIFIER, "Expected variable name")) {
        type_destroy(var_type);
        return NULL;
    }
    
    ast_node_t* node = ast_node_create(AST_VAR_DECL);
    if (!node) {
        type_destroy(var_type);
        return NULL;
    }
    
    node->data.var_decl.name = cc_strdup(name_tok->value);
    node->data.var_decl.var_type = var_type;
    node->line = name_tok->line;
    node->column = name_tok->column;
    
    /* Check for initializer */
    if (parser_match(parser, TOK_ASSIGN)) {
        node->data.var_decl.initializer = parse_expression(parser);
    } else {
        node->data.var_decl.initializer = NULL;
    }
    
    parser_consume(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

/* Parse statement */
static ast_node_t* parse_statement(parser_t* parser) {
    /* Variable declaration */
    if (is_type_keyword(parser_current(parser)->type)) {
        return parse_var_declaration(parser);
    }
    
    /* Return statement */
/* Parse function definition */
static ast_node_t* parse_function(parser_t* parser) {
    ast_node_t* node = ast_node_create(AST_FUNCTION);
    if (!node) return NULL;
    
    /* Parse return type */
    type_t* return_type = parse_type(parser);
    if (!return_type) {
        cc_free(node);
        return NULL;
    }
    node->data.function.return_type = return_type;
    
    /* Parse function name */
    token_t* name_tok = parser_current(parser);
    if (!parser_consume(parser, TOK_IDENTIFIER, "Expected function name")) {
        type_destroy(return_type);
        cc_free(node);
        return NULL;
    }
    
    node->data.function.name = cc_strdup(name_tok->value);
    node->line = name_tok->line;
    node->column = name_tok->column;
    
    /* Parse parameters */
    if (!parser_consume(parser, TOK_LPAREN, "Expected '('")) {
        cc_free(node);
        return NULL;
    }
    
    /* Parse parameter list */
    node->data.function.param_count = 0;
    node->data.function.params = NULL;
    
    if (!parser_check(parser, TOK_RPAREN)) {
        do {
            /* Parse parameter type */
            if (is_type_keyword(parser_current(parser)->type)) {
                type_t* param_type = parse_type(parser);
                
                /* Parse parameter name (optional in declarations) */
                if (parser_check(parser, TOK_IDENTIFIER)) {
                    parser_advance(parser);
                }
                
                node->data.function.param_count++;
                
                if (param_type) {
                    type_destroy(param_type);
                }
            } else {
                break;
            }
        } while (parser_match(parser, TOK_COMMA));
    }
    
    if (!parser_consume(parser, TOK_RPAREN, "Expected ')'")) {
        cc_free(node);
        return NULL;
    }
    
    /* Parse function body */
    node->data.function.body = parse_statement(parser);
    
    return node;
}   
    /* Empty statement */
    if (parser_match(parser, TOK_SEMICOLON)) {
        return ast_node_create(AST_EXPR_STMT);
    }
    
    return NULL;
}           /* Array access */
            ast_node_t* index = parse_expression(parser);
            parser_consume(parser, TOK_RBRACKET, "Expected ']'");
            
            ast_node_t* access = ast_node_create(AST_ARRAY_ACCESS);
            if (access) {
                /* Store array and index - simplified */
            }
            ast_node_destroy(index);
        } else {
            break;
        }
    }
    
    return expr;
}

/* Parse unary expression: -expr, !expr, &expr, *expr, ++expr, --expr */
static ast_node_t* parse_unary(parser_t* parser) {
    if (parser_match(parser, TOK_MINUS)) {
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (node) {
            node->data.unary_op.op = OP_NEG;
            node->data.unary_op.operand = parse_unary(parser);
        }
        return node;
    }
    
    if (parser_match(parser, TOK_EXCLAIM)) {
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (node) {
            node->data.unary_op.op = OP_LNOT;
            node->data.unary_op.operand = parse_unary(parser);
        }
        return node;
    }
    
    if (parser_match(parser, TOK_AMPERSAND)) {
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (node) {
            node->data.unary_op.op = OP_ADDR;
            node->data.unary_op.operand = parse_unary(parser);
        }
        return node;
    }
    
    if (parser_match(parser, TOK_STAR)) {
        ast_node_t* node = ast_node_create(AST_UNARY_OP);
        if (node) {
            node->data.unary_op.op = OP_DEREF;
            node->data.unary_op.operand = parse_unary(parser);
        }
        return node;
    }
    
    return parse_postfix(parser);
}

/* Parse multiplicative expression: *, /, % */
static ast_node_t* parse_factor(parser_t* parser) {
    ast_node_t* left = parse_unary(parser);
    if (!left) return NULL;
    
    while (true) {
        binary_op_t op;
        
        if (parser_match(parser, TOK_STAR)) {
            op = OP_MUL;
        } else if (parser_match(parser, TOK_SLASH)) {
            op = OP_DIV;
        } else if (parser_match(parser, TOK_PERCENT)) {
            op = OP_MOD;
        } else {
            break;
        }
        
        ast_node_t* right = parse_unary(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = op;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse additive expression: +, - */
static ast_node_t* parse_term(parser_t* parser) {
    ast_node_t* left = parse_factor(parser);
    if (!left) return NULL;
    
    while (true) {
        binary_op_t op;
        
        if (parser_match(parser, TOK_PLUS)) {
            op = OP_ADD;
        } else if (parser_match(parser, TOK_MINUS)) {
            op = OP_SUB;
        } else {
            break;
        }
        
        ast_node_t* right = parse_factor(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = op;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse comparison expression: <, >, <=, >= */
static ast_node_t* parse_comparison(parser_t* parser) {
    ast_node_t* left = parse_term(parser);
    if (!left) return NULL;
    
    while (true) {
        binary_op_t op;
        
        if (parser_match(parser, TOK_LT)) {
            op = OP_LT;
        } else if (parser_match(parser, TOK_GT)) {
            op = OP_GT;
        } else if (parser_match(parser, TOK_LE)) {
            op = OP_LE;
        } else if (parser_match(parser, TOK_GE)) {
            op = OP_GE;
        } else {
            break;
        }
        
        ast_node_t* right = parse_term(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = op;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse equality expression: ==, != */
static ast_node_t* parse_equality(parser_t* parser) {
    ast_node_t* left = parse_comparison(parser);
    if (!left) return NULL;
    
    while (true) {
        binary_op_t op;
        
        if (parser_match(parser, TOK_EQ)) {
            op = OP_EQ;
        } else if (parser_match(parser, TOK_NE)) {
            op = OP_NE;
        } else {
            break;
        }
        
        ast_node_t* right = parse_comparison(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = op;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse logical AND expression: && */
static ast_node_t* parse_logical_and(parser_t* parser) {
    ast_node_t* left = parse_equality(parser);
    if (!left) return NULL;
    
    while (parser_match(parser, TOK_AND)) {
        ast_node_t* right = parse_equality(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = OP_LAND;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse logical OR expression: || */
static ast_node_t* parse_logical_or(parser_t* parser) {
    ast_node_t* left = parse_logical_and(parser);
    if (!left) return NULL;
    
    while (parser_match(parser, TOK_OR)) {
        ast_node_t* right = parse_logical_and(parser);
        if (!right) {
            ast_node_destroy(left);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_BINARY_OP);
        if (node) {
            node->data.binary_op.op = OP_LOR;
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        left = node;
    }
    
    return left;
}

/* Parse assignment expression: = */
static ast_node_t* parse_assignment(parser_t* parser) {
    ast_node_t* expr = parse_logical_or(parser);
    if (!expr) return NULL;
    
    if (parser_match(parser, TOK_ASSIGN)) {
        ast_node_t* rvalue = parse_assignment(parser);
        if (!rvalue) {
            ast_node_destroy(expr);
            return NULL;
        }
        
        ast_node_t* node = ast_node_create(AST_ASSIGN);
        if (node) {
            node->data.assign.lvalue = expr;
            node->data.assign.rvalue = rvalue;
        }
        return node;
    }
    
    return expr;
}

/* Parse expression (entry point) */
static ast_node_t* parse_expression(parser_t* parser) {
    return parse_assignment(parser);
}       parser_consume(parser, TOK_RPAREN, "Expected ')'");
        return expr;
    }
    
    cc_error("Unexpected token in expression");
    parser->error_count++;
    return NULL;
}

/* Parse expression - simplified for now */
static ast_node_t* parse_expression(parser_t* parser) {
    return parse_primary(parser);
}

/* Parse statement */
static ast_node_t* parse_statement(parser_t* parser) {
    /* Return statement */
    if (parser_match(parser, TOK_RETURN)) {
        ast_node_t* node = ast_node_create(AST_RETURN_STMT);
        if (parser_current(parser)->type != TOK_SEMICOLON) {
            node->data.return_stmt.expr = parse_expression(parser);
        } else {
            node->data.return_stmt.expr = NULL;
        }
        parser_consume(parser, TOK_SEMICOLON, "Expected ';'");
        return node;
    }
    
    /* Compound statement */
    if (parser_match(parser, TOK_LBRACE)) {
        ast_node_t* node = ast_node_create(AST_COMPOUND_STMT);
        /* Parse statements until } */
        while (parser_current(parser)->type != TOK_RBRACE && 
               parser_current(parser)->type != TOK_EOF) {
            ast_node_t* stmt = parse_statement(parser);
            if (!stmt) break;
            /* For now, just discard individual statements */
        }
        parser_consume(parser, TOK_RBRACE, "Expected '}'");
        return node;
    }
    
    /* Expression statement */
    if (parser_current(parser)->type != TOK_SEMICOLON) {
        ast_node_t* expr = parse_expression(parser);
        parser_consume(parser, TOK_SEMICOLON, "Expected ';'");
        
        ast_node_t* node = ast_node_create(AST_EXPR_STMT);
        /* Store expr in node */
        return node;
    }
    
    return NULL;
}

/* Parse function definition */
static ast_node_t* parse_function(parser_t* parser) {
    ast_node_t* node = ast_node_create(AST_FUNCTION);
    if (!node) return NULL;
    
    /* Parse return type (simplified - assume int for now) */
    if (parser_match(parser, TOK_INT) || parser_match(parser, TOK_VOID)) {
        /* Got type */
    }
    
    /* Parse function name */
    token_t* name_tok = parser_current(parser);
    if (!parser_consume(parser, TOK_IDENTIFIER, "Expected function name")) {
        cc_free(node);
        return NULL;
    }
    
    node->data.function.name = cc_strdup(name_tok->value);
    
    /* Parse parameters */
    if (!parser_consume(parser, TOK_LPAREN, "Expected '('")) {
        cc_free(node);
        return NULL;
    }
    
    /* Skip parameters for now */
    while (parser_current(parser)->type != TOK_RPAREN && 
           parser_current(parser)->type != TOK_EOF) {
        parser_advance(parser);
    }
    
    if (!parser_consume(parser, TOK_RPAREN, "Expected ')'")) {
        cc_free(node);
        return NULL;
    }
    
    /* Parse function body */
    node->data.function.body = parse_statement(parser);
    
    return node;
}

/* Parse top-level declaration */
static ast_node_t* parse_declaration(parser_t* parser) {
    /* For now, assume everything is a function */
    return parse_function(parser);
}

/* Main parse function */
ast_node_t* parser_parse(parser_t* parser) {
    ast_node_t* program = ast_node_create(AST_PROGRAM);
    if (!program) return NULL;
    
    /* Parse all declarations */
    while (parser_current(parser)->type != TOK_EOF) {
        ast_node_t* decl = parse_declaration(parser);
        if (!decl) {
            if (parser->error_count == 0) {
                parser->error_count++;
            }
            break;
        }
        /* Add to program node */
    }
    
    return program;
}

void ast_node_destroy(ast_node_t* node) {
    if (!node) return;
    
    /* Free node-specific data */
    switch (node->type) {
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
        default:
            break;
    }
    
    /* Free the node itself */
    cc_free(node);
}

const char* ast_node_type_to_string(ast_node_type_t type) {
    switch (type) {
        case AST_PROGRAM: return "PROGRAM";
        case AST_FUNCTION: return "FUNCTION";
        case AST_IDENTIFIER: return "IDENTIFIER";
        case AST_CONSTANT: return "CONSTANT";
        case AST_RETURN_STMT: return "RETURN";
        default: return "UNKNOWN";
    }
}

#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"

/* Keep AST counters consistent across host/ZOS to expose limits early. */
typedef uint8_t ast_decl_count_t;
typedef uint8_t ast_param_count_t;
typedef uint8_t ast_stmt_count_t;
typedef uint8_t ast_arg_count_t;

/* AST node types */
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_VAR_DECL,
    AST_COMPOUND_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_GOTO_STMT,
    AST_LABEL_STMT,
    AST_ASSIGN,
    AST_CALL,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_IDENTIFIER,
    AST_CONSTANT,
    AST_STRING_LITERAL,
    AST_ARRAY_ACCESS
} ast_node_type_t;

/* Binary operators */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_LAND, OP_LOR
} binary_op_t;

/* Unary operators */
typedef enum {
    OP_NEG, OP_NOT, OP_LNOT, OP_ADDR, OP_DEREF,
    OP_PREINC, OP_PREDEC, OP_POSTINC, OP_POSTDEC
} unary_op_t;

/* AST node */
struct ast_node {
    ast_node_type_t type;
    
    union {
        struct {
            ast_node_t** declarations;
            ast_decl_count_t decl_count;
        } program;
        
        struct {
            char* name;
            type_t* return_type;
            ast_node_t** params;
            ast_param_count_t param_count;
            ast_node_t* body;
        } function;
        
        struct {
            ast_node_t** statements;
            ast_stmt_count_t stmt_count;
        } compound;
        
        struct {
            ast_node_t* condition;
            ast_node_t* then_branch;
            ast_node_t* else_branch;
        } if_stmt;
        
        struct {
            ast_node_t* condition;
            ast_node_t* body;
        } while_stmt;
        
        struct {
            ast_node_t* init;
            ast_node_t* condition;
            ast_node_t* increment;
            ast_node_t* body;
        } for_stmt;
        
        struct {
            ast_node_t* expr;
        } return_stmt;

        struct {
            char* label;
        } goto_stmt;

        struct {
            char* label;
        } label_stmt;

        struct {
            binary_op_t op;
            ast_node_t* left;
            ast_node_t* right;
        } binary_op;
        
        struct {
            unary_op_t op;
            ast_node_t* operand;
        } unary_op;
        
        struct {
            ast_node_t* lvalue;
            ast_node_t* rvalue;
        } assign;
        
        struct {
            char* name;
            ast_node_t** args;
            ast_arg_count_t arg_count;
        } call;
        
        struct {
            char* name;
        } identifier;
        
        struct {
            int16_t int_value;
        } constant;
        
        struct {
            char* value;
        } string_literal;

        struct {
            ast_node_t* base;
            ast_node_t* index;
        } array_access;

        struct {
            char* name;
            type_t* var_type;
            ast_node_t* initializer;
        } var_decl;
    } data;
};

/* Parser structure */
typedef struct {
    token_t* current;
    token_t* next;
    uint16_t error_count;
} parser_t;

extern parser_t* parser;

/* Parser functions */
parser_t* parser_create(void);
void parser_destroy(parser_t* parser);
ast_node_t* parser_parse(void);
ast_node_t* parser_parse_next(void);
void ast_node_destroy(ast_node_t* node);

#endif /* PARSER_H */

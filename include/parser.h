#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "lexer.h"

/* AST node types */
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_DECLARATION,
    AST_STATEMENT,
    AST_EXPRESSION,
    
    /* Statements */
    AST_COMPOUND_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RETURN_STMT,
    AST_EXPR_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    
    /* Expressions */
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_ASSIGN,
    AST_CALL,
    AST_CAST,
    AST_SIZEOF,
    AST_IDENTIFIER,
    AST_CONSTANT,
    AST_STRING_LITERAL,
    AST_ARRAY_ACCESS,
    AST_MEMBER_ACCESS,
    AST_POINTER_ACCESS,
    AST_TERNARY,
    
    /* Declarations */
    AST_VAR_DECL,
    AST_FUNC_DECL,
    AST_PARAM_DECL,
    AST_STRUCT_DECL,
    AST_ENUM_DECL,
    AST_TYPEDEF_DECL
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
    int line;
    int column;
    
    union {
        struct {
            ast_node_t** declarations;
            size_t decl_count;
        } program;
        
        struct {
            char* name;
            type_t* return_type;
            ast_node_t** params;
            size_t param_count;
            ast_node_t* body;
        } function;
        
        struct {
            ast_node_t** statements;
            size_t stmt_count;
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
            size_t arg_count;
        } call;
        
        struct {
            char* name;
        } identifier;
        
        struct {
            long long int_value;
            double float_value;
            bool is_float;
        } constant;
        
        struct {
            char* value;
        } string_literal;
        
        struct {
            char* name;
            type_t* var_type;
            ast_node_t* initializer;
        } var_decl;
    } data;
    
    type_t* expr_type;
    ast_node_t* next;
};

/* Parser structure */
typedef struct {
    token_t* tokens;
    token_t* current;
    int error_count;
} parser_t;

/* Parser functions */
parser_t* parser_create(token_t* tokens);
void parser_destroy(parser_t* parser);
ast_node_t* parser_parse(parser_t* parser);
void ast_node_destroy(ast_node_t* node);
const char* ast_node_type_to_string(ast_node_type_t type);

#endif /* PARSER_H */

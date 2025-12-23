#ifndef AST_FORMAT_H
#define AST_FORMAT_H

/* AST node tags for the binary format */
#define AST_TAG_PROGRAM 1
#define AST_TAG_FUNCTION 2
#define AST_TAG_VAR_DECL 3
#define AST_TAG_COMPOUND_STMT 4
#define AST_TAG_RETURN_STMT 5
#define AST_TAG_IF_STMT 6
#define AST_TAG_WHILE_STMT 7
#define AST_TAG_FOR_STMT 8
#define AST_TAG_ASSIGN 9
#define AST_TAG_CALL 10
#define AST_TAG_BINARY_OP 11
#define AST_TAG_UNARY_OP 12
#define AST_TAG_IDENTIFIER 13
#define AST_TAG_CONSTANT 14
#define AST_TAG_STRING_LITERAL 15
#define AST_TAG_ARRAY_ACCESS 16

/* AST binary header size in bytes */
#define AST_HEADER_SIZE 16

/* Type encoding base values */
#define AST_BASE_INT 1
#define AST_BASE_CHAR 2
#define AST_BASE_VOID 3

#endif /* AST_FORMAT_H */

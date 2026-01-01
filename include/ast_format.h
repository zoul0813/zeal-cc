#ifndef AST_FORMAT_H
#define AST_FORMAT_H

/* AST node tags for the binary format */
#define AST_TAG_PROGRAM 1
#define AST_TAG_FUNCTION 2
#define AST_TAG_VAR_DECL 3
#define AST_TAG_COMPOUND_STMT 4
#define AST_TAG_RETURN_STMT 5
#define AST_TAG_BREAK_STMT 6
#define AST_TAG_CONTINUE_STMT 7
#define AST_TAG_GOTO_STMT 8
#define AST_TAG_LABEL_STMT 9
#define AST_TAG_IF_STMT 10
#define AST_TAG_WHILE_STMT 11
#define AST_TAG_FOR_STMT 12
#define AST_TAG_ASSIGN 13
#define AST_TAG_CALL 14
#define AST_TAG_BINARY_OP 15
#define AST_TAG_UNARY_OP 16
#define AST_TAG_IDENTIFIER 17
#define AST_TAG_CONSTANT 18
#define AST_TAG_STRING_LITERAL 19
#define AST_TAG_ARRAY_ACCESS 20

/* AST binary header size in bytes */
#define AST_HEADER_SIZE 16

/* Type encoding base values */
#define AST_BASE_INT 1
#define AST_BASE_CHAR 2
#define AST_BASE_VOID 3
#define AST_BASE_FLAG_UNSIGNED 0x80
#define AST_BASE_MASK 0x7F

#endif /* AST_FORMAT_H */

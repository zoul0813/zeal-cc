#ifndef LEXER_H
#define LEXER_H

#include "common.h"
#include "target.h"

/* Token types */
typedef enum {
    /* End of file */
    TOK_EOF = 0,
    
    /* Literals */
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_CHAR,
    
    /* Keywords */
    TOK_AUTO,
    TOK_BREAK,
    TOK_CASE,
    TOK_CHAR_KW,
    TOK_CONST,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_DOUBLE,
    TOK_ELSE,
    TOK_ENUM,
    TOK_EXTERN,
    TOK_FLOAT,
    TOK_FOR,
    TOK_GOTO,
    TOK_IF,
    TOK_INT,
    TOK_LONG,
    TOK_REGISTER,
    TOK_RETURN,
    TOK_SHORT,
    TOK_SIGNED,
    TOK_SIZEOF,
    TOK_STATIC,
    TOK_STRUCT,
    TOK_SWITCH,
    TOK_TYPEDEF,
    TOK_UNION,
    TOK_UNSIGNED,
    TOK_VOID,
    TOK_VOLATILE,
    TOK_WHILE,
    
    /* Operators */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */
    TOK_AMPERSAND,      /* & */
    TOK_PIPE,           /* | */
    TOK_CARET,          /* ^ */
    TOK_TILDE,          /* ~ */
    TOK_EXCLAIM,        /* ! */
    TOK_ASSIGN,         /* = */
    TOK_LT,             /* < */
    TOK_GT,             /* > */
    TOK_PLUS_PLUS,      /* ++ */
    TOK_MINUS_MINUS,    /* -- */
    TOK_LSHIFT,         /* << */
    TOK_RSHIFT,         /* >> */
    TOK_EQ,             /* == */
    TOK_NE,             /* != */
    TOK_LE,             /* <= */
    TOK_GE,             /* >= */
    TOK_AND,            /* && */
    TOK_OR,             /* || */
    TOK_PLUS_ASSIGN,    /* += */
    TOK_MINUS_ASSIGN,   /* -= */
    TOK_STAR_ASSIGN,    /* *= */
    TOK_SLASH_ASSIGN,   /* /= */
    TOK_PERCENT_ASSIGN, /* %= */
    TOK_AND_ASSIGN,     /* &= */
    TOK_OR_ASSIGN,      /* |= */
    TOK_XOR_ASSIGN,     /* ^= */
    TOK_LSHIFT_ASSIGN,  /* <<= */
    TOK_RSHIFT_ASSIGN,  /* >>= */
    
    /* Punctuation */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_SEMICOLON,      /* ; */
    TOK_COMMA,          /* , */
    TOK_DOT,            /* . */
    TOK_ARROW,          /* -> */
    TOK_QUESTION,       /* ? */
    TOK_COLON,          /* : */
    
    /* Special */
    TOK_ERROR
} token_type_t;

/* Token structure */
struct token {
    token_type_t type;
    char* value;
    int line;
    int column;
    int16_t int_val;
    token_t* next;
};

/* Lexer structure */
typedef struct {
    const char* filename;
    reader_t* reader;
    int current_char;
    bool eof;
    int line;
    int column;
} lexer_t;

/* Lexer functions */
lexer_t* lexer_create(const char* filename, reader_t* reader);
void lexer_destroy(lexer_t* lexer);
token_t* lexer_next_token(lexer_t* lexer);
token_t* lexer_tokenize(lexer_t* lexer);
void token_destroy(token_t* token);
void token_list_destroy(token_t* head);
const char* token_type_to_string(token_type_t type);

#endif /* LEXER_H */

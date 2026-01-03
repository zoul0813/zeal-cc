#include "lexer.h"

#include <ctype.h>

#include "common.h"

/* Helper functions */
static void lexer_advance(void);
static void lexer_skip_whitespace(void);
static bool is_identifier_start(char c);
static bool is_identifier_char(char c);
static token_t* token_create(token_type_t type, const char* value, uint16_t line, uint16_t column);

/* Keyword table */
static const struct {
    const char* name;
    token_type_t type;
} keywords[] = {
    // {"auto", TOK_AUTO},
    {"break", TOK_BREAK},
    {"case", TOK_CASE},
    {"char", TOK_CHAR_KW},
    {"const", TOK_CONST},
    {"continue", TOK_CONTINUE},
    {"default", TOK_DEFAULT},
    {"do", TOK_DO},
    // {"double", TOK_DOUBLE},
    {"else", TOK_ELSE},
    // {"enum", TOK_ENUM},
    // {"extern", TOK_EXTERN},
    // {"float", TOK_FLOAT},
    {"for", TOK_FOR},
    {"goto", TOK_GOTO},
    {"if", TOK_IF},
    {"int", TOK_INT},
    {"long", TOK_LONG},
    // {"register", TOK_REGISTER},
    {"return", TOK_RETURN},
    // {"short", TOK_SHORT},
    {"signed", TOK_SIGNED},
    // {"sizeof", TOK_SIZEOF},
    // {"static", TOK_STATIC},
    // {"struct", TOK_STRUCT},
    {"switch", TOK_SWITCH},
    // {"typedef", TOK_TYPEDEF},
    // {"union", TOK_UNION},
    {"unsigned", TOK_UNSIGNED},
    {"void", TOK_VOID},
    // {"volatile", TOK_VOLATILE},
    {"while", TOK_WHILE},
    {NULL, TOK_EOF}};

lexer_t* lexer_create(const char* filename) {
    lexer = (lexer_t*)cc_malloc(sizeof(lexer_t));
    if (!lexer) return NULL;

    lexer->filename = filename;
    lexer->line = 1;
    lexer->column = 1;
    lexer->eof = false;
    lexer->current_char = reader_next(reader);
    if (lexer->current_char < 0) {
        lexer->current_char = '\0';
        lexer->eof = true;
    }

    return lexer;
}

void lexer_destroy(lexer_t* lexer) {
    if (lexer) {
        cc_free(lexer);
    }
}

static void lexer_advance(void) {
    if (lexer->eof) return;
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    int16_t next = reader_next(reader);
    if (next < 0) {
        lexer->current_char = '\0';
        lexer->eof = true;
    } else {
        lexer->current_char = next;
    }
}

static char lexer_peek(uint8_t offset) {
    if (lexer->eof) return '\0';
    if (offset == 1) {
        int16_t c = reader_peek(reader);
        if (c < 0) return '\0';
        return (char)c;
    }
    /* Only simple lookahead of 1 is supported in streaming mode */
    return '\0';
}

static void lexer_skip_whitespace(void) {
    while (lexer->current_char == ' ' ||
           lexer->current_char == '\t' ||
           lexer->current_char == '\n' ||
           lexer->current_char == '\r') {
        lexer_advance();
    }
}

static void lexer_skip_line_comment(void) {
    /* Skip // */
    lexer_advance();
    lexer_advance();

    while (lexer->current_char != '\n' && lexer->current_char != '\0') {
        lexer_advance();
    }
}

static void lexer_skip_block_comment(void) {
    /* Skip the opening slash-star */
    lexer_advance();
    lexer_advance();

    while (lexer->current_char != '\0') {
        if (lexer->current_char == '*' && lexer_peek(1) == '/') {
            lexer_advance(); /* Skip * */
            lexer_advance(); /* Skip / */
            break;
        }
        lexer_advance();
    }
}

static bool is_identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_identifier_char(char c) {
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

static token_t* token_create(token_type_t type, const char* value, uint16_t line, uint16_t column) {
    token_t* token = (token_t*)cc_malloc(sizeof(token_t));
    if (!token) return NULL;

    token->type = type;
    token->line = line;
    token->column = column;
    token->next = NULL;
    token->value = NULL;

    if (type == TOK_IDENTIFIER || type == TOK_STRING) {
        token->value = cc_strdup(value ? value : "");
    }

    return token;
}

static token_t* lexer_read_number(void) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_TOKEN_LENGTH];
    uint8_t len = 0;
    bool is_hex = false;

    /* Check for hex prefix */
    if (lexer->current_char == '0' &&
        (lexer_peek(1) == 'x' || lexer_peek(1) == 'X')) {
        is_hex = true;
        buffer[len++] = lexer->current_char;
        lexer_advance();
        buffer[len++] = lexer->current_char;
        lexer_advance();
    }

    /* Read digits */
    while (len < MAX_TOKEN_LENGTH - 1) {
        if (is_hex) {
            if ((lexer->current_char >= '0' && lexer->current_char <= '9') ||
                (lexer->current_char >= 'a' && lexer->current_char <= 'f') ||
                (lexer->current_char >= 'A' && lexer->current_char <= 'F')) {
                buffer[len++] = lexer->current_char;
                lexer_advance();
            } else {
                break;
            }
        } else {
            if (lexer->current_char >= '0' && lexer->current_char <= '9') {
                buffer[len++] = lexer->current_char;
                lexer_advance();
            } else if (lexer->current_char == '.') {
                buffer[len++] = lexer->current_char;
                lexer_advance();
            } else if (lexer->current_char == 'e' || lexer->current_char == 'E') {
                buffer[len++] = lexer->current_char;
                lexer_advance();
                if (lexer->current_char == '+' || lexer->current_char == '-') {
                    buffer[len++] = lexer->current_char;
                    lexer_advance();
                }
            } else {
                break;
            }
        }
    }

    /* Skip suffix (L, U, f, etc.) */
    while (lexer->current_char == 'L' || lexer->current_char == 'l' ||
           lexer->current_char == 'U' || lexer->current_char == 'u' ||
           lexer->current_char == 'F' || lexer->current_char == 'f') {
        lexer_advance();
    }

    buffer[len] = '\0';

    token_t* token = token_create(TOK_NUMBER, buffer, start_line, start_column);
    if (token) {
        /* Parse integer */
        int32_t val = 0;
        uint8_t i = 0;

        if (is_hex) {
            i = 2; /* Skip 0x */
            while (i < len) {
                val = (int32_t)(val * 16);
                char c = buffer[i];
                if (c >= '0' && c <= '9') {
                    val += (int32_t)(c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    val += (int32_t)(c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    val += (int32_t)(c - 'A' + 10);
                }
                i++;
            }
        } else {
            while (i < len && buffer[i] >= '0' && buffer[i] <= '9') {
                val = (int32_t)(val * 10 + (buffer[i] - '0'));
                i++;
            }
        }

        token->int_val = (int16_t)val;
    }

    return token;
}

static token_t* lexer_read_identifier(void) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_IDENTIFIER_LENGTH];
    uint8_t len = 0;

    while (len < MAX_IDENTIFIER_LENGTH - 1 && is_identifier_char(lexer->current_char)) {
        buffer[len++] = lexer->current_char;
        lexer_advance();
    }
    buffer[len] = '\0';

    /* Check if it's a keyword */
    for (uint8_t i = 0; keywords[i].name != NULL; i++) {
        bool match = true;
        for (uint8_t j = 0; keywords[i].name[j] != '\0' || buffer[j] != '\0'; j++) {
            if (keywords[i].name[j] != buffer[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return token_create(keywords[i].type, buffer, start_line, start_column);
        }
    }

    return token_create(TOK_IDENTIFIER, buffer, start_line, start_column);
}

typedef struct {
    char esc;
    char value;
} lexer_escape_t;

static char lexer_unescape_char(char c) {
    static const lexer_escape_t escapes[] = {
        { 'n', '\n' },
        { 't', '\t' },
        { 'r', '\r' },
        { '\\', '\\' },
        { '"', '"' },
        { '\'', '\'' },
        { '0', '\0' }
    };
    for (uint8_t i = 0; i < (uint8_t)(sizeof(escapes) / sizeof(escapes[0])); i++) {
        if (escapes[i].esc == c) {
            return escapes[i].value;
        }
    }
    return c;
}

static token_t* lexer_read_string(void) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_STRING_LENGTH];
    uint8_t len = 0;

    /* Skip opening quote */
    lexer_advance();

    while (lexer->current_char != '"' && lexer->current_char != '\0' &&
           len < MAX_STRING_LENGTH - 1) {
        if (lexer->current_char == '\\') {
            lexer_advance();
            buffer[len++] = lexer_unescape_char(lexer->current_char);
            lexer_advance();
        } else {
            buffer[len++] = lexer->current_char;
            lexer_advance();
        }
    }

    if (lexer->current_char == '"') {
        lexer_advance();
    }

    buffer[len] = '\0';
    return token_create(TOK_STRING, buffer, start_line, start_column);
}

static token_t* lexer_read_char(void) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[8];

    /* Skip opening quote */
    lexer_advance();

    char c = lexer->current_char;
    if (c == '\\') {
        lexer_advance();
        c = lexer_unescape_char(lexer->current_char);
    }
    lexer_advance();

    if (lexer->current_char == '\'') {
        lexer_advance();
    }

    buffer[0] = c;
    buffer[1] = '\0';

    token_t* token = token_create(TOK_CHAR, NULL, start_line, start_column);
    if (token) {
        token->int_val = (int16_t)c;
    }
    return token;
}

token_t* lexer_next_token(void) {
    static const struct {
        char ch;
        token_type_t type;
    } k_single_tokens[] = {
        { '(', TOK_LPAREN },
        { ')', TOK_RPAREN },
        { '{', TOK_LBRACE },
        { '}', TOK_RBRACE },
        { '[', TOK_LBRACKET },
        { ']', TOK_RBRACKET },
        { ';', TOK_SEMICOLON },
        { ',', TOK_COMMA },
        { '.', TOK_DOT },
        { '~', TOK_TILDE },
        { '?', TOK_QUESTION },
        { ':', TOK_COLON }
    };
    static const struct {
        char ch;
        token_type_t single;
        token_type_t next_same;
        token_type_t next_eq;
        token_type_t next_alt;
        char alt_ch;
    } k_two_char_ops[] = {
        { '+', TOK_PLUS, TOK_PLUS_PLUS, TOK_PLUS_ASSIGN, TOK_EOF, 0 },
        { '-', TOK_MINUS, TOK_MINUS_MINUS, TOK_MINUS_ASSIGN, TOK_ARROW, '>' },
        { '*', TOK_STAR, TOK_EOF, TOK_STAR_ASSIGN, TOK_EOF, 0 },
        { '/', TOK_SLASH, TOK_EOF, TOK_SLASH_ASSIGN, TOK_EOF, 0 },
        { '%', TOK_PERCENT, TOK_EOF, TOK_PERCENT_ASSIGN, TOK_EOF, 0 },
        { '&', TOK_AMPERSAND, TOK_AND, TOK_AND_ASSIGN, TOK_EOF, 0 },
        { '|', TOK_PIPE, TOK_OR, TOK_OR_ASSIGN, TOK_EOF, 0 },
        { '^', TOK_CARET, TOK_EOF, TOK_XOR_ASSIGN, TOK_EOF, 0 },
        { '=', TOK_ASSIGN, TOK_EOF, TOK_EQ, TOK_EOF, 0 },
        { '!', TOK_EXCLAIM, TOK_EOF, TOK_NE, TOK_EOF, 0 }
    };
    static const struct {
        char ch;
        token_type_t single;
        token_type_t eq;
        token_type_t shift;
        token_type_t shift_assign;
    } k_shift_ops[] = {
        { '<', TOK_LT, TOK_LE, TOK_LSHIFT, TOK_LSHIFT_ASSIGN },
        { '>', TOK_GT, TOK_GE, TOK_RSHIFT, TOK_RSHIFT_ASSIGN }
    };

    while (lexer->current_char != '\0') {
        /* Skip whitespace */
        if (lexer->current_char == ' ' || lexer->current_char == '\t' ||
            lexer->current_char == '\n' || lexer->current_char == '\r') {
            lexer_skip_whitespace();
            continue;
        }

        /* Skip comments */
        if (lexer->current_char == '/' && lexer_peek(1) == '/') {
            lexer_skip_line_comment();
            continue;
        }

        if (lexer->current_char == '/' && lexer_peek(1) == '*') {
            lexer_skip_block_comment();
            continue;
        }

        uint16_t start_line = lexer->line;
        uint16_t start_column = lexer->column;
        char c = lexer->current_char;

        /* Numbers */
        if ((c >= '0' && c <= '9')) {
            return lexer_read_number();
        }

        /* Identifiers and keywords */
        if (is_identifier_start(c)) {
            return lexer_read_identifier();
        }

        /* Strings */
        if (c == '"') {
            return lexer_read_string();
        }

        /* Characters */
        if (c == '\'') {
            return lexer_read_char();
        }

        /* Two-character operators */
        char next = lexer_peek(1);

        for (uint8_t i = 0; i < (uint8_t)(sizeof(k_two_char_ops) / sizeof(k_two_char_ops[0])); i++) {
            if (k_two_char_ops[i].ch != c) {
                continue;
            }
            lexer_advance();
            if (k_two_char_ops[i].alt_ch && next == k_two_char_ops[i].alt_ch) {
                lexer_advance();
                return token_create(k_two_char_ops[i].next_alt, NULL, start_line, start_column);
            }
            if (k_two_char_ops[i].next_same != TOK_EOF && next == c) {
                lexer_advance();
                return token_create(k_two_char_ops[i].next_same, NULL, start_line, start_column);
            }
            if (k_two_char_ops[i].next_eq != TOK_EOF && next == '=') {
                lexer_advance();
                return token_create(k_two_char_ops[i].next_eq, NULL, start_line, start_column);
            }
            return token_create(k_two_char_ops[i].single, NULL, start_line, start_column);
        }

        for (uint8_t i = 0; i < (uint8_t)(sizeof(k_shift_ops) / sizeof(k_shift_ops[0])); i++) {
            if (k_shift_ops[i].ch != c) {
                continue;
            }
            lexer_advance();
            if (next == c) {
                lexer_advance();
                if (lexer->current_char == '=') {
                    lexer_advance();
                    return token_create(k_shift_ops[i].shift_assign, NULL, start_line, start_column);
                }
                return token_create(k_shift_ops[i].shift, NULL, start_line, start_column);
            }
            if (next == '=') {
                lexer_advance();
                return token_create(k_shift_ops[i].eq, NULL, start_line, start_column);
            }
            return token_create(k_shift_ops[i].single, NULL, start_line, start_column);
        }

        /* Single-character tokens */
        lexer_advance();
        for (uint8_t i = 0; i < (uint8_t)(sizeof(k_single_tokens) / sizeof(k_single_tokens[0])); i++) {
            if (k_single_tokens[i].ch == c) {
                return token_create(k_single_tokens[i].type,
                                    NULL,
                                    start_line,
                                    start_column);
            }
        }
        return token_create(TOK_ERROR, NULL, start_line, start_column);
    }

    return token_create(TOK_EOF, NULL, lexer->line, lexer->column);
}

void token_destroy(token_t* token) {
    if (token) {
        if (token->value) {
            cc_free(token->value);
        }
        cc_free(token);
    }
}

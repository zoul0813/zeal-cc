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
    lexer->failed = false;
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

static bool lexer_skip_block_comment(void) {
    /* Skip the opening slash-star */
    lexer_advance();
    lexer_advance();

    while (lexer->current_char != '\0') {
        if (lexer->current_char == '*' && lexer_peek(1) == '/') {
            lexer_advance(); /* Skip * */
            lexer_advance(); /* Skip / */
            return true;
        }
        lexer_advance();
    }
    return false;
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
    token->int_val = 0;
    token->value = NULL;

    if (type == TOK_IDENTIFIER || type == TOK_STRING) {
        token->value = cc_strdup(value ? value : "");
    }

    return token;
}

static token_t* lexer_error(const char* message, uint16_t line, uint16_t column) {
    log_error("Lex error: ");
    log_error(message);
    log_error("\n");
    lexer->failed = true;
    lexer->eof = true;
    lexer->current_char = '\0';
    return token_create(TOK_ERROR, NULL, line, column);
}

static uint8_t digit_value(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 255;
}

static token_t* lexer_read_number(void) {
    uint16_t line = lexer->line;
    uint16_t column = lexer->column;
    uint16_t value = 0;
    uint8_t base = 10;
    uint8_t length = 0;
    uint8_t digits = 0;
    if (lexer->current_char == '0') {
        base = 8;
        if (lexer_peek(1) == 'x' || lexer_peek(1) == 'X') {
            base = 16;
            lexer_advance();
            lexer_advance();
            length = 2;
        }
    }
    while (1) {
        uint8_t digit = digit_value((char)lexer->current_char);
        if (digit >= base) break;
        if (++length >= MAX_TOKEN_LENGTH) return lexer_error("Numeric token too long", line, column);
        if (value > (UINT16_MAX - digit) / base) return lexer_error("Integer exceeds 16-bit range", line, column);
        value = (uint16_t)(value * base + digit);
        digits++;
        lexer_advance();
    }
    if (!digits) return lexer_error("Expected digits after numeric prefix", line, column);
    /* The implemented integer model is unsigned by default; long/float suffixes are unsupported. */
    if (lexer->current_char == 'u' || lexer->current_char == 'U') {
        if (++length >= MAX_TOKEN_LENGTH) return lexer_error("Numeric token too long", line, column);
        lexer_advance();
    }
    if (is_identifier_char((char)lexer->current_char) || lexer->current_char == '.') {
        return lexer_error("Invalid or unsupported integer literal", line, column);
    }
    token_t* token = token_create(TOK_NUMBER, NULL, line, column);
    if (token) token->int_val = (int16_t)value;
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
    if (is_identifier_char((char)lexer->current_char)) return lexer_error("Identifier too long", start_line, start_column);
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

static bool lexer_unescape_char(char c, char* out) {
    static const lexer_escape_t escapes[] = {
        { 'n', '\n' }, { 't', '\t' }, { 'r', '\r' },
        { '\\', '\\' }, { '"', '"' }, { '\'', '\'' }, { '0', '\0' }
    };
    for (uint8_t i = 0; i < DIM(escapes); i++) {
        if (escapes[i].esc == c) { *out = escapes[i].value; return true; }
    }
    return false;
}

static token_t* lexer_read_string(void) {
    uint16_t line = lexer->line;
    uint16_t column = lexer->column;
    char buffer[MAX_STRING_LENGTH];
    uint8_t length = 0;
    lexer_advance();
    while (lexer->current_char != '"') {
        if (lexer->eof || lexer->current_char == '\0' || lexer->current_char == '\n' || lexer->current_char == '\r')
            return lexer_error("Unterminated string literal", line, column);
        char c = (char)lexer->current_char;
        if (c == '\\') {
            lexer_advance();
            if (!lexer_unescape_char((char)lexer->current_char, &c))
                return lexer_error("Unsupported string escape", line, column);
            if (c == '\0') return lexer_error("Embedded NUL strings are unsupported", line, column);
        }
        if (length >= MAX_STRING_LENGTH - 1) return lexer_error("String literal too long", line, column);
        buffer[length++] = c;
        lexer_advance();
    }
    lexer_advance();
    buffer[length] = '\0';
    return token_create(TOK_STRING, buffer, line, column);
}

static token_t* lexer_read_char(void) {
    uint16_t line = lexer->line;
    uint16_t column = lexer->column;
    lexer_advance();
    if (lexer->eof || lexer->current_char == '\0' || lexer->current_char == '\n' || lexer->current_char == '\r' || lexer->current_char == '\'')
        return lexer_error("Invalid character literal", line, column);
    char c = (char)lexer->current_char;
    if (c == '\\') {
        lexer_advance();
        if (!lexer_unescape_char((char)lexer->current_char, &c))
            return lexer_error("Unsupported character escape", line, column);
    }
    lexer_advance();
    if (lexer->current_char != '\'') return lexer_error("Expected closing character quote", line, column);
    lexer_advance();
    token_t* token = token_create(TOK_CHAR, NULL, line, column);
    if (token) token->int_val = (uint8_t)c;
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
            if (!lexer_skip_block_comment()) return lexer_error("Unterminated block comment", lexer->line, lexer->column);
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
        return lexer_error("Unsupported source character", start_line, start_column);
    }

    if (!lexer->eof) return lexer_error("NUL byte in source", lexer->line, lexer->column);
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

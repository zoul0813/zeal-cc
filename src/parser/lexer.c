#include "lexer.h"

#include <ctype.h>

#include "common.h"

/* Helper functions */
static void lexer_advance(lexer_t* lexer);
static void lexer_skip_whitespace(lexer_t* lexer);
static bool is_identifier_start(char c);
static bool is_identifier_char(char c);
static token_t* token_create(token_type_t type, const char* value, uint16_t line, uint16_t column);

/* Keyword table */
static const struct {
    const char* name;
    token_type_t type;
} keywords[] = {
    {"auto", TOK_AUTO},
    {"break", TOK_BREAK},
    {"case", TOK_CASE},
    {"char", TOK_CHAR_KW},
    {"const", TOK_CONST},
    {"continue", TOK_CONTINUE},
    {"default", TOK_DEFAULT},
    {"do", TOK_DO},
    {"double", TOK_DOUBLE},
    {"else", TOK_ELSE},
    {"enum", TOK_ENUM},
    {"extern", TOK_EXTERN},
    {"float", TOK_FLOAT},
    {"for", TOK_FOR},
    {"goto", TOK_GOTO},
    {"if", TOK_IF},
    {"int", TOK_INT},
    {"long", TOK_LONG},
    {"register", TOK_REGISTER},
    {"return", TOK_RETURN},
    {"short", TOK_SHORT},
    {"signed", TOK_SIGNED},
    {"sizeof", TOK_SIZEOF},
    {"static", TOK_STATIC},
    {"struct", TOK_STRUCT},
    {"switch", TOK_SWITCH},
    {"typedef", TOK_TYPEDEF},
    {"union", TOK_UNION},
    {"unsigned", TOK_UNSIGNED},
    {"void", TOK_VOID},
    {"volatile", TOK_VOLATILE},
    {"while", TOK_WHILE},
    {NULL, TOK_EOF}};

lexer_t* lexer_create(const char* filename, reader_t* reader) {
    lexer_t* lexer = (lexer_t*)cc_malloc(sizeof(lexer_t));
    if (!lexer) return NULL;

    lexer->filename = filename;
    lexer->reader = reader;
    lexer->line = 1;
    lexer->column = 1;
    lexer->eof = false;
    lexer->current_char = reader_next(lexer->reader);
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

static void lexer_advance(lexer_t* lexer) {
    if (lexer->eof) return;
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    int16_t next = reader_next(lexer->reader);
    if (next < 0) {
        lexer->current_char = '\0';
        lexer->eof = true;
    } else {
        lexer->current_char = next;
    }
}

static char lexer_peek(lexer_t* lexer, uint8_t offset) {
    if (lexer->eof) return '\0';
    if (offset == 1) {
        int16_t c = reader_peek(lexer->reader);
        if (c < 0) return '\0';
        return (char)c;
    }
    /* Only simple lookahead of 1 is supported in streaming mode */
    return '\0';
}

static void lexer_skip_whitespace(lexer_t* lexer) {
    while (lexer->current_char == ' ' ||
           lexer->current_char == '\t' ||
           lexer->current_char == '\n' ||
           lexer->current_char == '\r') {
        lexer_advance(lexer);
    }
}

static void lexer_skip_line_comment(lexer_t* lexer) {
    /* Skip // */
    lexer_advance(lexer);
    lexer_advance(lexer);

    while (lexer->current_char != '\n' && lexer->current_char != '\0') {
        lexer_advance(lexer);
    }
}

static void lexer_skip_block_comment(lexer_t* lexer) {
    /* Skip the opening slash-star */
    lexer_advance(lexer);
    lexer_advance(lexer);

    while (lexer->current_char != '\0') {
        if (lexer->current_char == '*' && lexer_peek(lexer, 1) == '/') {
            lexer_advance(lexer); /* Skip * */
            lexer_advance(lexer); /* Skip / */
            break;
        }
        lexer_advance(lexer);
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

    if (type == TOK_IDENTIFIER || type == TOK_STRING || type == TOK_CHAR) {
        token->value = cc_strdup(value ? value : "");
    }

    return token;
}

static token_t* lexer_read_number(lexer_t* lexer) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_TOKEN_LENGTH];
    size_t len = 0;
    bool is_hex = false;

    /* Check for hex prefix */
    if (lexer->current_char == '0' &&
        (lexer_peek(lexer, 1) == 'x' || lexer_peek(lexer, 1) == 'X')) {
        is_hex = true;
        buffer[len++] = lexer->current_char;
        lexer_advance(lexer);
        buffer[len++] = lexer->current_char;
        lexer_advance(lexer);
    }

    /* Read digits */
    while (len < MAX_TOKEN_LENGTH - 1) {
        if (is_hex) {
            if ((lexer->current_char >= '0' && lexer->current_char <= '9') ||
                (lexer->current_char >= 'a' && lexer->current_char <= 'f') ||
                (lexer->current_char >= 'A' && lexer->current_char <= 'F')) {
                buffer[len++] = lexer->current_char;
                lexer_advance(lexer);
            } else {
                break;
            }
        } else {
            if (lexer->current_char >= '0' && lexer->current_char <= '9') {
                buffer[len++] = lexer->current_char;
                lexer_advance(lexer);
            } else if (lexer->current_char == '.') {
                buffer[len++] = lexer->current_char;
                lexer_advance(lexer);
            } else if (lexer->current_char == 'e' || lexer->current_char == 'E') {
                buffer[len++] = lexer->current_char;
                lexer_advance(lexer);
                if (lexer->current_char == '+' || lexer->current_char == '-') {
                    buffer[len++] = lexer->current_char;
                    lexer_advance(lexer);
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
        lexer_advance(lexer);
    }

    buffer[len] = '\0';

    token_t* token = token_create(TOK_NUMBER, buffer, start_line, start_column);
    if (token) {
        /* Parse integer */
        int32_t val = 0;
        size_t i = 0;

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

static token_t* lexer_read_identifier(lexer_t* lexer) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_IDENTIFIER_LENGTH];
    size_t len = 0;

    while (len < MAX_IDENTIFIER_LENGTH - 1 && is_identifier_char(lexer->current_char)) {
        buffer[len++] = lexer->current_char;
        lexer_advance(lexer);
    }
    buffer[len] = '\0';

    /* Check if it's a keyword */
    for (uint16_t i = 0; keywords[i].name != NULL; i++) {
        bool match = true;
        for (size_t j = 0; keywords[i].name[j] != '\0' || buffer[j] != '\0'; j++) {
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

static token_t* lexer_read_string(lexer_t* lexer) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[MAX_STRING_LENGTH];
    size_t len = 0;

    /* Skip opening quote */
    lexer_advance(lexer);

    while (lexer->current_char != '"' && lexer->current_char != '\0' &&
           len < MAX_STRING_LENGTH - 1) {
        if (lexer->current_char == '\\') {
            lexer_advance(lexer);
            switch (lexer->current_char) {
                case 'n':
                    buffer[len++] = '\n';
                    break;
                case 't':
                    buffer[len++] = '\t';
                    break;
                case 'r':
                    buffer[len++] = '\r';
                    break;
                case '\\':
                    buffer[len++] = '\\';
                    break;
                case '"':
                    buffer[len++] = '"';
                    break;
                case '0':
                    buffer[len++] = '\0';
                    break;
                default:
                    buffer[len++] = lexer->current_char;
                    break;
            }
            lexer_advance(lexer);
        } else {
            buffer[len++] = lexer->current_char;
            lexer_advance(lexer);
        }
    }

    if (lexer->current_char == '"') {
        lexer_advance(lexer);
    }

    buffer[len] = '\0';
    return token_create(TOK_STRING, buffer, start_line, start_column);
}

static token_t* lexer_read_char(lexer_t* lexer) {
    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;
    char buffer[8];

    /* Skip opening quote */
    lexer_advance(lexer);

    char c = lexer->current_char;
    if (c == '\\') {
        lexer_advance(lexer);
        switch (lexer->current_char) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '\'':
                c = '\'';
                break;
            case '0':
                c = '\0';
                break;
            default:
                c = lexer->current_char;
                break;
        }
    }
    lexer_advance(lexer);

    if (lexer->current_char == '\'') {
        lexer_advance(lexer);
    }

    buffer[0] = c;
    buffer[1] = '\0';

    token_t* token = token_create(TOK_CHAR, buffer, start_line, start_column);
    if (token) {
        token->int_val = (int16_t)c;
    }
    return token;
}

token_t* lexer_next_token(lexer_t* lexer) {
    while (lexer->current_char != '\0') {
        /* Skip whitespace */
        if (lexer->current_char == ' ' || lexer->current_char == '\t' ||
            lexer->current_char == '\n' || lexer->current_char == '\r') {
            lexer_skip_whitespace(lexer);
            continue;
        }

        /* Skip comments */
        if (lexer->current_char == '/' && lexer_peek(lexer, 1) == '/') {
            lexer_skip_line_comment(lexer);
            continue;
        }

        if (lexer->current_char == '/' && lexer_peek(lexer, 1) == '*') {
            lexer_skip_block_comment(lexer);
            continue;
        }

        uint16_t start_line = lexer->line;
        uint16_t start_column = lexer->column;
        char c = lexer->current_char;

        /* Numbers */
        if ((c >= '0' && c <= '9')) {
            return lexer_read_number(lexer);
        }

        /* Identifiers and keywords */
        if (is_identifier_start(c)) {
            return lexer_read_identifier(lexer);
        }

        /* Strings */
        if (c == '"') {
            return lexer_read_string(lexer);
        }

        /* Characters */
        if (c == '\'') {
            return lexer_read_char(lexer);
        }

        /* Two-character operators */
        char next = lexer_peek(lexer, 1);

        if (c == '+') {
            lexer_advance(lexer);
            if (next == '+') {
                lexer_advance(lexer);
                return token_create(TOK_PLUS_PLUS, "++", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_PLUS_ASSIGN, "+=", start_line, start_column);
            }
            return token_create(TOK_PLUS, "+", start_line, start_column);
        }

        if (c == '-') {
            lexer_advance(lexer);
            if (next == '-') {
                lexer_advance(lexer);
                return token_create(TOK_MINUS_MINUS, "--", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_MINUS_ASSIGN, "-=", start_line, start_column);
            } else if (next == '>') {
                lexer_advance(lexer);
                return token_create(TOK_ARROW, "->", start_line, start_column);
            }
            return token_create(TOK_MINUS, "-", start_line, start_column);
        }

        if (c == '*') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_STAR_ASSIGN, "*=", start_line, start_column);
            }
            return token_create(TOK_STAR, "*", start_line, start_column);
        }

        if (c == '/') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_SLASH_ASSIGN, "/=", start_line, start_column);
            }
            return token_create(TOK_SLASH, "/", start_line, start_column);
        }

        if (c == '%') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_PERCENT_ASSIGN, "%=", start_line, start_column);
            }
            return token_create(TOK_PERCENT, "%", start_line, start_column);
        }

        if (c == '&') {
            lexer_advance(lexer);
            if (next == '&') {
                lexer_advance(lexer);
                return token_create(TOK_AND, "&&", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_AND_ASSIGN, "&=", start_line, start_column);
            }
            return token_create(TOK_AMPERSAND, "&", start_line, start_column);
        }

        if (c == '|') {
            lexer_advance(lexer);
            if (next == '|') {
                lexer_advance(lexer);
                return token_create(TOK_OR, "||", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_OR_ASSIGN, "|=", start_line, start_column);
            }
            return token_create(TOK_PIPE, "|", start_line, start_column);
        }

        if (c == '^') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_XOR_ASSIGN, "^=", start_line, start_column);
            }
            return token_create(TOK_CARET, "^", start_line, start_column);
        }

        if (c == '<') {
            lexer_advance(lexer);
            if (next == '<') {
                lexer_advance(lexer);
                if (lexer->current_char == '=') {
                    lexer_advance(lexer);
                    return token_create(TOK_LSHIFT_ASSIGN, "<<=", start_line, start_column);
                }
                return token_create(TOK_LSHIFT, "<<", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_LE, "<=", start_line, start_column);
            }
            return token_create(TOK_LT, "<", start_line, start_column);
        }

        if (c == '>') {
            lexer_advance(lexer);
            if (next == '>') {
                lexer_advance(lexer);
                if (lexer->current_char == '=') {
                    lexer_advance(lexer);
                    return token_create(TOK_RSHIFT_ASSIGN, ">>=", start_line, start_column);
                }
                return token_create(TOK_RSHIFT, ">>", start_line, start_column);
            } else if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_GE, ">=", start_line, start_column);
            }
            return token_create(TOK_GT, ">", start_line, start_column);
        }

        if (c == '=') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_EQ, "==", start_line, start_column);
            }
            return token_create(TOK_ASSIGN, "=", start_line, start_column);
        }

        if (c == '!') {
            lexer_advance(lexer);
            if (next == '=') {
                lexer_advance(lexer);
                return token_create(TOK_NE, "!=", start_line, start_column);
            }
            return token_create(TOK_EXCLAIM, "!", start_line, start_column);
        }

        /* Single-character tokens */
        lexer_advance(lexer);
        switch (c) {
            case '(':
                return token_create(TOK_LPAREN, "(", start_line, start_column);
            case ')':
                return token_create(TOK_RPAREN, ")", start_line, start_column);
            case '{':
                return token_create(TOK_LBRACE, "{", start_line, start_column);
            case '}':
                return token_create(TOK_RBRACE, "}", start_line, start_column);
            case '[':
                return token_create(TOK_LBRACKET, "[", start_line, start_column);
            case ']':
                return token_create(TOK_RBRACKET, "]", start_line, start_column);
            case ';':
                return token_create(TOK_SEMICOLON, ";", start_line, start_column);
            case ',':
                return token_create(TOK_COMMA, ",", start_line, start_column);
            case '.':
                return token_create(TOK_DOT, ".", start_line, start_column);
            case '~':
                return token_create(TOK_TILDE, "~", start_line, start_column);
            case '?':
                return token_create(TOK_QUESTION, "?", start_line, start_column);
            case ':':
                return token_create(TOK_COLON, ":", start_line, start_column);
            default:
                return token_create(TOK_ERROR, "", start_line, start_column);
        }
    }

    return token_create(TOK_EOF, "", lexer->line, lexer->column);
}

token_t* lexer_tokenize(lexer_t* lexer) {
    token_t* head = NULL;
    token_t* tail = NULL;

    while (true) {
        token_t* token = lexer_next_token(lexer);
        if (!token) {
            break;
        }

        if (!head) {
            head = token;
            tail = token;
        } else {
            tail->next = token;
            tail = token;
        }

        if (token->type == TOK_EOF || token->type == TOK_ERROR) {
            break;
        }
    }

    return head;
}

void token_destroy(token_t* token) {
    if (token) {
        if (token->value) {
            cc_free(token->value);
        }
        cc_free(token);
    }
}

void token_list_destroy(token_t* head) {
    while (head) {
        token_t* next = head->next;
        token_destroy(head);
        head = next;
    }
}

const char* token_type_to_string(token_type_t type) {
    switch (type) {
        case TOK_EOF:
            return "EOF";
        case TOK_IDENTIFIER:
            return "IDENTIFIER";
        case TOK_NUMBER:
            return "NUMBER";
        case TOK_STRING:
            return "STRING";
        case TOK_CHAR:
            return "CHAR";
        case TOK_INT:
            return "int";
        case TOK_RETURN:
            return "return";
        case TOK_IF:
            return "if";
        case TOK_ELSE:
            return "else";
        case TOK_WHILE:
            return "while";
        case TOK_FOR:
            return "for";
        case TOK_VOID:
            return "void";
        case TOK_SEMICOLON:
            return ";";
        case TOK_LPAREN:
            return "(";
        case TOK_RPAREN:
            return ")";
        case TOK_LBRACE:
            return "{";
        case TOK_RBRACE:
            return "}";
        case TOK_COMMA:
            return ",";
        case TOK_PLUS:
            return "+";
        case TOK_MINUS:
            return "-";
        case TOK_STAR:
            return "*";
        case TOK_SLASH:
            return "/";
        case TOK_ASSIGN:
            return "=";
        case TOK_LT:
            return "<";
        case TOK_GT:
            return ">";
        case TOK_EQ:
            return "==";
        case TOK_NE:
            return "!=";
        default:
            return "UNKNOWN";
    }
}

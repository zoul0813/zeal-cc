#include "ast_reader.h"
#include "ast_io.h"
#include "common.h"

int8_t ast_reader_load_strings(void) {
    if (reader_seek(reader, ast->string_table_offset) < 0) return -1;
    if (ast->string_count) {
        size_t bytes = sizeof(char*);
        if (bytes > (size_t)-1 / ast->string_count) return -1;
        bytes *= ast->string_count;
        ast->strings = (char**)cc_malloc(bytes);
        if (!ast->strings) return -1;
        for (uint16_t i = 0; i < ast->string_count; i++) ast->strings[i] = NULL;
        for (uint16_t i = 0; i < ast->string_count; i++) {
            uint16_t length = ast_read_u16();
            if (length >= MAX_STRING_LENGTH) return -1;
            /* Cast before addition: SDCC's int and size_t are both 16-bit. */
            char* text = (char*)cc_malloc((size_t)length + 1);
            if (!text) return -1;
            ast->strings[i] = text;
            for (uint16_t j = 0; j < length; j++) {
                int16_t ch = reader_next(reader);
                if (ch <= 0) return -1; /* Format stores C strings, never embedded NUL. */
                text[j] = (char)ch;
            }
            text[length] = '\0';
        }
    }
    if (reader_next(reader) != -1 || reader_failed(reader)) return -1;
    return ast_reader_validate();
}

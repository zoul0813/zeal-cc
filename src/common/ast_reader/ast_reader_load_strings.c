#include "ast_reader.h"

#include "ast_io.h"
#include "common.h"

int8_t ast_reader_load_strings(void) {

    if (ast->string_count == 0) return 0;
    if (reader_seek(reader, ast->string_table_offset) < 0) return -1;
    ast->strings = (char**)cc_malloc(sizeof(char*) * ast->string_count);
    if (!ast->strings) return -1;
    for (uint16_t i = 0; i < ast->string_count; i++) {
        ast->strings[i] = NULL;
    }
    for (uint16_t i = 0; i < ast->string_count; i++) {
        uint16_t len = 0;
        len = ast_read_u16();
        char* buf = (char*)cc_malloc(len + 1);
        if (!buf) {
            ast_reader_destroy();
            return -1;
        }
        for (uint16_t j = 0; j < len; j++) {
            int16_t ch = reader_next(reader);
            if (ch < 0) {
                cc_free(buf);
                ast_reader_destroy();
                return -1;
            }
            buf[j] = (char)ch;
        }
        buf[len] = '\0';
        ast->strings[i] = buf;
    }
    return 0;
}

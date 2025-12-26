#include "ast_reader.h"

const char* ast_reader_string(ast_reader_t* ast, uint16_t index) {
    if (!ast || !ast->strings || index >= ast->string_count) return NULL;
    return ast->strings[index];
}

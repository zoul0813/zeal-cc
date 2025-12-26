#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_read_type_info(ast_reader_t* ast, uint8_t* base, uint8_t* depth) {
    if (!ast || !ast->reader || !base || !depth) return -1;
    if (ast_read_u8(ast->reader, base) < 0) return -1;
    if (ast_read_u8(ast->reader, depth) < 0) return -1;
    return 0;
}

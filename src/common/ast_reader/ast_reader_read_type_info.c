#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_read_type_info(ast_reader_t* ast, uint8_t* base, uint8_t* depth,
                                 uint16_t* array_len) {
    if (!ast || !ast->reader || !base || !depth || !array_len) return -1;
    ast_read_u8(ast->reader, base);
    ast_read_u8(ast->reader, depth);
    ast_read_u16(ast->reader, array_len);
    return 0;
}

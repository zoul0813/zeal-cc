#include "ast_reader.h"

#include "ast_format.h"
#include "ast_io.h"

int8_t ast_reader_begin_program(ast_reader_t* ast, uint16_t* decl_count) {
    uint8_t tag = 0;
    uint16_t count = 0;
    if (!ast || !ast->reader || !decl_count) return -1;
    if (reader_seek(ast->reader, AST_HEADER_SIZE) < 0) return -1;
    ast_read_u8(ast->reader, &tag);
    if (tag != AST_TAG_PROGRAM) return -1;
    ast_read_u16(ast->reader, &count);
    ast->decl_count = count;
    ast->decl_index = 0;
    ast->program_started = 1;
    *decl_count = count;
    return 0;
}

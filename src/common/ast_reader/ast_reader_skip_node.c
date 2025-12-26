#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_skip_node(ast_reader_t* ast) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;
    if (ast_read_u8(ast->reader, &tag) < 0) return -1;
    return ast_reader_skip_tag(ast, tag);
}

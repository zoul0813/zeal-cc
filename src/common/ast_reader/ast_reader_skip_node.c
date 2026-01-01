#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_skip_node(ast_reader_t* ast) {
    uint8_t tag = 0;
    if (!ast || !ast->reader) return -1;
    tag = ast_read_u8(ast->reader);
    return ast_reader_skip_tag(ast, tag);
}

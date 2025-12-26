#include "ast_reader_internal.h"

#include "ast_format.h"
#include "ast_io.h"

ast_node_t* ast_reader_read_root(ast_reader_t* ast) {
    if (!ast || !ast->reader) return NULL;
    if (reader_seek(ast->reader, AST_HEADER_SIZE) < 0) return NULL;
    return ast_read_node(ast);
}

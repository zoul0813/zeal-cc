#include "ast_reader_internal.h"

ast_node_t* ast_reader_read_decl(ast_reader_t* ast) {
    if (!ast || !ast->reader) return NULL;
    if (!ast->program_started) return NULL;
    if (ast->decl_index >= ast->decl_count) return NULL;
    ast_node_t* node = ast_read_node(ast);
    if (!node) return NULL;
    ast->decl_index++;
    return node;
}

#include "ast_reader.h"

#include "common.h"

void ast_reader_destroy(ast_reader_t* ast) {
    if (!ast) return;
    if (ast->strings) {
        for (uint16_t i = 0; i < ast->string_count; i++) {
            cc_free(ast->strings[i]);
        }
        cc_free(ast->strings);
    }
    ast->strings = NULL;
    ast->string_count = 0;
    ast->node_count = 0;
    ast->string_table_offset = 0;
    ast->decl_count = 0;
    ast->decl_index = 0;
    ast->program_started = 0;
}

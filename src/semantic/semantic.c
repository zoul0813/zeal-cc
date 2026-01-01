#include "semantic.h"

cc_error_t semantic_validate(ast_reader_t* ast) {
    uint16_t decl_count = 0;
    if (!ast) return CC_ERROR_INVALID_ARG;
    if (ast_reader_begin_program(ast, &decl_count) < 0) {
        return CC_ERROR_SEMANTIC;
    }
    for (uint16_t i = 0; i < decl_count; i++) {
        if (ast_reader_skip_node(ast) < 0) {
            return CC_ERROR_SEMANTIC;
        }
    }
    return CC_OK;
}

#include "ast_reader.h"

#include "ast_io.h"
#include "ast_format.h"

int8_t ast_reader_read_type_info(uint8_t* base, uint8_t* depth,
                                 uint16_t* array_len) {
    if (!base || !depth || !array_len) return -1;
    *base = ast_read_u8();
    *depth = ast_read_u8();
    *array_len = ast_read_u16();
    uint8_t kind = (uint8_t)(*base & AST_BASE_MASK);
    if (kind < AST_BASE_INT || kind > AST_BASE_VOID || *depth > AST_MAX_POINTER_DEPTH) return -1;
    if (kind == AST_BASE_VOID && ((*base & AST_BASE_FLAG_UNSIGNED) || (*array_len && !*depth))) return -1;
    if (*array_len > INT16_MAX) return -1;
    return 0;
}

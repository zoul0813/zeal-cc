#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_read_type_info(uint8_t* base, uint8_t* depth,
                                 uint16_t* array_len) {
    if (!base || !depth || !array_len) return -1;
    *base = ast_read_u8();
    *depth = ast_read_u8();
    *array_len = ast_read_u16();
    return 0;
}

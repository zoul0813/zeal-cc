#include "ast_reader.h"

#include "ast_io.h"

int8_t ast_reader_skip_node() {
    uint8_t tag = 0;
    tag = ast_read_u8();
    return ast_reader_skip_tag(tag);
}

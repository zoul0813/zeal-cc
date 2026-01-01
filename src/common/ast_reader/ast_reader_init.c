#include "ast_reader.h"

#include "ast_format.h"
#include "ast_io.h"
#include "cc_compat.h"
#include "common.h"

static int8_t ast_read_header(
    reader_t* reader,
    uint8_t* out_version,
    uint16_t* node_count,
    uint16_t* string_count,
    uint32_t* string_table_offset
) {
    if (!reader || !out_version || !node_count || !string_count || !string_table_offset) return -1;
    char magic[4];
    for (uint8_t i = 0; i < 4; i++) {
        int16_t ch = reader_next(reader);
        if (ch < 0) return -1;
        magic[i] = (char)ch;
    }
    if (mem_cmp(magic, AST_MAGIC, 4) != 0) return -1;
    uint8_t version = 0;
    uint8_t reserved = 0;
    uint16_t flags = 0;
    version = ast_read_u8(reader);
    reserved = ast_read_u8(reader);
    flags = ast_read_u16(reader);
    *out_version = version;
    (void)reserved;
    (void)flags;
    *node_count = ast_read_u16(reader);
    *string_count = ast_read_u16(reader);
    *string_table_offset = ast_read_u32(reader);
    return 0;
}

int8_t ast_reader_init(ast_reader_t* ast, reader_t* reader) {
    if (!ast || !reader) return -1;
    ast->reader = reader;
    ast->node_count = 0;
    ast->string_count = 0;
    ast->string_table_offset = 0;
    ast->format_version = 0;
    ast->strings = NULL;
    ast->decl_count = 0;
    ast->decl_index = 0;
    ast->program_started = 0;

    if (reader_seek(reader, 0) < 0) return -1;
    if (ast_read_header(reader, &ast->format_version, &ast->node_count, &ast->string_count,
                        &ast->string_table_offset) < 0) {
        return -1;
    }
    if (ast->string_count > 0 && ast->string_table_offset < AST_HEADER_SIZE) return -1;
    return 0;
}

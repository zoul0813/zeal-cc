#include "ast_io.h"

static error_handler s_handler;
static char* s_message;

void ast_read_handler(error_handler f, char* msg)
{
    s_handler = f;
    s_message = msg;
}

/** Unsafe */

static int8_t ast_read_u8_unsafe(reader_t* reader, uint8_t* out)
{
    if (!out)
        return -1;
    int16_t ch = reader_next(reader);
    if (ch < 0)
        return -1;
    *out = (uint8_t) ch;
    return 0;
}

static int8_t ast_read_u16_unsafe(reader_t* reader, uint16_t* out)
{
    uint8_t lo = 0;
    uint8_t hi = 0;
    if (!out)
        return -1;
    if (ast_read_u8_unsafe(reader, &lo) < 0)
        return -1;
    if (ast_read_u8_unsafe(reader, &hi) < 0)
        return -1;
    *out = (uint16_t) (lo | ((uint16_t) hi << 8));
    return 0;
}

static int8_t ast_read_u32_unsafe(reader_t* reader, uint32_t* out)
{
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;
    if (!out)
        return -1;
    if (ast_read_u8_unsafe(reader, &b0) < 0)
        return -1;
    if (ast_read_u8_unsafe(reader, &b1) < 0)
        return -1;
    if (ast_read_u8_unsafe(reader, &b2) < 0)
        return -1;
    if (ast_read_u8_unsafe(reader, &b3) < 0)
        return -1;
    *out = (uint32_t) b0 | ((uint32_t) b1 << 8) | ((uint32_t) b2 << 16) | ((uint32_t) b3 << 24);
    return 0;
}

static int8_t ast_read_i16_unsafe(reader_t* reader, int16_t* out)
{
    uint16_t value = 0;
    if (!out)
        return -1;
    if (ast_read_u16_unsafe(reader, &value) < 0)
        return -1;
    *out = (int16_t) value;
    return 0;
}

/** Safe */
uint8_t ast_read_u8(reader_t* reader)
{
    uint8_t value = 0;
    if (ast_read_u8_unsafe(reader, &value) < 0) {
        s_handler(s_message);
    }
    return value;
}

uint16_t ast_read_u16(reader_t* reader)
{
    uint16_t value = 0;
    if (ast_read_u16_unsafe(reader, &value) < 0) {
        s_handler(s_message);
    }
    return value;
}

uint32_t ast_read_u32(reader_t* reader)
{
    uint32_t value = 0;
    if (ast_read_u32_unsafe(reader, &value) < 0) {
        s_handler(s_message);
    }
    return value;
}

int16_t ast_read_i16(reader_t* reader)
{
    int16_t value = 0;
    if (ast_read_i16_unsafe(reader, &value) < 0) {
        s_handler(s_message);
    }
    return value;
}

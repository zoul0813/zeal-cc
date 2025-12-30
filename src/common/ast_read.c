#include "ast_io.h"

static error_handler s_handler;

void ast_read_handler(error_handler f)
{
    s_handler = f;
}

int8_t ast_read_u8(reader_t* reader, uint8_t* out) {
    if (!out) return -1;
    int16_t ch = reader_next(reader);
    if (ch < 0) return -1;
    *out = (uint8_t)ch;
    return 0;
}

int8_t ast_read_u16(reader_t* reader, uint16_t* out) {
    uint8_t lo = 0;
    uint8_t hi = 0;
    if (!out) return -1;
    if (ast_read_u8(reader, &lo) < 0) return -1;
    if (ast_read_u8(reader, &hi) < 0) return -1;
    *out = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 0;
}

int8_t ast_read_u32(reader_t* reader, uint32_t* out) {
    uint8_t b0 = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t b3 = 0;
    if (!out) return -1;
    if (ast_read_u8(reader, &b0) < 0) return -1;
    if (ast_read_u8(reader, &b1) < 0) return -1;
    if (ast_read_u8(reader, &b2) < 0) return -1;
    if (ast_read_u8(reader, &b3) < 0) return -1;
    *out = (uint32_t)b0 |
           ((uint32_t)b1 << 8) |
           ((uint32_t)b2 << 16) |
           ((uint32_t)b3 << 24);
    return 0;
}

int8_t ast_read_i16(reader_t* reader, int16_t* out) {
    uint16_t value = 0;
    if (!out) return -1;
    if (ast_read_u16(reader, &value) < 0) return -1;
    *out = (int16_t)value;
    return 0;
}

uint8_t ast_read_u8_safe(reader_t* reader)
{
    static uint8_t ch;
    if (ast_read_u8(reader, &ch) < 0)
        s_handler();
    return ch;
}

uint16_t ast_read_u16_safe(reader_t* reader)
{
    static uint16_t ch;
    if (ast_read_u16(reader, &ch) < 0)
        s_handler();
    return ch;
}

uint32_t ast_read_u32_safe(reader_t* reader)
{
    static uint32_t ch;
    if (ast_read_u32(reader, &ch) < 0)
        s_handler();
    return ch;
}

int16_t ast_read_i16_safe(reader_t* reader)
{
    static int16_t ch;
    if (ast_read_i16(reader, &ch) < 0)
        s_handler();
    return ch;
}


#include "ast_io.h"

int8_t ast_write_u8(output_t out, uint8_t value) {
    return output_write(out, (const char*)&value, 1);
}

int8_t ast_write_u16(output_t out, uint16_t value) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    return output_write(out, (const char*)buf, 2);
}

int8_t ast_write_u32(output_t out, uint32_t value) {
    uint8_t buf[4];
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    buf[2] = (uint8_t)((value >> 16) & 0xFF);
    buf[3] = (uint8_t)((value >> 24) & 0xFF);
    return output_write(out, (const char*)buf, 4);
}

int8_t ast_write_i16(output_t out, int16_t value) {
    return ast_write_u16(out, (uint16_t)value);
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

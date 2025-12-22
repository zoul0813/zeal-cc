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

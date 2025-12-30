#include "ast_io.h"

static uint8_t g_ast_u8_buf;
static uint8_t g_ast_u16_buf[2];
static uint8_t g_ast_u32_buf[4];

static error_handler s_handler;
static char* s_message;

void ast_write_handler(error_handler f, char* msg)
{
    s_handler = f;
    s_message = msg;
}

static int8_t ast_write_u8_unsafe(output_t out, uint8_t value)
{
    g_ast_u8_buf = value;
    return output_write(out, (const char*) &g_ast_u8_buf, 1);
}

static int8_t ast_write_u16_unsafe(output_t out, uint16_t value)
{
    g_ast_u16_buf[0] = (uint8_t) (value & 0xFF);
    g_ast_u16_buf[1] = (uint8_t) ((value >> 8) & 0xFF);
    return output_write(out, (const char*) g_ast_u16_buf, 2);
}

static int8_t ast_write_u32_unsafe(output_t out, uint32_t value)
{
    g_ast_u32_buf[0] = (uint8_t) (value & 0xFF);
    g_ast_u32_buf[1] = (uint8_t) ((value >> 8) & 0xFF);
    g_ast_u32_buf[2] = (uint8_t) ((value >> 16) & 0xFF);
    g_ast_u32_buf[3] = (uint8_t) ((value >> 24) & 0xFF);
    return output_write(out, (const char*) g_ast_u32_buf, 4);
}

static int8_t ast_write_i16_unsafe(output_t out, int16_t value)
{
    return ast_write_u16_unsafe(out, (uint16_t) value);
}

void ast_write_u8(output_t out, uint8_t value)
{
    if (ast_write_u8_unsafe(out, value) < 0) {
        s_handler(s_message);
    }
}

void ast_write_u16(output_t out, uint16_t value)
{
    if (ast_write_u16_unsafe(out, value) < 0) {
        s_handler(s_message);
    }
}

void ast_write_u32(output_t out, uint32_t value)
{
    if (ast_write_u32_unsafe(out, value) < 0) {
        s_handler(s_message);
    }
}

void ast_write_i16(output_t out, int16_t value)
{
    if (ast_write_i16_unsafe(out, value) < 0) {
        s_handler(s_message);
    }
}

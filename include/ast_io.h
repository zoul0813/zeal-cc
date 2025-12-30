#ifndef AST_IO_H
#define AST_IO_H

#include <stdint.h>

#include "target.h"

#define AST_MAGIC "ZAST"
#define AST_FORMAT_VERSION 1

typedef void (*error_handler)(char* msg);

void ast_write_handler(error_handler f, char* msg);
void ast_read_handler(error_handler f, char* msg);

// int8_t ast_write_u8(output_t out, uint8_t value);
// int8_t ast_write_u16(output_t out, uint16_t value);
// int8_t ast_write_u32(output_t out, uint32_t value);
// int8_t ast_write_i16(output_t out, int16_t value);

void ast_write_u8(output_t out, uint8_t value);
void ast_write_u16(output_t out, uint16_t value);
void ast_write_u32(output_t out, uint32_t value);
void ast_write_i16(output_t out, int16_t value);

void ast_read_u8(reader_t* reader, uint8_t* out);
void ast_read_u16(reader_t* reader, uint16_t* out);
void ast_read_u32(reader_t* reader, uint32_t* out);
void ast_read_i16(reader_t* reader, int16_t* out);

// uint8_t ast_read_u8_safe(reader_t* reader);
// uint16_t ast_read_u16_safe(reader_t* reader);
// uint32_t ast_read_u32_safe(reader_t* reader);
// int16_t ast_read_i16_safe(reader_t* reader);

#endif /* AST_IO_H */

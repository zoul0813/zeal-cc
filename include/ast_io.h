#ifndef AST_IO_H
#define AST_IO_H

#include <stdint.h>

#include "target.h"

#define AST_MAGIC "ZAST"
#define AST_FORMAT_VERSION 1

int ast_write_u8(output_t out, uint8_t value);
int ast_write_u16(output_t out, uint16_t value);
int ast_write_u32(output_t out, uint32_t value);
int ast_write_i16(output_t out, int16_t value);

int ast_read_u8(reader_t* reader, uint8_t* out);
int ast_read_u16(reader_t* reader, uint16_t* out);
int ast_read_u32(reader_t* reader, uint32_t* out);
int ast_read_i16(reader_t* reader, int16_t* out);

#endif /* AST_IO_H */

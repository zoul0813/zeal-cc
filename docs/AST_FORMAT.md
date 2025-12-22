AST Binary Format

Overview
- Purpose: compact, seekable encoding of the AST for the `cc_parse` -> `cc_codegen` split.
- Endianness: little-endian.
- Versioning: header includes a format version for forward compatibility.
- Strings: stored once in a string table and referenced by index.

File Layout (seekable)
1) Header
2) Node stream (preorder traversal)
3) String table (at end)

Header (fixed size, little-endian)
- magic: 4 bytes = "ZAST"
- version: u8 (current = 1)
- flags: u8 (bit 0 = reserved; bit 1 = reserved)
- reserved: u16 (set to 0)
- node_count: u16 (0 = unknown, read until string_table_offset)
- string_count: u16
- string_table_offset: u32 (absolute offset from file start; must be >= header size)

String Table (placed at `string_table_offset`)
- Repeated `string_count` entries:
  - length: u16 (number of bytes, no null terminator)
  - bytes: `length` bytes (ASCII/UTF-8 as emitted by lexer)

Node Stream
- `node_count` nodes, preorder (parent before children), or until `string_table_offset`.
- Each node:
  - tag: u8
  - payload: tag-specific fields (see below)

Node Tags and Payloads (explicit IDs, not tied to `ast_node_type_t` enum values)
- AST_PROGRAM (tag = 1)
  - decl_count: u16
  - children: `decl_count` nodes
- AST_FUNCTION (tag = 2)
  - name_index: u16
  - return_type: type_encoding
  - param_count: u8
  - children: `param_count` parameters, then body node
- AST_VAR_DECL (tag = 3)
  - name_index: u16
  - var_type: type_encoding
  - has_initializer: u8 (0/1)
  - children: initializer node if present
- AST_COMPOUND_STMT (tag = 4)
  - stmt_count: u16
  - children: `stmt_count` statement nodes
- AST_RETURN_STMT (tag = 5)
  - has_expr: u8 (0/1)
  - children: expr node if present
- AST_IF_STMT (tag = 6)
  - has_else: u8 (0/1)
  - children: condition, then_branch, else_branch (if present)
- AST_WHILE_STMT (tag = 7)
  - children: condition, body
- AST_FOR_STMT (tag = 8)
  - has_init: u8 (0/1)
  - has_cond: u8 (0/1)
  - has_inc: u8 (0/1)
  - children: init (if present), cond (if present), inc (if present), body
- AST_ASSIGN (tag = 9)
  - children: lvalue, rvalue
- AST_CALL (tag = 10)
  - name_index: u16
  - arg_count: u8
  - children: `arg_count` args
- AST_BINARY_OP (tag = 11)
  - op: u8
  - children: left, right
- AST_UNARY_OP (tag = 12)
  - op: u8
  - children: operand
- AST_IDENTIFIER (tag = 13)
  - name_index: u16
- AST_CONSTANT (tag = 14)
  - value: i16 (current compiler emits 8/16-bit values only)
- AST_STRING_LITERAL (tag = 15)
  - value_index: u16
- AST_ARRAY_ACCESS (tag = 16)
  - children: base, index

Type Encoding (type_encoding)
- base: u8 (1 = int, 2 = char, 3 = void)
- pointer_depth: u8 (0 for non-pointer, 1 for *, 2 for **, etc.)

Operator Encoding (op)
- Use the `binary_op_t` / `unary_op_t` values from `include/parser.h`.
- These values are distinct from lexer `token_type_t`.

Binary op values
- OP_ADD = 0
- OP_SUB = 1
- OP_MUL = 2
- OP_DIV = 3
- OP_MOD = 4
- OP_AND = 5
- OP_OR  = 6
- OP_XOR = 7
- OP_SHL = 8
- OP_SHR = 9
- OP_EQ  = 10
- OP_NE  = 11
- OP_LT  = 12
- OP_LE  = 13
- OP_GT  = 14
- OP_GE  = 15
- OP_LAND = 16
- OP_LOR  = 17

Unary op values
- OP_NEG = 0
- OP_NOT = 1
- OP_LNOT = 2
- OP_ADDR = 3
- OP_DEREF = 4
- OP_PREINC = 5
- OP_PREDEC = 6
- OP_POSTINC = 7
- OP_POSTDEC = 8

Notes
- Strings are stored exactly as the lexer emits them (no implicit null terminators).
- The reader must support seek so codegen can jump to `string_table_offset`
  to resolve identifiers/literals after parsing the node stream.
- `node_count` allows single-pass parsing without EOF checks; `0` means read until
  `string_table_offset`.
- `node_count` uses u16 and is limited to 65,535 nodes per file.
- `string_table_offset` must be less than file size; invalid offsets should error.
- Unknown tags or versions should cause a hard error in `cc_codegen`.
- Keep `src/tools/ast_dump.c` in sync with any AST format changes for validation.

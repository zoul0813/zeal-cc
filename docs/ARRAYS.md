# Arrays Support Strategy

This document outlines the planned array support for `int`, `char`, `void`, and pointers.

## Scope

- [x] Support array declarators in locals, globals, and parameters.
- [x] Support array access expressions (`arr[i]`).
- [x] Support string literal initialization for `char[]`.
- [x] Maintain pointer decay for array parameters.

## Type System and AST Encoding

- [x] Add `type_create_array(type_t* element, size_t length)` and track `TYPE_ARRAY`.
- [x] Extend AST type encoding beyond `(base, pointer_depth)` to include array length(s).
  - [x] Minimal viable: support 1-D arrays with a single length field.
  - [ ] If multi-dimensional arrays are needed later, encode an array depth and lengths list.

## Parser Changes

- [x] Extend declarator parsing in:
  - [x] `parse_statement` (locals)
  - [x] `parse_parameter` (params)
  - [x] `parse_declaration` (globals)
- [x] Parse `[]` after identifiers and build array types around the base/pointer type.
- Validation rules:
  - [x] Disallow `void[]` (allow `void*` and arrays of `void*` if needed).
  - [x] Array parameters decay to pointer type in the AST/type encoding.

## Codegen Changes

- Allocation:
  - [x] Locals/globals allocate `length * element_size` bytes.
- Base address for array access:
  - [x] Local arrays: `IX + offset` (base address of storage).
  - [x] Global arrays: label address.
  - [x] Pointer variables: existing pointer load path.
- Index scaling:
  - [x] `char` elements: 1 byte.
  - [x] `int` and pointer elements: 2 bytes.
- Element load/store:
  - [x] `char`: use `A`.
  - [x] `int`/pointer: use `HL`.
- String literal initialization:
  - [x] Emit `.dm` (or explicit bytes) plus null terminator.
  - [x] Validate literal length <= array length.
- Remaining:
  - [ ] Expand array access to support pointer-returning expressions (not just identifiers/literals).

## Tests

- [x] `int a[3]; a[1] = 7; return a[1];`
- [x] `char s[4] = "hi"; return s[1];`
- [x] `int* p[2]; p[0] = &x; return *p[0];`
- Array parameter decay:
  - [x] `int f(int a[]) { return a[0]; }`
  - [x] `int f(char* s) { return s[0]; }` (covers pointer-param indexing)

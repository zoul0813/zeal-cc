# Limitations and Constraints

## Supported C Subset
- Only `int` and `char` are supported. No `float`/`double`.
- `long` (32-bit) is currently unsupported.
- Expressions: `+ - * / %` and comparisons `== != < > <= >=`.
- Statements: `if/else`, `while`, `for`, `return`, compound blocks.
- Global variable declarations are supported.
- Functions: definitions and calls are supported; parameters are passed on the
  stack with an `IX` frame (see calling convention).
- String literals are supported only for constant indexing (e.g., `"hi"[0]`).
- Pointers are supported only for address-of/deref, pointer assignment, and
  constant indexing via `ptr[const]`.

## Not Implemented Yet
- Arrays, structs, unions.
- Pointer arithmetic and non-constant indexing.
- Type checking and semantic analysis.
- Optimizations.

## Memory and I/O
- Static memory pool is fixed (12 KB).
- Source input is streaming only (512-byte buffer). No full-file loads.
- File buffer is placed at 0xE000 (512 B).

## Output
- Assembly output is Z80 and intended for Zealasm.
- The output uses `org 0x4000` and a minimal `crt0` stub.
- Program return values are expected in register `A` for 8-bit results and in
  `HL` for 16-bit results (including `int` and pointers). The ZOS exit code is
  8-bit, so `crt0` truncates `main`'s return value to the low byte (`L`) before
  calling `_exit`.

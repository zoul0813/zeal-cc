# Limitations and Constraints

## Supported C Subset
- Only `int` and `char` are supported. No `float`/`double`.
- `long` (32-bit) is currently unsupported.
- Expressions: `+ - * / %`, comparisons `== != < > <= >=`, logical `&& ||`, bitwise `& | ^ ~`, shifts `<< >>`.
- Unary expressions: `+`, `-`, `!`, `~`, `++`, `--`, address-of (`&`), and dereference (`*`) on identifiers.
- Statements: `if/else`, `while`, `for`, `return`, compound blocks.
- Global and local variable declarations are supported.
- Functions: definitions and calls are supported; parameters are passed on the
  stack with an `IX` frame (see calling convention).
- Single-dimension arrays are supported for globals/locals, with array params
  decaying to pointers.
- Array and pointer indexing are supported (8-bit indices).
- String literals are supported for pointer/array initialization and indexing
  (e.g., `"hi"[0]`).

## Not Implemented Yet
- Multi-dimensional arrays and array initializers (non-string).
- Structs, unions, enums, typedefs.
- Pointer arithmetic via `+`/`-` (array-style indexing only).
- Ternary, switch, break/continue, do/while.
- Type checking and semantic analysis.
- Optimizations.

## Memory and I/O
- Static memory pool is fixed (parser uses 0x1700 on target).
- Source input is streaming only (512-byte buffer). No full-file loads.
- File buffer is placed at 0xC300 (512 B).

## Output
- Assembly output is Z80 and intended for Zealasm.
- The output uses `org 0x4000` and a minimal `crt0` stub.
- Program return values are expected in register `A` for 8-bit results and in
  `HL` for 16-bit results (including `int` and pointers). The ZOS exit code is
  8-bit, so `crt0` truncates `main`'s return value to the low byte (`L`) before
  calling `_exit`.

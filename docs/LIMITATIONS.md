# Limitations and Constraints

## Supported C Subset
- Only `int` is supported. No `float`/`double`.
- Expressions: `+ - * / %` and comparisons `== != < > <= >=`.
- Statements: `if/else`, `while`, `for`, `return`, compound blocks.
- Functions: definitions and calls are supported, but parameters are not
  passed yet (see calling convention).

## Not Implemented Yet
- Calling convention and stack-based locals.
- Pointers, arrays, structs, unions.
- Type checking and semantic analysis.
- Optimizations.

## Memory and I/O
- Static memory pool is fixed (12 KB).
- Source input is streaming only (512-byte buffer). No full-file loads.
- File buffer is placed at 0xE000 (512 B).

## Output
- Assembly output is Z80 and intended for Zealasm.
- The output uses `org 0x4000` and a minimal `crt0` stub.
- Program return values are expected in register `A`.

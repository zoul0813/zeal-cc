# Calling Convention (Partial Implementation)

This document describes the current calling convention for the Zeal C
Compiler. It is partially implemented: parameters are stack-based and accessed
via an `IX` frame, with locals stored in the same stack frame. Return values
use `A` for 8-bit results and `HL` for 16-bit results (including `int` and
pointers).

## Goals
- Simple stack-based argument passing.
- 8-bit return values in `A`, 16-bit return values in `HL` (`int`/pointers).
- Support 16-bit arguments (passed on stack as 2 bytes).
- Minimal prologue/epilogue overhead.

## Current Convention

### Caller
- Evaluate arguments right-to-left.
- Push each argument on the stack.
- 8-bit args are widened to 16-bit and pushed in `HL`.
- 16-bit args are pushed in `HL`
- Emit `call <function>`.
- Caller cleans the stack after return.

### Callee
- Prologue:
- Preserve `IX` and set a simple frame (`push ix; ld ix, 0; add ix, sp`).
- Parameters accessed via fixed offsets from `IX`.
  - 8-bit params load from a single byte.
  - 16-bit params are pushed
- Return value:
  - 8-bit in `A`
  - 16-bit return values in `HL`
- Epilogue restores registers and returns via `ret`.

### Program Entry/Exit
- `main` returns follow the same rules (8-bit in `A`, 16-bit in `HL`).
- ZOS exit codes are 8-bit; `crt0` truncates `main`'s return value to the low
  byte (`L`) before calling `_exit`.

### Registers
- Return:
  - 8-bit: `A`
  - 16-bit: `HL`
- Temporaries: `A`, `L` are used heavily by codegen today.
- Register pairs (`BC`, `DE`, `HL`) available for 16-bit ops.
- `IX` is used for parameter access; locals are on the stack.

## Pending Work
- Consistent type sizing and stack cleanup for mixed-width params.

When the calling convention is implemented, both call sites and function
prologues will change to match the rules above.

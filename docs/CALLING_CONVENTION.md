# Calling Convention (Partial Implementation)

This document describes the current calling convention for the Zeal C
Compiler. It is partially implemented: parameters are stack-based and accessed
via an `IX` frame, while locals are still emitted as globals.

## Goals
- Simple stack-based argument passing.
- 8-bit return values in `A`, 16-bit return values in `HL`, 32-bit return values in `DEHL`.
- Support 16-bit arguments (passed on stack as 2 bytes) and 32-bit arguments (passed as 4 bytes).
- Minimal prologue/epilogue overhead.

## Current Convention

### Caller
- Evaluate arguments right-to-left.
- Push each argument on the stack.
- 8-bit args are widened to 16-bit and pushed as `HL`.
- 32-bit args are pushed as 4 bytes (high word first, then low word).
- Emit `call <function>`.
- Caller cleans the stack after return.

### Callee
- Prologue:
- Preserve `IX` and set a simple frame (`push ix; ld ix, 0; add ix, sp`).
- Parameters accessed via fixed offsets from `IX`.
  - 8-bit params load from a single byte.
- 16-bit params are not supported yet.
- 32-bit params are supported for `long`.
- Return value:
  - 8-bit in `A`
  - 32-bit in `DEHL`
- Epilogue restores registers and returns via `ret`.

### Registers
- Return:
  - 8-bit: `A`
  - 16-bit: `HL`
- Temporaries: `A`, `L` are used heavily by codegen today.
- Register pairs (`BC`, `DE`, `HL`) available for 16-bit ops.
- `IX` is used for parameter access; locals still use global labels.

## Pending Work
- Stack-based local variable storage (stack allocation).
- 16-bit parameters and return values.
- Consistent type sizing and stack cleanup for mixed-width params.

When the calling convention is implemented, both call sites and function
prologues will change to match the rules above.

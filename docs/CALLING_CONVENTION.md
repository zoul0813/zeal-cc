# Calling Convention (Planned)

This document describes the *intended* calling convention for the Zeal C
Compiler. It is not fully implemented yet, but serves as the design target
before codegen changes.

## Goals
- Simple stack-based argument passing.
- 8-bit return values in `A`, 16-bit return values in `HL`.
- Support 16-bit arguments (passed on stack as 2 bytes).
- Minimal prologue/epilogue overhead.

## Proposed Convention

### Caller
- Evaluate arguments right-to-left.
- Push each argument on the stack.
  - 8-bit: push one byte.
  - 16-bit: push low byte, then high byte (stack grows downward).
- Emit `call <function>`.
- Caller cleans the stack after return.

### Callee
- Prologue:
  - Preserve registers as needed.
  - Set up a simple stack frame (planned via `IX`).
- Parameters accessed via fixed offsets from `IX`.
  - 8-bit params load from a single byte.
  - 16-bit params load from two bytes into a register pair (e.g., `HL`).
- Return value:
  - 8-bit in `A`
  - 16-bit in `HL`
- Epilogue restores registers and returns via `ret`.

### Registers
- Return:
  - 8-bit: `A`
  - 16-bit: `HL`
- Temporaries: `A`, `L` are used heavily by codegen today.
- Register pairs (`BC`, `DE`, `HL`) available for 16-bit ops.
- `IX` reserved for stack frame once locals/params are implemented.

## Current Behavior (Not Final)
- Parameters are passed on the stack at call sites.
- Locals are emitted as global labels.
- Function calls push arguments then emit `call`.
- No stack frame is emitted yet; `IX` is not used for locals/params.

When the calling convention is implemented, both call sites and function
prologues will change to match the rules above.

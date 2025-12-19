# Zeal 8-bit C Compiler — Task Board

Scope: C99→Z80 compiler for Zeal 8-bit OS. All test inputs/outputs live in `tests/`; never write artifacts elsewhere. Desktop build (make) and ZOS build (cmake) both supported.

## P0: Unblock Current Failures
- Fix parser hang on function parameters (`int add(int a, int b)`). Add regression test in `tests/` to cover parameter lists.
- Align docs: update README vs STATUS so control-flow status and known limitations match reality.

## P1: Stack Locals & Calling Convention
- Implement stack-based locals with function prologue/epilogue (IX frame or alternative) and switch off global-label locals.
- Define and implement calling convention for arguments (stack/registers), including parameter loading in prologue and correct return value convention.
- Update codegen to emit argument passing for calls and local variable access. Add tests for functions with parameters and nested calls.

## P2: Semantic Analysis Foundations
- Integrate symbol table with scopes (block/function) and detect redeclarations/undefined identifiers.
- Basic type system and checking for expressions/assignments/returns; enforce `int` for now if needed.
- Emit meaningful diagnostics (line/column from lexer) for semantic errors. Add negative tests.

## P3: Language Coverage Expansion
- Pointer and array parsing/codegen (at least address-of, deref, basic indexing).
- Struct/union parsing skeleton (even if codegen deferred). Define layout rules.
- Additional operators: logical &&/||/!, bitwise ops, shifts, ternary.
- Control flow completeness: switch/case/default, break/continue semantics.

## P4: Codegen Quality & Runtime
- Simple optimizations: constant folding in parser/AST, dead code for unused branches where trivial.
- Improve register/stack usage to reduce pushes/pops where safe.
- Expand runtime helpers (division/mod for 16-bit, mem copy/set stubs) as needed by new features.

## P5: Tooling, Tests, and CI
- Grow regression suite: positive/negative cases for parameters, locals, control flow, pointers, arrays.
- Add scripted test runner target (keeps outputs in `tests/`). Consider snapshotting expected asm for key cases.
- Document build/test matrix (desktop vs ZOS) and prerequisites (SDCC, ZOS_PATH, ZVB SDK).

## P6: Documentation
- Update architecture notes to reflect multi-pass flow and calling convention.
- Provide usage examples that cover parameters, locals, and control flow.
- Keep STATUS.md in sync with progress; note remaining limitations and known bugs.

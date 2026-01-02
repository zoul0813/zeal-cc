# Semantic Pass (cc_semantic)

## Purpose

`cc_semantic` sits between parse and codegen to validate the AST and, later, attach semantic information (types, symbol IDs, and scope data). This keeps the parser small and lets semantic checks evolve without changing parsing logic.

## Current Behavior (Skeleton)

- Validation-only: no AST output or mutation.
- Reads the AST header and string table.
- Walks the program and skips each declaration to validate structure.
- Symbol resolution and scope rules: adds vars/funcs, rejects duplicates, errors on undefined identifiers/functions
- Lvalue/rvalue checks: assignment target must be lvalue; unary inc/dec and & require lvalue; deref produces lvalue
- Control-flow rules: break/continue only inside loops; goto labels must exist and can’t jump into deeper scope; duplicate labels
- Function return rules: void functions can’t return values; non-void functions must return a value
- Array init constraint: array initializers must be string literals
- Type checking and implicit conversions.

## Planned Responsibilities

- Array/pointer decay rules and index validity.
- Emit semantic diagnostics with line context where available.
- Optional type annotations in the AST for codegen.

## Pipeline

Current intended flow:

```sh
cc_parse     input.c   output.ast
cc_semantic  output.ast
cc_codegen   output.ast output.asm
```

## Notes

- The semantic pass should remain streaming-friendly and memory-aware.
- AST annotations can be added either via new tags/sections or a side-table to avoid large AST rewrites.

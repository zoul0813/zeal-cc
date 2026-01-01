# Semantic Pass (cc_semantic)

## Purpose
`cc_semantic` sits between parse and codegen to validate the AST and, later, attach semantic information (types, symbol IDs, and scope data). This keeps the parser small and lets semantic checks evolve without changing parsing logic.

## Current Behavior (Skeleton)
- Reads the AST header and string table.
- Walks the program and skips each declaration to validate structure.
- Validation-only: no AST output or mutation.

## Planned Responsibilities
- Symbol resolution and scope rules (undefined identifiers, redeclarations, shadowing).
- Type checking and implicit conversions.
- Lvalue/rvalue validation for assignments and address-of.
- Control-flow validation (`break`/`continue` in loops, `goto`/labels).
- Array/pointer decay rules and index validity.
- Emit semantic diagnostics with line context where available.
- Optional type annotations in the AST for codegen.

## Pipeline
Current intended flow:

```
cc_parse     input.c   output.ast
cc_semantic  output.ast
cc_codegen   output.ast output.asm
```

## Notes
- The semantic pass should remain streaming-friendly and memory-aware.
- AST annotations can be added either via new tags/sections or a side-table to avoid large AST rewrites.

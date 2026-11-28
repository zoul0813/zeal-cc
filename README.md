# Zeal 8-bit C Compiler

A C99 compiler targeting the Zeal 8-bit OS platform, powered by a Z80 CPU.

## Project Status

This is an iterative development project. Currently implemented:

- ✅ Phase 1: Basic compiler structure
- ✅ Phase 2: Lexer/Tokenizer (complete with C99 token support)
- ✅ Phase 3: Parser (expressions, variables, assignments, function calls)
- ✅ Phase 4: Symbol table (basic implementation)
- ✅ Phase 5: Z80 code generator (expressions, statements, runtime helpers)
- ✅ Phase 6: Control flow (if, while, for)
- ✅ Phase 7: Stack-based locals and full calling convention
- 🚧 Phase 8: Optimizations and documentation

### Working Features ✅
- Complete C99 lexer with all token types
- Recursive descent parser with operator precedence
- Streaming file input on both host and ZOS (512-byte buffered reader; no full-file loads)
- Control flow: if/else, while, and for
- Control flow: break, continue, goto, labels
- Comparison operators (==, !=, <, >, <=, >=)
- Binary arithmetic operators (+, -, *, /, %)
- Bitwise operators (&, |, ^, ~, <<, >>)
- Logical operators (&&, ||) with short-circuit
- Function definitions and calls
- Stack-based argument passing (IX frame for params)
- Stack-based local variables
- Variable declarations and assignments
- Global variable declarations
- Single-dimension arrays (globals/locals) and array parameters (decay to pointer)
- Array and pointer indexing (8-bit indices)
- String literals for pointer/array initialization and indexing
- Unary operators: `+`, `-`, `!`, `~`, `++`, `--`
- Return statements with expressions
- Compound statements (blocks)
- Runtime library for mul/div/mod
- Expression evaluation with correct precedence
- Multiple functions per file
- `char` type and character literals
- `signed`/`unsigned` qualifiers on `char`/`int` (default unsigned)
- Pointer basics: address-of and dereference on identifiers

### In Progress 🚧
- Parser target build uses 0x1700 static pool; file buffer at 0xC300 (512 B)
- Semantic pass (cc_semantic) skeleton: validation-only

### C99/ANSI C Gap (Current Limitations)
- No preprocessor (`#include`, `#define`, macros).
- Limited type system: only `int` and `char` (no `short`, `long`, floats, structs/unions/enums/typedefs).
- No full type checking or semantic analysis.
- Missing language features: switch, ternary, do/while, pointer arithmetic, multi-dimensional arrays.

### Planned Features ⏳
- Switch statement, do/while, ternary
- Full pointer arithmetic and multidimensional arrays
- Struct and union types
- Type checking and semantic analysis
- Optimizations (constant folding, dead code elimination)
- Preprocessor support

## Building

### Desktop Build (for testing)

```bash
make
```

This will compile the compiler for your desktop (macOS/Linux) using GCC.

### ZOS Build (target platform)

```bash
zde cmake
```

Verbose build (defines `VERBOSE` for `#ifdef VERBOSE`):
```bash
zde cmake --target verbose
```

This requires:
- SDCC 4.4.0
- ZOS_PATH environment variable set to Zeal 8-bit OS source
- ZVB SDK and coreutils

## Usage

### Desktop version:
```bash
./bin/cc_parse_darwin input.c output.ast
./bin/cc_semantic_darwin output.ast
./bin/cc_codegen_darwin output.ast output.asm
```

Use the `_linux` suffix on Linux. Run codegen from the repository root so it can
read `runtime/*.asm`. Any stage failure stops the pipeline; check its exit status
before running the next stage.

### Build and run on Zeal

Run `make dist` on the host. Copy the contents of `dist/zeal-cc/` to the root
of a writable Zeal drive, install Zealasm as `bin/zealasm`, and put that
drive's `bin` directory on the ZShell PATH. From the installation drive:

```text
cd /examples/hexdump
cc hexdump.c
./hexdump.bin
```

The example reads `sample.dat` in the current directory and prints
`5A 65 61 6C 0A`. Use `cc -S hexdump.c` to inspect its assembly.
The separate parser, semantic checker, and code generator remain available.

## Testing

Run `make test` for native infrastructure and host generation. Run
`make test-media` to prepare the UART kernel and assembler, then
`make accept` to build and exercise the installation under `zeal-headless`.
For a symbolic breakpoint in the target parser run
`make debug CASE=simple_return STAGE=parse`. See [TESTING.md](TESTING.md).

## Architecture

- `src/main.c` - Entry point and file I/O
- `src/lexer.c` - Lexical analysis (tokenization)
- `src/parser.c` - Syntax analysis (AST generation)
- `src/symbol.c` - Symbol table and type system
- `src/codegen.c` - Z80 assembly code generation

## Current Limitations

- Parser lacks do/while, ternary, switch, break/continue
- Fixed static pool allocator; parser uses 0x1700 on target (no dynamic malloc)
- 512-byte input reader buffer; very large source files are not supported
- No optimization passes yet
- Limited error reporting
- `long` (32-bit) is currently unsupported
- Not yet supported in codegen: do/while, ternary operator, structs/unions
- Array init is limited to string literals for `char[]`
- Pointer support is limited to address-of/deref on identifiers and array-style indexing

## Next Steps

We will iterate on this implementation, adding:
1. Semantic analysis and type checking
2. Extend parser support (do/while, ternary, logical/bitwise, switch/break/continue)
3. Complete codegen for missing constructs (structs/unions, pointer arithmetic, multidimensional arrays, array init)
4. Preprocessor support
5. Optimization passes
6. Register allocation

## License

See LICENSE file for details.

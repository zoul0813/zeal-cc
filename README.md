# Zeal 8-bit C Compiler

A C99 compiler targeting the Zeal 8-bit OS platform, powered by a Z80 CPU.

## Project Status

This is an iterative development project. Currently implemented:

- ✅ Phase 1: Basic compiler structure
- ✅ Phase 2: Lexer/Tokenizer (complete with C99 token support)
- 🚧 Phase 3: Parser (basic structure in place)
- 🚧 Phase 4: Symbol table (basic implementation)
- 🚧 Phase 5: Z80 code generator (stub implementation)
- ⏳ Phase 6: Testing and iteration
- ⏳ Phase 7: Optimizations
- ⏳ Phase 8: Documentation and validation

## Building

### Desktop Build (for testing)

```bash
make
```

This will compile the compiler for your desktop (macOS/Linux) using GCC.

### ZOS Build (target platform)

```bash
mkdir -p build && cd build
cmake ..
make
```

This requires:
- SDCC 4.4.0
- ZOS_PATH environment variable set to Zeal 8-bit OS source
- ZVB SDK and coreutils

## Usage

### Desktop version:
```bash
./bin/cc input.c output.asm
```

### ZOS version:
```bash
cc input.c output.asm
```

The compiler generates Z80 assembly compatible with Zealasm.

## Testing

```bash
make test
```

## Architecture

- `src/main.c` - Entry point and file I/O
- `src/lexer.c` - Lexical analysis (tokenization)
- `src/parser.c` - Syntax analysis (AST generation)
- `src/symbol.c` - Symbol table and type system
- `src/codegen.c` - Z80 assembly code generation

## Current Limitations

- Parser is simplified and needs expansion
- Only basic code generation is implemented
- No optimization passes yet
- Limited error reporting

## Next Steps

We will iterate on this implementation, adding:
1. Complete recursive descent parser
2. Full semantic analysis
3. Comprehensive code generation
4. Register allocation
5. Optimization passes
6. Extended C99 feature support

## License

See LICENSE file for details.

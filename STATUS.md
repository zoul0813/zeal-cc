# Zeal 8-bit C Compiler - Development Status

## Development Guidelines

### Testing Requirements

- **All test files MUST be in `tests/` directory**
- Test input: `tests/*.c`
- Test output: `tests/*.asm`, `tests/*.o`, etc.
- **NEVER write to `/tmp` or locations outside project**
- Keep all artifacts in `tests/` for version control

## ✅ Completed Components

### Phase 1: Basic Structure ✓

- Project directory structure created
- CMakeLists.txt configured for ZOS builds (includes `zde cmake` flow and verbose target)
- Makefile for desktop testing
- Header files with complete type definitions
- Version management system

### Phase 2: Lexer/Tokenizer ✓

- Complete C99 token support
- Keywords recognition
- Operators (single and multi-character)
- String and character literals
- Number literals (decimal and hexadecimal)
- Comments (// and /* */)
- Proper line/column tracking
- **TESTED AND WORKING**

### Phase 3: Parser ✓ COMPLETE

- ✅ Operator precedence climbing (factor/term hierarchy)
- ✅ Binary operators (+, -, *, /, %)
- ✅ Comparison operators (==, !=, <, >, <=, >=)
- ✅ Assignment expressions (x = 5)
- ✅ Variable declarations (int x; int x = value;)
- ✅ Function calls (add(x, y))
- ✅ Compound statements with statement lists
- ✅ Return statements with expressions
- ✅ If/else statements
- ✅ While loops
- ✅ For loops (init, condition, increment, body)
- ✅ Proper AST construction and traversal
- ✅ Program node with multiple functions
- **FULLY TESTED AND WORKING**

### Phase 4: Symbol Table ✓

- ✅ Basic hash table implemented
- ⏳ Needs full integration with parser
- ⏳ Needs type checking
- ⏳ Needs scope management

### Phase 5: Code Generator ✓ FUNCTIONAL

- ✅ Complete AST traversal
- ✅ Expression code generation with stack manipulation
- ✅ Binary operators (add, sub, mul, div, mod)
- ✅ Comparison operators with proper Z80 flags
- ✅ Variable storage and access (global labels)
- ✅ Function definitions and calls
- ✅ Stack-based argument passing
- ✅ Return statements with values
- ✅ If/else with conditional jumps (JP Z, JP NZ)
- ✅ While loops with loop/end labels
- ✅ For loops with init/condition/increment
- ✅ Runtime library (__mul_a_l, __div_a_l, __mod_a_l)
- **GENERATES WORKING Z80 ASSEMBLY**

## 🚧 In Progress

### Phase 5: Code Generator (incomplete tasks)

- ❌ Stack-based local variables (currently use global labels)
- ❌ Parameters/locals are treated as globals in codegen

## ⏳ Not Started

### Advanced Features

- ❌ Array access and pointers
- ❌ Structs and unions
- ❌ Type checking and semantic analysis
- ❌ Optimizations

### Phase 6: Testing

- ✅ Host regression suite in `tests/test*.c` compiles; target run passes
- ✅ ZOS regression suite in `tests/test*.c` compiles; target run passes

### Phase 7: Optimizations

- ❌ Not started

### Phase 8: Documentation

- ⚠️ README/SCOPE/USAGE updated; fuller docs still needed

## Next Steps for Iteration

### Critical Issues to Fix

1. 🟡 **Stack-based variables** - Replace global labels with proper stack frames
   - Currently all variables are global labels (x:, y:, sum:)
   - Should use: `ld (ix+offset), a` for local variables
   - Requires: Function prologue/epilogue with IX register

### Completed Features ✅

1. ✅ **Control flow statements** - if/else, while, for loops
2. ✅ **Comparison operators** - ==, !=, <, >, <=, >=
3. ✅ **Binary arithmetic** - +, -, *, /, %
4. ✅ **Variable declarations and assignments**
5. ✅ **Function calls with stack-based arguments**
6. ✅ **Runtime library** for mul/div/mod

## Current Test Status

- ✅ Host: `tests/test*.c` compile to `.asm` (includes add/expr/mul/div/mod/params/for/while/if/test1/test2/test_comp).
- ✅ Target: headless run passes; update `test.zs` to include `test_comp` so it runs on target.

**All tests write output to `tests/` only.**

## How to Test Current Build

**Remember: All test output goes to `tests/` directory!**

```bash
# Build
make clean && make

# Run all tests
for f in tests/test*.c; do
    echo "✓ $f"
    ./bin/cc "$f" "${f%.c}.asm"
done

# Individual tests (all output in tests/)
./bin/cc tests/test1.c tests/test1.asm        # Simple return
./bin/cc tests/test_expr.c tests/test_expr.asm  # Expression precedence
./bin/cc tests/test_add.c tests/test_add.asm    # Addition
./bin/cc tests/test_mul.c tests/test_mul.asm    # Multiplication
./bin/cc tests/test_div.c tests/test_div.asm    # Division
./bin/cc tests/test_mod.c tests/test_mod.asm    # Modulo
./bin/cc tests/test_if.c tests/test_if.asm      # If statement
./bin/cc tests/test_while.c tests/test_while.asm  # While loop
./bin/cc tests/test_for.c tests/test_for.asm    # For loop
./bin/cc tests/test2.c tests/test2.asm          # Multiple functions
./bin/cc tests/test_params.c tests/test_params.asm  # Function parameters
./bin/cc tests/test_comp.c tests/test_comp.asm  # Comprehensive test

# View generated assembly
cat tests/test1.asm
```

## Git Commit History

Recent commits:

- `3c0f0c8` - Implement stack-based (IX) function arguments, add `#ifdef VERBOSE` for log_verbose, update `test2.c`
- `616de03` - Improve test.py
- `dbbabf7` - Remove `test.sh`, update TESTING.md, add docs/ with usage/limitations/calling convention
- `a0de6b1` - Add return-code checks to Zeal tests and Python runner
- `1230668` - Rename target_ prefix and update README/SCOPE/STATUS
- `ac4d22b` - Refactor main error handling to reduce binary size
- `ddfac99` - Codegen refactor, runtime fixes, label rules, tests + TESTING.md
- `76dbc0d` - Stream input from 512-byte reader; align host/target IO and memory layout

## Architecture Notes

The compiler follows a traditional multi-pass design:

1. **Source → Lexer → Tokens**
2. **Tokens → Parser → AST**
3. **AST → Semantic Analyzer → Annotated AST**
4. **Annotated AST → Code Generator → Z80 Assembly**
5. **Assembly → Zealasm → Binary**

We are currently stuck at step 2 (Parser) for complex programs.

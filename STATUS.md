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
- ✅ Stack-based argument passing (IX frame for params)
- ✅ Return statements with values
- ✅ If/else with conditional jumps (JP Z, JP NZ)
- ✅ While loops with loop/end labels
- ✅ For loops with init/condition/increment
- ✅ Runtime library (__mul_a_l, __div_a_l, __mod_a_l)
- **GENERATES WORKING Z80 ASSEMBLY**

## 🚧 In Progress

### Phase 5: Code Generator (incomplete tasks)

- ❌ Stack-based local variables (currently use global labels)
- ❌ Local variable storage in stack frames (no local allocation yet)
- ❌ 16-bit locals/params and return values (8-bit only today)
- ❌ Pointer/array addressing and dereference codegen

## ⏳ Not Started

### Advanced Features

- ❌ Array access and pointers
- ❌ Structs and unions
- ❌ Type checking and semantic analysis
- ❌ Optimizations

### Phase 6: Testing

- ✅ Host regression suite in `tests/*.c` compiles; target run passes
- ✅ ZOS regression suite in `tests/*.c` compiles; target run passes

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
7. ✅ **Char literals and `char` type parsing**
8. ✅ **`long` type parsing and 32-bit codegen**

## Current Test Status

- ✅ Host: `tests/*.c` compile to `.asm` (includes simple_return/locals_params/assign/array/compares/comp/expr/for/if/math/params/pointer/struct/while/do_while/unary/string/char/ternary).
- ✅ Target: headless run passes; `test.zs` includes current tests.
- ⚠️ Expected-fail tests (tracked in `test.py`): array, pointer, struct, do_while, unary, string, ternary.

**All tests write output to `tests/` only.**

## How to Test Current Build

**Remember: All test output goes to `tests/` directory!**

```bash
# Build
make clean && make

# Run all tests
for f in tests/*.c; do
    echo "✓ $f"
    ./bin/cc "$f" "${f%.c}.asm"
done

# Individual tests (all output in tests/)
./bin/cc tests/simple_return.c tests/simple_return.asm        # Simple return
./bin/cc tests/expr.c tests/expr.asm  # Expression precedence
./bin/cc tests/assign.c tests/assign.asm  # Assignment chaining
./bin/cc tests/compares.c tests/compares.asm  # Comparisons
./bin/cc tests/math.c tests/math.asm  # Math ops
./bin/cc tests/if.c tests/if.asm      # If statement
./bin/cc tests/while.c tests/while.asm  # While loop
./bin/cc tests/do_while.c tests/do_while.asm  # Do/while (expected fail)
./bin/cc tests/unary.c tests/unary.asm  # Unary ops (expected fail)
./bin/cc tests/string.c tests/string.asm  # String literals (expected fail)
./bin/cc tests/char.c tests/char.asm    # Char literals (expected fail)
./bin/cc tests/for.c tests/for.asm    # For loop
./bin/cc tests/locals_params.c tests/locals_params.asm          # Locals + params
./bin/cc tests/params.c tests/params.asm  # Function parameters
./bin/cc tests/comp.c tests/comp.asm  # Comprehensive test

# View generated assembly
cat tests/simple_return.asm
```

## Git Commit History

Recent commits:

- `250bdda` - Ensure bin/zealasm exists for headless runs (copied from .zeal8bit/zealasm)
- `1d74350` - Streamed codegen, allocator updates, improved test.py (detect "Failed to compile"), expanded comp test
- `6e3f14d` - Implement stack-based (IX) function arguments, add `#ifdef VERBOSE` for log_verbose, update locals_params.c, docs
- `616de03` - Improve test.py
- `dbbabf7` - Remove test.sh, update TESTING.md, add docs/ usage/limitations/calling convention
- `a0de6b1` - Add return-code checks to Zeal tests and Python runner
- `1230668` - Rename target_ prefix and update README/SCOPE/STATUS
- `ac4d22b` - Refactor main error handling to reduce binary size

## Architecture Notes

The compiler follows a traditional multi-pass design:

1. **Source → Lexer → Tokens**
2. **Tokens → Parser → AST**
3. **AST → Semantic Analyzer → Annotated AST**
4. **Annotated AST → Code Generator → Z80 Assembly**
5. **Assembly → Zealasm → Binary**

We are currently generating Z80 assembly in step 4, without semantic analysis (step 3).

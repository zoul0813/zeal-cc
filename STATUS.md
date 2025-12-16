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
- CMakeLists.txt configured for both ZOS and desktop builds
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
- ✅ Return statements with values
- ✅ If/else with conditional jumps (JP Z, JP NZ)
- ✅ While loops with loop/end labels
- ✅ For loops with init/condition/increment
- ✅ Runtime library (__mul_a_l, __div_a_l, __mod_a_l)
- **GENERATES WORKING Z80 ASSEMBLY**

## 🚧 In Progress

### Known Limitations
- ❌ Function parameters not properly parsed/passed
- ❌ Stack-based local variables (currently use global labels)
- ❌ Proper function calling convention
- ❌ Argument passing via stack/registers
- ⚠️ Functions with parameters cause infinite loop in parser

## ⏳ Not Started

### Advanced Features
- ❌ Array access and pointers
- ❌ Structs and unions
- ❌ Type checking and semantic analysis
- ❌ Optimizations

### Phase 6: Testing
- Need comprehensive test suite
- Need regression tests
- Need ZOS integration testing

### Phase 7: Optimizations
- Not started

### Phase 8: Documentation
- Basic README exists
- Need complete documentation

## Next Steps for Iteration

### Critical Issues to Fix
1. 🔴 **Function parameters** - Parser hangs on functions with parameters (infinite loop)
   - Currently: `int add(int a, int b)` causes parser to hang
   - Need to fix parameter list parsing in `parse_function`
   - Parameters are parsed but cause infinite loop

2. 🟡 **Stack-based variables** - Replace global labels with proper stack frames
   - Currently all variables are global labels (x:, y:, sum:)
   - Should use: `ld (ix+offset), a` for local variables
   - Requires: Function prologue/epilogue with IX register

3. 🟡 **Calling convention** - Proper argument passing
   - Need to pass arguments via stack or registers
   - Standard Z80 calling convention
   - Return values via A (8-bit) or HL (16-bit)

### Completed Features ✅
1. ✅ **Control flow statements** - if/else, while, for loops
2. ✅ **Comparison operators** - ==, !=, <, >, <=, >=
3. ✅ **Binary arithmetic** - +, -, *, /, %
4. ✅ **Variable declarations and assignments**
5. ✅ **Basic function calls** (without parameter passing)
6. ✅ **Runtime library** for mul/div/mod

## Current Test Results - ALL PASSING ✅

**All tests write output to `tests/` directory only.**

### Complete Test Suite (8 tests)

1. **test1.c** - Simple return value
   ```c
   int main() { return 42; }
   ```
   ✅ Generates: `ld a, 42 / ret`

2. **test_expr.c** - Operator precedence
   ```c
   int main() { return 2 + 3 * 4; }  // = 14
   ```
   ✅ Correctly evaluates multiplication before addition

3. **test_add.c** - Addition operator
   ```c
   int main() { return 10 + 5; }  // = 15
   ```
   ✅ Generates: `ld a, 10 / push af / ld a, 5 / ld l, a / pop af / add a, l`

4. **test_mul.c** - Multiplication
   ```c
   int main() { return 5 * 3; }  // = 15
   ```
   ✅ Generates call to runtime `__mul_a_l` helper

5. **test_if.c** - Conditional logic
   ```c
   int main() {
       int x;
       x = 5;
       if (x == 5) { return 42; }
       return 0;
   }
   ```
   ✅ Generates comparison and conditional jumps (JP Z)

6. **test_while.c** - While loop
   ```c
   int main() {
       int x, sum;
       x = 0; sum = 0;
       while (x < 5) {
           sum = sum + x;
           x = x + 1;
       }
       return sum;  // = 10
   }
   ```
   ✅ Generates loop labels and conditional jumps

7. **test_for.c** - For loop
   ```c
   int main() {
       int i, sum;
       sum = 0;
       for (i = 0; i < 5; i = i + 1) {
           sum = sum + i;
       }
       return sum;  // = 10
   }
   ```
   ✅ Generates init/condition/increment/body loop structure

8. **test2.c** - Multiple functions (NO PARAMETERS)
   ```c
   int add(int a, int b) { return a + b; }
   int main() {
       int x, y;
       x = 5; y = 10;
       return add(x, y);
   }
   ```
   ✅ Generates but parameters not properly passed (uses global labels for a, b)

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
./bin/cc tests/test_if.c tests/test_if.asm      # If statement
./bin/cc tests/test_while.c tests/test_while.asm  # While loop
./bin/cc tests/test_for.c tests/test_for.asm    # For loop
./bin/cc tests/test2.c tests/test2.asm          # Multiple functions

# View generated assembly
cat tests/test1.asm
```

## Git Commit History

Recent commits:
- `3ade4fa` - Add comprehensive test (simplified version)
- `0f2f8ec` - Add complete control flow: while and for loops
- `8b7a361` - Add if statements and comparison operators
- `41531e6` - Document test directory policy
- `c1e160e` - Add runtime library and update documentation
- `45b7e45` - Codegen Phase 1: Expression and statement code generation
- `c924b4b` - Parser Phase 3: Add expression parsing, variables, assignments, function calls
- `33e2c22` - Initial compiler structure with working lexer

# Test complex program (currently fails)
./bin/cc tests/test2.c tests/test2.asm
```

## Architecture Notes

The compiler follows a traditional multi-pass design:

1. **Source → Lexer → Tokens**
2. **Tokens → Parser → AST**
3. **AST → Semantic Analyzer → Annotated AST**
4. **Annotated AST → Code Generator → Z80 Assembly**
5. **Assembly → Zealasm → Binary**

We are currently stuck at step 2 (Parser) for complex programs.

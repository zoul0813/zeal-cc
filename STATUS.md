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

### Phase 3: Parser ✓
- ✅ Operator precedence climbing (factor/term hierarchy)
- ✅ Binary operators (+, -, *, /, %)
- ✅ Assignment expressions (x = 5)
- ✅ Variable declarations (int x; int x = value;)
- ✅ Function calls (add(x, y))
- ✅ Compound statements with statement lists
- ✅ Return statements with expressions
- ✅ Proper AST construction and traversal
- ✅ Program node with multiple functions
- **TESTED AND WORKING**

### Phase 4: Symbol Table ✓
- ✅ Basic hash table implemented
- ⏳ Needs full integration with parser
- ⏳ Needs type checking
- ⏳ Needs scope management

### Phase 5: Code Generator ✓ (Basic)
- ✅ Complete AST traversal
- ✅ Expression code generation with stack manipulation
- ✅ Binary operators (add, sub, mul, div, mod)
- ✅ Variable storage and access (global labels)
- ✅ Function definitions and calls
- ✅ Return statements with values
- ✅ Runtime library (__mul_a_l, __div_a_l, __mod_a_l)
- **GENERATES WORKING Z80 ASSEMBLY**

## 🚧 In Progress

### Code Generator Improvements
- ⏳ Stack-based local variables (currently use global labels)
- ⏳ Proper function calling convention
- ⏳ Argument passing via stack/registers
- ⏳ Control flow (if, while, for)
- ⏳ Comparison operators

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

### Immediate Priority
1. ✅ **Control flow statements** - if/while/for
2. ✅ **Comparison operators** - ==, !=, <, >, <=, >=
3. ⏳ **Stack-based variables** - Replace global labels with proper stack frames
4. ⏳ **Function parameters** - Parse and pass arguments correctly
5. ⏳ **Calling convention** - Proper argument passing and return values

## Current Test Results

All tests write output to `tests/` directory only.

**Test 1 (Simple Return):** ✅ PASS
```c
int main() {
    return 42;
}
```
Generates: `ld a, 42 / ret`

**Test 2 (Multiple Functions):** ✅ PASS
```c
int add(int a, int b) {
    return a + b;
}

int main() {
    int x;
    int y;
    x = 5;
    y = 10;
    return add(x, y);
}
```
Generates working assembly with function calls and arithmetic.

**Test 3 (Expression Precedence):** ✅ PASS
```c
int main() {
    return 2 + 3 * 4;  // = 14
}
```
Correctly evaluates multiplication before addition.

## How to Test Current Build

**Remember: All test output goes to `tests/` directory!**

```bash
# Build
make clean && make

# Test suite (all output in tests/)
./bin/cc tests/test1.c tests/test1.asm        # Simple return
./bin/cc tests/test_expr.c tests/test_expr.asm  # Expression precedence
./bin/cc tests/test_add.c tests/test_add.asm    # Addition
./bin/cc tests/test_mul.c tests/test_mul.asm    # Multiplication
./bin/cc tests/test2.c tests/test2.asm          # Multiple functions

# View generated assembly
cat tests/test1.asm

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

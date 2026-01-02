echo TEST: tests/ternary.c
cc_parse tests/ternary.c tests/ternary.ast
: echo Failed to parse tests/ternary.c
? cc_semantic tests/ternary.ast
: echo Failed to validate tests/ternary.ast
? cc_codegen tests/ternary.ast tests/ternary.asm
: echo Failed to codegen tests/ternary.ast
? zealasm tests/ternary.asm tests/ternary.bin
? return tests/ternary.bin
: echo Failed to assemble tests/ternary.asm
: echo Failed to compile tests/ternary.c

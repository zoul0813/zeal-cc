echo TEST: tests/unary.c
cc_parse tests/unary.c tests/unary.ast
: echo Failed to parse tests/unary.c
? cc_semantic tests/unary.ast
: echo Failed to validate tests/unary.ast
? cc_codegen tests/unary.ast tests/unary.asm
: echo Failed to codegen tests/unary.ast
? zealasm tests/unary.asm tests/unary.bin
? return tests/unary.bin
: echo Failed to assemble tests/unary.asm
: echo Failed to compile tests/unary.c

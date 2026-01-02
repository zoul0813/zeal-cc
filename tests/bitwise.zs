echo TEST: tests/bitwise.c
cc_parse tests/bitwise.c tests/bitwise.ast
: echo Failed to parse tests/bitwise.c
? cc_semantic tests/bitwise.ast
: echo Failed to validate tests/bitwise.ast
? cc_codegen tests/bitwise.ast tests/bitwise.asm
: echo Failed to codegen tests/bitwise.ast
? zealasm tests/bitwise.asm tests/bitwise.bin
? return tests/bitwise.bin
: echo Failed to assemble tests/bitwise.asm
: echo Failed to compile tests/bitwise.c
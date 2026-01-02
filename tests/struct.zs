echo TEST: tests/struct.c
cc_parse tests/struct.c tests/struct.ast
: echo Failed to parse tests/struct.c
? cc_semantic tests/struct.ast
: echo Failed to validate tests/struct.ast
? cc_codegen tests/struct.ast tests/struct.asm
: echo Failed to codegen tests/struct.ast
? zealasm tests/struct.asm tests/struct.bin
? return tests/struct.bin
: echo Failed to assemble tests/struct.asm
: echo Failed to compile tests/struct.c
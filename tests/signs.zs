echo TEST: tests/signs.c
cc_parse tests/signs.c tests/signs.ast
: echo Failed to parse tests/signs.c
? cc_semantic tests/signs.ast
: echo Failed to validate tests/signs.ast
? cc_codegen tests/signs.ast tests/signs.asm
: echo Failed to codegen tests/signs.ast
? zealasm tests/signs.asm tests/signs.bin
? return tests/signs.bin
: echo Failed to assemble tests/signs.asm
: echo Failed to compile tests/signs.c
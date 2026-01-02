echo TEST: tests/break.c
cc_parse tests/break.c tests/break.ast
: echo Failed to parse tests/break.c
? cc_semantic tests/break.ast
: echo Failed to validate tests/break.ast
? cc_codegen tests/break.ast tests/break.asm
: echo Failed to codegen tests/break.ast
? zealasm tests/break.asm tests/break.bin
? return tests/break.bin
: echo Failed to assemble tests/break.asm
: echo Failed to compile tests/break.c
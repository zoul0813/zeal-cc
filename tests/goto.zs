echo TEST: tests/goto.c
cc_parse tests/goto.c tests/goto.ast
: echo Failed to parse tests/goto.c
? cc_semantic tests/goto.ast
: echo Failed to validate tests/goto.ast
? cc_codegen tests/goto.ast tests/goto.asm
: echo Failed to codegen tests/goto.ast
? zealasm tests/goto.asm tests/goto.bin
? return tests/goto.bin
: echo Failed to assemble tests/goto.asm
: echo Failed to compile tests/goto.c
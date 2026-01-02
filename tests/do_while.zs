echo TEST: tests/do_while.c
cc_parse tests/do_while.c tests/do_while.ast
: echo Failed to parse tests/do_while.c
? cc_semantic tests/do_while.ast
: echo Failed to validate tests/do_while.ast
? cc_codegen tests/do_while.ast tests/do_while.asm
: echo Failed to codegen tests/do_while.ast
? zealasm tests/do_while.asm tests/do_while.bin
? return tests/do_while.bin
: echo Failed to assemble tests/do_while.asm
: echo Failed to compile tests/do_while.c

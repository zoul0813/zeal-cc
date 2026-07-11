echo TEST: h:/tests/do_while.c
cc_parse h:/tests/do_while.c h:/tests/do_while.ast
: echo Failed to parse h:/tests/do_while.c
? cc_semantic h:/tests/do_while.ast
: echo Failed to validate h:/tests/do_while.ast
? cc_codegen h:/tests/do_while.ast h:/tests/do_while.asm
: echo Failed to codegen h:/tests/do_while.ast
? zealasm h:/tests/do_while.asm h:/tests/do_while.bin
? return h:/tests/do_while.bin
: echo Failed to assemble h:/tests/do_while.asm
: echo Failed to compile h:/tests/do_while.c

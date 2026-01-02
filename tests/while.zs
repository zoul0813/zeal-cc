echo TEST: h:/tests/while.c
cc_parse h:/tests/while.c h:/tests/while.ast
: echo Failed to parse h:/tests/while.c
? cc_semantic tests/while.ast
: echo Failed to validate tests/while.ast
? cc_codegen h:/tests/while.ast h:/tests/while.asm
: echo Failed to codegen h:/tests/while.ast
? zealasm h:/tests/while.asm h:/tests/while.bin
? return tests/while.bin
: echo Failed to assemble h:/tests/while.asm
: echo Failed to compile tests/while.c
echo TEST: h:/tests/compares.c
cc_parse h:/tests/compares.c h:/tests/compares.ast
: echo Failed to parse h:/tests/compares.c
? cc_semantic h:/tests/compares.ast
: echo Failed to validate h:/tests/compares.ast
? cc_codegen h:/tests/compares.ast h:/tests/compares.asm
: echo Failed to codegen h:/tests/compares.ast
? zealasm h:/tests/compares.asm h:/tests/compares.bin
? return h:/tests/compares.bin
: echo Failed to assemble h:/tests/compares.asm
: echo Failed to compile h:/tests/compares.c
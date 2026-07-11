echo TEST: h:/tests/break.c
cc_parse h:/tests/break.c h:/tests/break.ast
: echo Failed to parse h:/tests/break.c
? cc_semantic h:/tests/break.ast
: echo Failed to validate h:/tests/break.ast
? cc_codegen h:/tests/break.ast h:/tests/break.asm
: echo Failed to codegen h:/tests/break.ast
? zealasm h:/tests/break.asm h:/tests/break.bin
? return h:/tests/break.bin
: echo Failed to assemble h:/tests/break.asm
: echo Failed to compile h:/tests/break.c
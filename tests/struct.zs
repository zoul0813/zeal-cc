echo TEST: h:/tests/struct.c
cc_parse h:/tests/struct.c h:/tests/struct.ast
: echo Failed to parse h:/tests/struct.c
? cc_semantic h:/tests/struct.ast
: echo Failed to validate h:/tests/struct.ast
? cc_codegen h:/tests/struct.ast h:/tests/struct.asm
: echo Failed to codegen h:/tests/struct.ast
? zealasm h:/tests/struct.asm h:/tests/struct.bin
? return h:/tests/struct.bin
: echo Failed to assemble h:/tests/struct.asm
: echo Failed to compile h:/tests/struct.c
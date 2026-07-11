echo TEST: h:/tests/unary.c
cc_parse h:/tests/unary.c h:/tests/unary.ast
: echo Failed to parse h:/tests/unary.c
? cc_semantic h:/tests/unary.ast
: echo Failed to validate h:/tests/unary.ast
? cc_codegen h:/tests/unary.ast h:/tests/unary.asm
: echo Failed to codegen h:/tests/unary.ast
? zealasm h:/tests/unary.asm h:/tests/unary.bin
? return h:/tests/unary.bin
: echo Failed to assemble h:/tests/unary.asm
: echo Failed to compile h:/tests/unary.c

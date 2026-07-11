echo TEST: h:/tests/ternary.c
cc_parse h:/tests/ternary.c h:/tests/ternary.ast
: echo Failed to parse h:/tests/ternary.c
? cc_semantic h:/tests/ternary.ast
: echo Failed to validate h:/tests/ternary.ast
? cc_codegen h:/tests/ternary.ast h:/tests/ternary.asm
: echo Failed to codegen h:/tests/ternary.ast
? zealasm h:/tests/ternary.asm h:/tests/ternary.bin
? return h:/tests/ternary.bin
: echo Failed to assemble h:/tests/ternary.asm
: echo Failed to compile h:/tests/ternary.c

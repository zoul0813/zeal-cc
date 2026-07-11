echo TEST: h:/tests/bitwise.c
cc_parse h:/tests/bitwise.c h:/tests/bitwise.ast
: echo Failed to parse h:/tests/bitwise.c
? cc_semantic h:/tests/bitwise.ast
: echo Failed to validate h:/tests/bitwise.ast
? cc_codegen h:/tests/bitwise.ast h:/tests/bitwise.asm
: echo Failed to codegen h:/tests/bitwise.ast
? zealasm h:/tests/bitwise.asm h:/tests/bitwise.bin
? return h:/tests/bitwise.bin
: echo Failed to assemble h:/tests/bitwise.asm
: echo Failed to compile h:/tests/bitwise.c
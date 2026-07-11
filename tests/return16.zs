echo TEST: h:/tests/return16.c
cc_parse h:/tests/return16.c h:/tests/return16.ast
: echo Failed to parse h:/tests/return16.c
? cc_semantic h:/tests/return16.ast
: echo Failed to validate h:/tests/return16.ast
? cc_codegen h:/tests/return16.ast h:/tests/return16.asm
: echo Failed to codegen h:/tests/return16.ast
? zealasm h:/tests/return16.asm h:/tests/return16.bin
? return h:/tests/return16.bin
: echo Failed to assemble h:/tests/return16.asm
: echo Failed to compile h:/tests/return16.c

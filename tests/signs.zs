echo TEST: h:/tests/signs.c
cc_parse h:/tests/signs.c h:/tests/signs.ast
: echo Failed to parse h:/tests/signs.c
? cc_semantic h:/tests/signs.ast
: echo Failed to validate h:/tests/signs.ast
? cc_codegen h:/tests/signs.ast h:/tests/signs.asm
: echo Failed to codegen h:/tests/signs.ast
? zealasm h:/tests/signs.asm h:/tests/signs.bin
? return h:/tests/signs.bin
: echo Failed to assemble h:/tests/signs.asm
: echo Failed to compile h:/tests/signs.c
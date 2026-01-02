echo TEST: h:/tests/global.c
cc_parse h:/tests/global.c h:/tests/global.ast
: echo Failed to parse h:/tests/global.c
? cc_semantic tests/global.ast
: echo Failed to validate tests/global.ast
? cc_codegen h:/tests/global.ast h:/tests/global.asm
: echo Failed to codegen h:/tests/global.ast
? zealasm h:/tests/global.asm h:/tests/global.bin
? return tests/global.bin
: echo Failed to assemble h:/tests/global.asm
: echo Failed to compile tests/global.c
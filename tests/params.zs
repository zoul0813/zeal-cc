echo TEST: h:/tests/params.c
cc_parse h:/tests/params.c h:/tests/params.ast
: echo Failed to parse h:/tests/params.c
? cc_semantic tests/params.ast
: echo Failed to validate tests/params.ast
? cc_codegen h:/tests/params.ast h:/tests/params.asm
: echo Failed to codegen h:/tests/params.ast
? zealasm h:/tests/params.asm h:/tests/params.bin
? return tests/params.bin
: echo Failed to assemble h:/tests/params.asm
: echo Failed to compile tests/params.c
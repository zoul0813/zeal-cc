echo TEST: h:/tests/compares.c
cc_parse h:/tests/compares.c h:/tests/compares.ast
: echo Failed to parse h:/tests/compares.c
? cc_semantic tests/compares.ast
: echo Failed to validate tests/compares.ast
? cc_codegen h:/tests/compares.ast h:/tests/compares.asm
: echo Failed to codegen h:/tests/compares.ast
? zealasm h:/tests/compares.asm h:/tests/compares.bin
? return tests/compares.bin
: echo Failed to assemble h:/tests/compares.asm
: echo Failed to compile tests/compares.c
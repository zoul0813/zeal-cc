echo TEST: h:/tests/pointer.c
cc_parse h:/tests/pointer.c h:/tests/pointer.ast
: echo Failed to parse h:/tests/pointer.c
? cc_semantic tests/pointer.ast
: echo Failed to validate tests/pointer.ast
? cc_codegen h:/tests/pointer.ast h:/tests/pointer.asm
: echo Failed to codegen h:/tests/pointer.ast
? zealasm h:/tests/pointer.asm h:/tests/pointer.bin
? return tests/pointer.bin
: echo Failed to assemble h:/tests/pointer.asm
: echo Failed to compile tests/pointer.c
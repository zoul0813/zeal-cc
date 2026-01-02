echo TEST: h:/tests/assign.c
cc_parse h:/tests/assign.c h:/tests/assign.ast
: echo Failed to parse h:/tests/assign.c
? cc_semantic tests/assign.ast
: echo Failed to validate tests/assign.ast
? cc_codegen h:/tests/assign.ast h:/tests/assign.asm
: echo Failed to codegen h:/tests/assign.ast
? zealasm h:/tests/assign.asm h:/tests/assign.bin
? return tests/assign.bin
: echo Failed to assemble h:/tests/assign.asm
: echo Failed to compile tests/assign.c
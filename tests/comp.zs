echo TEST: h:/tests/comp.c
cc_parse h:/tests/comp.c h:/tests/comp.ast
: echo Failed to parse h:/tests/comp.c
? cc_semantic tests/comp.ast
: echo Failed to validate tests/comp.ast
? cc_codegen h:/tests/comp.ast h:/tests/comp.asm
: echo Failed to codegen h:/tests/comp.ast
? zealasm h:/tests/comp.asm h:/tests/comp.bin
? return tests/comp.bin
: echo Failed to assemble h:/tests/comp.asm
: echo Failed to compile tests/comp.c
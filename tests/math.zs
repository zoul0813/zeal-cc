echo TEST: h:/tests/math.c
cc_parse h:/tests/math.c h:/tests/math.ast
: echo Failed to parse h:/tests/math.c
? cc_semantic tests/math.ast
: echo Failed to validate tests/math.ast
? cc_codegen h:/tests/math.ast h:/tests/math.asm
: echo Failed to codegen h:/tests/math.ast
? zealasm h:/tests/math.asm h:/tests/math.bin
? return tests/math.bin
: echo Failed to assemble h:/tests/math.asm
: echo Failed to compile tests/math.c
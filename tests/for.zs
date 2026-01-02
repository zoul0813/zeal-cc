echo TEST: h:/tests/for.c
cc_parse h:/tests/for.c h:/tests/for.ast
: echo Failed to parse h:/tests/for.c
? cc_semantic tests/for.ast
: echo Failed to validate tests/for.ast
? cc_codegen h:/tests/for.ast h:/tests/for.asm
: echo Failed to codegen h:/tests/for.ast
? zealasm h:/tests/for.asm h:/tests/for.bin
? return tests/for.bin
: echo Failed to assemble h:/tests/for.asm
: echo Failed to compile tests/for.c
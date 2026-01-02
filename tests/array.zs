echo TEST: h:/tests/array.c
cc_parse h:/tests/array.c h:/tests/array.ast
: echo Failed to parse h:/tests/array.c
? cc_semantic tests/array.ast
: echo Failed to validate tests/array.ast
? cc_codegen h:/tests/array.ast h:/tests/array.asm
: echo Failed to codegen h:/tests/array.ast
? zealasm h:/tests/array.asm h:/tests/array.bin
? return tests/array.bin
: echo Failed to assemble h:/tests/array.asm
: echo Failed to compile tests/array.c

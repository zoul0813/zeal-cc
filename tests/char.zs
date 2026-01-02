echo TEST: h:/tests/char.c
cc_parse h:/tests/char.c h:/tests/char.ast
: echo Failed to parse h:/tests/char.c
? cc_semantic tests/char.ast
: echo Failed to validate tests/char.ast
? cc_codegen h:/tests/char.ast h:/tests/char.asm
: echo Failed to codegen h:/tests/char.ast
? zealasm h:/tests/char.asm h:/tests/char.bin
? return tests/char.bin
: echo Failed to assemble h:/tests/char.asm
: echo Failed to compile tests/char.c
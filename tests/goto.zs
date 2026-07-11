echo TEST: h:/tests/goto.c
cc_parse h:/tests/goto.c h:/tests/goto.ast
: echo Failed to parse h:/tests/goto.c
? cc_semantic h:/tests/goto.ast
: echo Failed to validate h:/tests/goto.ast
? cc_codegen h:/tests/goto.ast h:/tests/goto.asm
: echo Failed to codegen h:/tests/goto.ast
? zealasm h:/tests/goto.asm h:/tests/goto.bin
? return h:/tests/goto.bin
: echo Failed to assemble h:/tests/goto.asm
: echo Failed to compile h:/tests/goto.c
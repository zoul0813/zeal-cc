echo TEST: h:/tests/zealos.c
cc_parse h:/tests/zealos.c h:/tests/zealos.ast
: echo Failed to parse h:/tests/zealos.c
? cc_semantic tests/zealos.ast
: echo Failed to validate tests/zealos.ast
? cc_codegen h:/tests/zealos.ast h:/tests/zealos.asm
: echo Failed to codegen h:/tests/zealos.ast
? zealasm h:/tests/zealos.asm h:/tests/zealos.bin
? return tests/zealos.bin
: echo Failed to assemble h:/tests/zealos.asm
: echo Failed to compile tests/zealos.c
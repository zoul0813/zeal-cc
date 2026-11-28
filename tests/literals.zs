echo TEST: h:/tests/literals.c
cc_parse h:/tests/literals.c h:/tests/literals.ast
: echo Failed to parse h:/tests/literals.c
? cc_semantic h:/tests/literals.ast
: echo Failed to validate h:/tests/literals.ast
? cc_codegen h:/tests/literals.ast h:/tests/literals.asm
: echo Failed to codegen h:/tests/literals.ast
? zealasm h:/tests/literals.asm h:/tests/literals.bin
? return h:/tests/literals.bin
: echo Failed to assemble h:/tests/literals.asm
: echo Failed to compile h:/tests/literals.c
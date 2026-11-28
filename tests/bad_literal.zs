echo TEST: h:/tests/bad_literal.c
cc_parse h:/tests/bad_literal.c h:/tests/bad_literal.ast
: echo Failed to parse h:/tests/bad_literal.c
? cc_semantic h:/tests/bad_literal.ast
: echo Failed to validate h:/tests/bad_literal.ast
? cc_codegen h:/tests/bad_literal.ast h:/tests/bad_literal.asm
: echo Failed to codegen h:/tests/bad_literal.ast
? zealasm h:/tests/bad_literal.asm h:/tests/bad_literal.bin
? return h:/tests/bad_literal.bin
: echo Failed to assemble h:/tests/bad_literal.asm
: echo Failed to compile h:/tests/bad_literal.c

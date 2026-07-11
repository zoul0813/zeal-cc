echo TEST: h:/tests/expr.c
cc_parse h:/tests/expr.c h:/tests/expr.ast
: echo Failed to parse h:/tests/expr.c
? cc_semantic h:/tests/expr.ast
: echo Failed to validate h:/tests/expr.ast
? cc_codegen h:/tests/expr.ast h:/tests/expr.asm
: echo Failed to codegen h:/tests/expr.ast
? zealasm h:/tests/expr.asm h:/tests/expr.bin
? return h:/tests/expr.bin
: echo Failed to assemble h:/tests/expr.asm
: echo Failed to compile h:/tests/expr.c
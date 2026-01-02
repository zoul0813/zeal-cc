echo TEST: h:/tests/expr.c
cc_parse h:/tests/expr.c h:/tests/expr.ast
: echo Failed to parse h:/tests/expr.c
? cc_semantic tests/expr.ast
: echo Failed to validate tests/expr.ast
? cc_codegen h:/tests/expr.ast h:/tests/expr.asm
: echo Failed to codegen h:/tests/expr.ast
? zealasm h:/tests/expr.asm h:/tests/expr.bin
? return tests/expr.bin
: echo Failed to assemble h:/tests/expr.asm
: echo Failed to compile tests/expr.c
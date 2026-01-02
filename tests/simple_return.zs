echo TEST: h:/tests/simple_return.c
cc_parse h:/tests/simple_return.c h:/tests/simple_return.ast
: echo Failed to parse h:/tests/simple_return.c
? cc_semantic tests/simple_return.ast
: echo Failed to validate tests/simple_return.ast
? cc_codegen h:/tests/simple_return.ast h:/tests/simple_return.asm
: echo Failed to codegen h:/tests/simple_return.ast
? zealasm h:/tests/simple_return.asm h:/tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to assemble h:/tests/simple_return.asm
: echo Failed to compile tests/simple_return.c
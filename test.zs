cd h:/

echo TEST: h:/tests/assign.c
cc_parse h:/tests/assign.c h:/tests/assign.ast
? cc_codegen h:/tests/assign.ast h:/tests/assign.asm
? zealasm h:/tests/assign.asm h:/tests/assign.bin
? return tests/assign.bin
: echo Failed to compile tests/assign.c

echo TEST: h:/tests/char.c
cc_parse h:/tests/char.c h:/tests/char.ast
? cc_codegen h:/tests/char.ast h:/tests/char.asm
? zealasm h:/tests/char.asm h:/tests/char.bin
? return tests/char.bin
: echo Failed to compile tests/char.c

echo TEST: h:/tests/char_ptr.c
cc_parse h:/tests/char_ptr.c h:/tests/char_ptr.ast
? cc_codegen h:/tests/char_ptr.ast h:/tests/char_ptr.asm
? zealasm h:/tests/char_ptr.asm h:/tests/char_ptr.bin
? return tests/char_ptr.bin
: echo Failed to compile tests/char_ptr.c

echo TEST: h:/tests/comp.c
cc_parse h:/tests/comp.c h:/tests/comp.ast
? cc_codegen h:/tests/comp.ast h:/tests/comp.asm
? zealasm h:/tests/comp.asm h:/tests/comp.bin
? return tests/comp.bin
: echo Failed to compile tests/comp.c

echo TEST: h:/tests/compares.c
cc_parse h:/tests/compares.c h:/tests/compares.ast
? cc_codegen h:/tests/compares.ast h:/tests/compares.asm
? zealasm h:/tests/compares.asm h:/tests/compares.bin
? return tests/compares.bin
: echo Failed to compile tests/compares.c

echo TEST: h:/tests/expr.c
cc_parse h:/tests/expr.c h:/tests/expr.ast
? cc_codegen h:/tests/expr.ast h:/tests/expr.asm
? zealasm h:/tests/expr.asm h:/tests/expr.bin
? return tests/expr.bin
: echo Failed to compile tests/expr.c

echo TEST: h:/tests/for.c
cc_parse h:/tests/for.c h:/tests/for.ast
? cc_codegen h:/tests/for.ast h:/tests/for.asm
? zealasm h:/tests/for.asm h:/tests/for.bin
? return tests/for.bin
: echo Failed to compile tests/for.c

echo TEST: h:/tests/global.c
cc_parse h:/tests/global.c h:/tests/global.ast
? cc_codegen h:/tests/global.ast h:/tests/global.asm
? zealasm h:/tests/global.asm h:/tests/global.bin
? return tests/global.bin
: echo Failed to compile tests/global.c

echo TEST: h:/tests/if.c
cc_parse h:/tests/if.c h:/tests/if.ast
? cc_codegen h:/tests/if.ast h:/tests/if.asm
? zealasm h:/tests/if.asm h:/tests/if.bin
? return tests/if.bin
: echo Failed to compile tests/if.c

echo TEST: h:/tests/locals_params.c
cc_parse h:/tests/locals_params.c h:/tests/locals_params.ast
? cc_codegen h:/tests/locals_params.ast h:/tests/locals_params.asm
? zealasm h:/tests/locals_params.asm h:/tests/locals_params.bin
? return tests/locals_params.bin
: echo Failed to compile tests/locals_params.c

echo TEST: h:/tests/math.c
cc_parse h:/tests/math.c h:/tests/math.ast
? cc_codegen h:/tests/math.ast h:/tests/math.asm
? zealasm h:/tests/math.asm h:/tests/math.bin
? return tests/math.bin
: echo Failed to compile tests/math.c

echo TEST: h:/tests/params.c
cc_parse h:/tests/params.c h:/tests/params.ast
? cc_codegen h:/tests/params.ast h:/tests/params.asm
? zealasm h:/tests/params.asm h:/tests/params.bin
? return tests/params.bin
: echo Failed to compile tests/params.c

echo TEST: h:/tests/pointer.c
cc_parse h:/tests/pointer.c h:/tests/pointer.ast
? cc_codegen h:/tests/pointer.ast h:/tests/pointer.asm
? zealasm h:/tests/pointer.asm h:/tests/pointer.bin
? return tests/pointer.bin
: echo Failed to compile tests/pointer.c

echo TEST: h:/tests/simple_return.c
cc_parse h:/tests/simple_return.c h:/tests/simple_return.ast
? cc_codegen h:/tests/simple_return.ast h:/tests/simple_return.asm
? zealasm h:/tests/simple_return.asm h:/tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to compile tests/simple_return.c

echo TEST: h:/tests/string.c
cc_parse h:/tests/string.c h:/tests/string.ast
? cc_codegen h:/tests/string.ast h:/tests/string.asm
? zealasm h:/tests/string.asm h:/tests/string.bin
? return tests/string.bin
: echo Failed to compile tests/string.c

echo TEST: h:/tests/while.c
cc_parse h:/tests/while.c h:/tests/while.ast
? cc_codegen h:/tests/while.ast h:/tests/while.asm
? zealasm h:/tests/while.asm h:/tests/while.bin
? return tests/while.bin
: echo Failed to compile tests/while.c

echo !!! Complete !!!

; reset

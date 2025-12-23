cd h:/

echo TEST: h:/tests/assign.c
cc_parse h:/tests/assign.c h:/tests/assign.ast
: echo Failed to parse h:/tests/assign.c
? cc_codegen h:/tests/assign.ast h:/tests/assign.asm
: echo Failed to codegen h:/tests/assign.ast
? zealasm h:/tests/assign.asm h:/tests/assign.bin
? return tests/assign.bin
: echo Failed to assemble h:/tests/assign.asm
: echo Failed to compile tests/assign.c

echo TEST: h:/tests/char.c
cc_parse h:/tests/char.c h:/tests/char.ast
: echo Failed to parse h:/tests/char.c
? cc_codegen h:/tests/char.ast h:/tests/char.asm
: echo Failed to codegen h:/tests/char.ast
? zealasm h:/tests/char.asm h:/tests/char.bin
? return tests/char.bin
: echo Failed to assemble h:/tests/char.asm
: echo Failed to compile tests/char.c

echo TEST: h:/tests/char_ptr.c
cc_parse h:/tests/char_ptr.c h:/tests/char_ptr.ast
: echo Failed to parse h:/tests/char_ptr.c
? cc_codegen h:/tests/char_ptr.ast h:/tests/char_ptr.asm
: echo Failed to codegen h:/tests/char_ptr.ast
? zealasm h:/tests/char_ptr.asm h:/tests/char_ptr.bin
? return tests/char_ptr.bin
: echo Failed to assemble h:/tests/char_ptr.asm
: echo Failed to compile tests/char_ptr.c

echo TEST: h:/tests/comp.c
cc_parse h:/tests/comp.c h:/tests/comp.ast
: echo Failed to parse h:/tests/comp.c
? cc_codegen h:/tests/comp.ast h:/tests/comp.asm
: echo Failed to codegen h:/tests/comp.ast
? zealasm h:/tests/comp.asm h:/tests/comp.bin
? return tests/comp.bin
: echo Failed to assemble h:/tests/comp.asm
: echo Failed to compile tests/comp.c

echo TEST: h:/tests/compares.c
cc_parse h:/tests/compares.c h:/tests/compares.ast
: echo Failed to parse h:/tests/compares.c
? cc_codegen h:/tests/compares.ast h:/tests/compares.asm
: echo Failed to codegen h:/tests/compares.ast
? zealasm h:/tests/compares.asm h:/tests/compares.bin
? return tests/compares.bin
: echo Failed to assemble h:/tests/compares.asm
: echo Failed to compile tests/compares.c

echo TEST: h:/tests/expr.c
cc_parse h:/tests/expr.c h:/tests/expr.ast
: echo Failed to parse h:/tests/expr.c
? cc_codegen h:/tests/expr.ast h:/tests/expr.asm
: echo Failed to codegen h:/tests/expr.ast
? zealasm h:/tests/expr.asm h:/tests/expr.bin
? return tests/expr.bin
: echo Failed to assemble h:/tests/expr.asm
: echo Failed to compile tests/expr.c

echo TEST: h:/tests/for.c
cc_parse h:/tests/for.c h:/tests/for.ast
: echo Failed to parse h:/tests/for.c
? cc_codegen h:/tests/for.ast h:/tests/for.asm
: echo Failed to codegen h:/tests/for.ast
? zealasm h:/tests/for.asm h:/tests/for.bin
? return tests/for.bin
: echo Failed to assemble h:/tests/for.asm
: echo Failed to compile tests/for.c

echo TEST: h:/tests/global.c
cc_parse h:/tests/global.c h:/tests/global.ast
: echo Failed to parse h:/tests/global.c
? cc_codegen h:/tests/global.ast h:/tests/global.asm
: echo Failed to codegen h:/tests/global.ast
? zealasm h:/tests/global.asm h:/tests/global.bin
? return tests/global.bin
: echo Failed to assemble h:/tests/global.asm
: echo Failed to compile tests/global.c

echo TEST: h:/tests/if.c
cc_parse h:/tests/if.c h:/tests/if.ast
: echo Failed to parse h:/tests/if.c
? cc_codegen h:/tests/if.ast h:/tests/if.asm
: echo Failed to codegen h:/tests/if.ast
? zealasm h:/tests/if.asm h:/tests/if.bin
? return tests/if.bin
: echo Failed to assemble h:/tests/if.asm
: echo Failed to compile tests/if.c

echo TEST: h:/tests/locals_params.c
cc_parse h:/tests/locals_params.c h:/tests/locals_params.ast
: echo Failed to parse h:/tests/locals_params.c
? cc_codegen h:/tests/locals_params.ast h:/tests/locals_params.asm
: echo Failed to codegen h:/tests/locals_params.ast
? zealasm h:/tests/locals_params.asm h:/tests/locals_params.bin
? return tests/locals_params.bin
: echo Failed to assemble h:/tests/locals_params.asm
: echo Failed to compile tests/locals_params.c

echo TEST: h:/tests/math.c
cc_parse h:/tests/math.c h:/tests/math.ast
: echo Failed to parse h:/tests/math.c
? cc_codegen h:/tests/math.ast h:/tests/math.asm
: echo Failed to codegen h:/tests/math.ast
? zealasm h:/tests/math.asm h:/tests/math.bin
? return tests/math.bin
: echo Failed to assemble h:/tests/math.asm
: echo Failed to compile tests/math.c

echo TEST: h:/tests/params.c
cc_parse h:/tests/params.c h:/tests/params.ast
: echo Failed to parse h:/tests/params.c
? cc_codegen h:/tests/params.ast h:/tests/params.asm
: echo Failed to codegen h:/tests/params.ast
? zealasm h:/tests/params.asm h:/tests/params.bin
? return tests/params.bin
: echo Failed to assemble h:/tests/params.asm
: echo Failed to compile tests/params.c

echo TEST: h:/tests/pointer.c
cc_parse h:/tests/pointer.c h:/tests/pointer.ast
: echo Failed to parse h:/tests/pointer.c
? cc_codegen h:/tests/pointer.ast h:/tests/pointer.asm
: echo Failed to codegen h:/tests/pointer.ast
? zealasm h:/tests/pointer.asm h:/tests/pointer.bin
? return tests/pointer.bin
: echo Failed to assemble h:/tests/pointer.asm
: echo Failed to compile tests/pointer.c

echo TEST: h:/tests/simple_return.c
cc_parse h:/tests/simple_return.c h:/tests/simple_return.ast
: echo Failed to parse h:/tests/simple_return.c
? cc_codegen h:/tests/simple_return.ast h:/tests/simple_return.asm
: echo Failed to codegen h:/tests/simple_return.ast
? zealasm h:/tests/simple_return.asm h:/tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to assemble h:/tests/simple_return.asm
: echo Failed to compile tests/simple_return.c

echo TEST: h:/tests/string.c
cc_parse h:/tests/string.c h:/tests/string.ast
: echo Failed to parse h:/tests/string.c
? cc_codegen h:/tests/string.ast h:/tests/string.asm
: echo Failed to codegen h:/tests/string.ast
? zealasm h:/tests/string.asm h:/tests/string.bin
? return tests/string.bin
: echo Failed to assemble h:/tests/string.asm
: echo Failed to compile tests/string.c

echo TEST: h:/tests/while.c
cc_parse h:/tests/while.c h:/tests/while.ast
: echo Failed to parse h:/tests/while.c
? cc_codegen h:/tests/while.ast h:/tests/while.asm
: echo Failed to codegen h:/tests/while.ast
? zealasm h:/tests/while.asm h:/tests/while.bin
? return tests/while.bin
: echo Failed to assemble h:/tests/while.asm
: echo Failed to compile tests/while.c

echo !!! Complete !!!

; reset

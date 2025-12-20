cd h:/

cc h:/tests/test1.c h:/tests/test1.asm
? zealasm h:/tests/test1.asm h:/tests/test1.bin
: echo Failed to compile tests/test1.c

cc h:/tests/test2.c h:/tests/test2.asm
? zealasm h:/tests/test2.asm h:/tests/test2.bin
: echo Failed to compile tests/test2.c

cc h:/tests/test_add.c h:/tests/test_add.asm
? zealasm h:/tests/test_add.asm h:/tests/test_add.bin
: echo Failed to compile tests/test_add.c

cc h:/tests/test_expr.c h:/tests/test_expr.asm
? zealasm h:/tests/test_expr.asm h:/tests/test_expr.bin
: echo Failed to compile tests/test_expr.c

cc h:/tests/test_for.c h:/tests/test_for.asm
? zealasm h:/tests/test_for.asm h:/tests/test_for.bin
: echo Failed to compile tests/test_for.c

cc h:/tests/test_if.c h:/tests/test_if.asm
? zealasm h:/tests/test_if.asm h:/tests/test_if.bin
: echo Failed to compile tests/test_if.c

cc h:/tests/test_mul.c h:/tests/test_mul.asm
? zealasm h:/tests/test_mul.asm h:/tests/test_mul.bin
: echo Failed to compile tests/test_mul.c

cc h:/tests/test_div.c h:/tests/test_div.asm
? zealasm h:/tests/test_div.asm h:/tests/test_div.bin
: echo Failed to compile tests/test_div.c

cc h:/tests/test_mod.c h:/tests/test_mod.asm
? zealasm h:/tests/test_mod.asm h:/tests/test_mod.bin
: echo Failed to compile tests/test_mod.c

cc h:/tests/test_params.c h:/tests/test_params.asm
? zealasm h:/tests/test_params.asm h:/tests/test_params.bin
: echo Failed to compile tests/test_params.c

cc h:/tests/test_while.c h:/tests/test_while.asm
? zealasm h:/tests/test_while.asm h:/tests/test_while.bin
: echo Failed to compile tests/test_while.c

cc h:/tests/test_comp.c h:/tests/test_comp.asm
? zealasm h:/tests/test_comp.asm h:/tests/test_comp.bin
: echo Failed to compile tests/test_comp.c

echo !!! Complete !!!

; reset

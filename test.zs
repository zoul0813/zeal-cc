cd h:/

echo h:/tests/simple_return.c
cc h:/tests/simple_return.c h:/tests/simple_return.asm
? zealasm h:/tests/simple_return.asm h:/tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to compile tests/simple_return.c

echo h:/tests/locals_params.c
cc h:/tests/locals_params.c h:/tests/locals_params.asm
? zealasm h:/tests/locals_params.asm h:/tests/locals_params.bin
? return tests/locals_params.bin
: echo Failed to compile tests/locals_params.c

echo h:/tests/assign.c
cc h:/tests/assign.c h:/tests/assign.asm
? zealasm h:/tests/assign.asm h:/tests/assign.bin
? return tests/assign.bin
: echo Failed to compile tests/assign.c

echo h:/tests/array.c
cc h:/tests/array.c h:/tests/array.asm
? zealasm h:/tests/array.asm h:/tests/array.bin
? return tests/array.bin
: echo Failed to compile tests/array.c

echo h:/tests/compares.c
cc h:/tests/compares.c h:/tests/compares.asm
? zealasm h:/tests/compares.asm h:/tests/compares.bin
? return tests/compares.bin
: echo Failed to compile tests/compares.c

echo h:/tests/do_while.c
cc h:/tests/do_while.c h:/tests/do_while.asm
? zealasm h:/tests/do_while.asm h:/tests/do_while.bin
? return tests/do_while.bin
: echo Failed to compile tests/do_while.c

echo h:/tests/math.c
cc h:/tests/math.c h:/tests/math.asm
? zealasm h:/tests/math.asm h:/tests/math.bin
? return tests/math.bin
: echo Failed to compile tests/math.c

echo h:/tests/expr.c
cc h:/tests/expr.c h:/tests/expr.asm
? zealasm h:/tests/expr.asm h:/tests/expr.bin
? return tests/expr.bin
: echo Failed to compile tests/expr.c

echo h:/tests/string.c
cc h:/tests/string.c h:/tests/string.asm
? zealasm h:/tests/string.asm h:/tests/string.bin
? return tests/string.bin
: echo Failed to compile tests/string.c

echo h:/tests/for.c
cc h:/tests/for.c h:/tests/for.asm
? zealasm h:/tests/for.asm h:/tests/for.bin
? return tests/for.bin
: echo Failed to compile tests/for.c

echo h:/tests/if.c
cc h:/tests/if.c h:/tests/if.asm
? zealasm h:/tests/if.asm h:/tests/if.bin
? return tests/if.bin
: echo Failed to compile tests/if.c

echo h:/tests/params.c
cc h:/tests/params.c h:/tests/params.asm
? zealasm h:/tests/params.asm h:/tests/params.bin
? return tests/params.bin
: echo Failed to compile tests/params.c

echo h:/tests/pointer.c
cc h:/tests/pointer.c h:/tests/pointer.asm
? zealasm h:/tests/pointer.asm h:/tests/pointer.bin
? return tests/pointer.bin
: echo Failed to compile tests/pointer.c

echo h:/tests/struct.c
cc h:/tests/struct.c h:/tests/struct.asm
? zealasm h:/tests/struct.asm h:/tests/struct.bin
? return tests/struct.bin
: echo Failed to compile tests/struct.c

echo h:/tests/ternary.c
cc h:/tests/ternary.c h:/tests/ternary.asm
? zealasm h:/tests/ternary.asm h:/tests/ternary.bin
? return tests/ternary.bin
: echo Failed to compile tests/ternary.c

echo h:/tests/unary.c
cc h:/tests/unary.c h:/tests/unary.asm
? zealasm h:/tests/unary.asm h:/tests/unary.bin
? return tests/unary.bin
: echo Failed to compile tests/unary.c

echo h:/tests/while.c
cc h:/tests/while.c h:/tests/while.asm
? zealasm h:/tests/while.asm h:/tests/while.bin
? return tests/while.bin
: echo Failed to compile tests/while.c

echo h:/tests/comp.c
cc h:/tests/comp.c h:/tests/comp.asm
? zealasm h:/tests/comp.asm h:/tests/comp.bin
? return tests/comp.bin
: echo Failed to compile tests/comp.c

echo h:/tests/char.c
cc h:/tests/char.c h:/tests/char.asm
? zealasm h:/tests/char.asm h:/tests/char.bin
? return tests/char.bin
: echo Failed to compile tests/char.c

echo !!! Complete !!!

; reset

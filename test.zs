cd h:/

echo TEST: tests/array.c
cc_parse tests/array.c tests/array.ast
: echo Failed to parse tests/array.c
? cc_codegen tests/array.ast tests/array.asm
: echo Failed to codegen tests/array.ast
? zealasm tests/array.asm tests/array.bin
? return tests/array.bin
: echo Failed to assemble tests/array.asm
: echo Failed to compile tests/array.c

echo TEST: tests/assign.c
cc_parse tests/assign.c tests/assign.ast
: echo Failed to parse tests/assign.c
? cc_codegen tests/assign.ast tests/assign.asm
: echo Failed to codegen tests/assign.ast
? zealasm tests/assign.asm tests/assign.bin
? return tests/assign.bin
: echo Failed to assemble tests/assign.asm
: echo Failed to compile tests/assign.c

echo TEST: tests/char.c
cc_parse tests/char.c tests/char.ast
: echo Failed to parse tests/char.c
? cc_codegen tests/char.ast tests/char.asm
: echo Failed to codegen tests/char.ast
? zealasm tests/char.asm tests/char.bin
? return tests/char.bin
: echo Failed to assemble tests/char.asm
: echo Failed to compile tests/char.c

echo TEST: tests/comp.c
cc_parse tests/comp.c tests/comp.ast
: echo Failed to parse tests/comp.c
? cc_codegen tests/comp.ast tests/comp.asm
: echo Failed to codegen tests/comp.ast
? zealasm tests/comp.asm tests/comp.bin
? return tests/comp.bin
: echo Failed to assemble tests/comp.asm
: echo Failed to compile tests/comp.c

echo TEST: tests/compares.c
cc_parse tests/compares.c tests/compares.ast
: echo Failed to parse tests/compares.c
? cc_codegen tests/compares.ast tests/compares.asm
: echo Failed to codegen tests/compares.ast
? zealasm tests/compares.asm tests/compares.bin
? return tests/compares.bin
: echo Failed to assemble tests/compares.asm
: echo Failed to compile tests/compares.c

echo TEST: tests/do_while.c
cc_parse tests/do_while.c tests/do_while.ast
: echo Failed to parse tests/do_while.c
? cc_codegen tests/do_while.ast tests/do_while.asm
: echo Failed to codegen tests/do_while.ast
? zealasm tests/do_while.asm tests/do_while.bin
? return tests/do_while.bin
: echo Failed to assemble tests/do_while.asm
: echo Failed to compile tests/do_while.c
: echo Expected failure: tests/do_while.c

echo TEST: tests/expr.c
cc_parse tests/expr.c tests/expr.ast
: echo Failed to parse tests/expr.c
? cc_codegen tests/expr.ast tests/expr.asm
: echo Failed to codegen tests/expr.ast
? zealasm tests/expr.asm tests/expr.bin
? return tests/expr.bin
: echo Failed to assemble tests/expr.asm
: echo Failed to compile tests/expr.c

echo TEST: tests/for.c
cc_parse tests/for.c tests/for.ast
: echo Failed to parse tests/for.c
? cc_codegen tests/for.ast tests/for.asm
: echo Failed to codegen tests/for.ast
? zealasm tests/for.asm tests/for.bin
? return tests/for.bin
: echo Failed to assemble tests/for.asm
: echo Failed to compile tests/for.c

echo TEST: tests/global.c
cc_parse tests/global.c tests/global.ast
: echo Failed to parse tests/global.c
? cc_codegen tests/global.ast tests/global.asm
: echo Failed to codegen tests/global.ast
? zealasm tests/global.asm tests/global.bin
? return tests/global.bin
: echo Failed to assemble tests/global.asm
: echo Failed to compile tests/global.c

echo TEST: tests/if.c
cc_parse tests/if.c tests/if.ast
: echo Failed to parse tests/if.c
? cc_codegen tests/if.ast tests/if.asm
: echo Failed to codegen tests/if.ast
? zealasm tests/if.asm tests/if.bin
? return tests/if.bin
: echo Failed to assemble tests/if.asm
: echo Failed to compile tests/if.c

echo TEST: tests/bitwise.c
cc_parse tests/bitwise.c tests/bitwise.ast
: echo Failed to parse tests/bitwise.c
? cc_codegen tests/bitwise.ast tests/bitwise.asm
: echo Failed to codegen tests/bitwise.ast
? zealasm tests/bitwise.asm tests/bitwise.bin
? return tests/bitwise.bin
: echo Failed to assemble tests/bitwise.asm
: echo Failed to compile tests/bitwise.c

echo TEST: tests/math.c
cc_parse tests/math.c tests/math.ast
: echo Failed to parse tests/math.c
? cc_codegen tests/math.ast tests/math.asm
: echo Failed to codegen tests/math.ast
? zealasm tests/math.asm tests/math.bin
? return tests/math.bin
: echo Failed to assemble tests/math.asm
: echo Failed to compile tests/math.c

echo TEST: tests/params.c
cc_parse tests/params.c tests/params.ast
: echo Failed to parse tests/params.c
? cc_codegen tests/params.ast tests/params.asm
: echo Failed to codegen tests/params.ast
? zealasm tests/params.asm tests/params.bin
? return tests/params.bin
: echo Failed to assemble tests/params.asm
: echo Failed to compile tests/params.c

echo TEST: tests/pointer.c
cc_parse tests/pointer.c tests/pointer.ast
: echo Failed to parse tests/pointer.c
? cc_codegen tests/pointer.ast tests/pointer.asm
: echo Failed to codegen tests/pointer.ast
? zealasm tests/pointer.asm tests/pointer.bin
? return tests/pointer.bin
: echo Failed to assemble tests/pointer.asm
: echo Failed to compile tests/pointer.c

echo TEST: tests/simple_return.c
cc_parse tests/simple_return.c tests/simple_return.ast
: echo Failed to parse tests/simple_return.c
? cc_codegen tests/simple_return.ast tests/simple_return.asm
: echo Failed to codegen tests/simple_return.ast
? zealasm tests/simple_return.asm tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to assemble tests/simple_return.asm
: echo Failed to compile tests/simple_return.c

echo TEST: tests/struct.c
cc_parse tests/struct.c tests/struct.ast
: echo Failed to parse tests/struct.c
? cc_codegen tests/struct.ast tests/struct.asm
: echo Failed to codegen tests/struct.ast
? zealasm tests/struct.asm tests/struct.bin
? return tests/struct.bin
: echo Failed to assemble tests/struct.asm
: echo Failed to compile tests/struct.c
: echo Expected failure: tests/struct.c

echo TEST: tests/signs.c
cc_parse tests/signs.c tests/signs.ast
: echo Failed to parse tests/signs.c
? cc_codegen tests/signs.ast tests/signs.asm
: echo Failed to codegen tests/signs.ast
? zealasm tests/signs.asm tests/signs.bin
? return tests/signs.bin
: echo Failed to assemble tests/signs.asm
: echo Failed to compile tests/signs.c

echo TEST: tests/ternary.c
cc_parse tests/ternary.c tests/ternary.ast
: echo Failed to parse tests/ternary.c
? cc_codegen tests/ternary.ast tests/ternary.asm
: echo Failed to codegen tests/ternary.ast
? zealasm tests/ternary.asm tests/ternary.bin
? return tests/ternary.bin
: echo Failed to assemble tests/ternary.asm
: echo Failed to compile tests/ternary.c
: echo Expected failure: tests/ternary.c

echo TEST: tests/unary.c
cc_parse tests/unary.c tests/unary.ast
: echo Failed to parse tests/unary.c
? cc_codegen tests/unary.ast tests/unary.asm
: echo Failed to codegen tests/unary.ast
? zealasm tests/unary.asm tests/unary.bin
? return tests/unary.bin
: echo Failed to assemble tests/unary.asm
: echo Failed to compile tests/unary.c

echo TEST: tests/while.c
cc_parse tests/while.c tests/while.ast
: echo Failed to parse tests/while.c
? cc_codegen tests/while.ast tests/while.asm
: echo Failed to codegen tests/while.ast
? zealasm tests/while.asm tests/while.bin
? return tests/while.bin
: echo Failed to assemble tests/while.asm
: echo Failed to compile tests/while.c

echo TEST: tests/return16.c
cc_parse tests/return16.c tests/return16.ast
: echo Failed to parse tests/return16.c
? cc_codegen tests/return16.ast tests/return16.asm
: echo Failed to codegen tests/return16.ast
? zealasm tests/return16.asm tests/return16.bin
? return tests/return16.bin
: echo Failed to assemble tests/return16.asm
: echo Failed to compile tests/return16.c

echo TEST: tests/zealos.c
cc_parse tests/zealos.c tests/zealos.ast
: echo Failed to parse tests/zealos.c
? cc_codegen tests/zealos.ast tests/zealos.asm
: echo Failed to codegen tests/zealos.ast
? zealasm tests/zealos.asm tests/zealos.bin
? return tests/zealos.bin
: echo Failed to assemble tests/zealos.asm
: echo Failed to compile tests/zealos.c

echo !!! Complete !!!

; reset

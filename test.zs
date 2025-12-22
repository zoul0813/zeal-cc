cd h:/

echo TEST: h:/tests/simple_return.c
cc h:/tests/simple_return.c h:/tests/simple_return.asm
? zealasm h:/tests/simple_return.asm h:/tests/simple_return.bin
? return tests/simple_return.bin
: echo Failed to compile tests/simple_return.c

echo !!! Complete !!!

; reset

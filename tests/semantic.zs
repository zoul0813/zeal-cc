echo TEST: tests/semantic.c
cc_parse tests/semantic.c tests/semantic.ast
: echo Failed to parse tests/semantic.c
? cc_semantic tests/semantic.ast
: echo Failed to validate tests/semantic.ast
? echo OK: tests/semantic.c
: echo Expected failure: tests/semantic.c
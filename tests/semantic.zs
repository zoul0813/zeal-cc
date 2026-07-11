echo TEST: h:/tests/semantic.c
cc_parse h:/tests/semantic.c h:/tests/semantic.ast
: echo Failed to parse h:/tests/semantic.c
? cc_semantic h:/tests/semantic.ast
: echo Failed to validate h:/tests/semantic.ast
? echo OK: h:/tests/semantic.c
: echo Expected failure: h:/tests/semantic.c
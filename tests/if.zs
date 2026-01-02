echo TEST: h:/tests/if.c
cc_parse h:/tests/if.c h:/tests/if.ast
: echo Failed to parse h:/tests/if.c
? cc_semantic tests/if.ast
: echo Failed to validate tests/if.ast
? cc_codegen h:/tests/if.ast h:/tests/if.asm
: echo Failed to codegen h:/tests/if.ast
? zealasm h:/tests/if.asm h:/tests/if.bin
? return tests/if.bin
: echo Failed to assemble h:/tests/if.asm
: echo Failed to compile tests/if.c
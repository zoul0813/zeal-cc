echo TEST: h:/tests/if.c
cc_parse h:/tests/if.c h:/tests/if.ast
: echo Failed to parse h:/tests/if.c
? cc_semantic h:/tests/if.ast
: echo Failed to validate h:/tests/if.ast
? cc_codegen h:/tests/if.ast h:/tests/if.asm
: echo Failed to codegen h:/tests/if.ast
? zealasm h:/tests/if.asm h:/tests/if.bin
? return h:/tests/if.bin
: echo Failed to assemble h:/tests/if.asm
: echo Failed to compile h:/tests/if.c
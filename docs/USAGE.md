# Usage

## ZOS

`make dist` builds the target executables inside ZDE. Copy the contents of
`dist/zeal-cc/bin/` into `T:/bin/` and `dist/zeal-cc/lib/` into `T:/lib/`.
Install Zealasm separately as `T:/bin/zealasm` and add `T:/bin/` to ZShell's PATH.
Run the compiler from your writable project directory. The resulting Bin-file
is written alongside the source. To try the optional hex dump, copy
`examples/hexdump/hexdump.c` and `sample.dat` there first.

```text
cc hexdump.c
./hexdump.bin
cc -S hexdump.c
cc hexdump.c listing.asm
```

Use single-file source paths without spaces. The driver creates `cc-work` in
the current directory and removes it after the build. If that name already
exists, the driver exits. Stage failures leave an existing output intact.
ZOS has no rename syscall. During the final copy the driver backs up an
existing destination in `cc-work/previous`. If the guest resets during
publication (observed on a full ZealFS EEPROM), save that backup before
removing the interrupted workspace. The destination may be incomplete.

## Host

Run `make`, then use `bin/cc_parse_darwin source.c program.ast`,
`bin/cc_semantic_darwin program.ast`, and
`bin/cc_codegen_darwin program.ast program.asm` from the project root.
Linux uses the `_linux` suffix. Each stage must succeed before the next runs.

## AST tools

`ast_dump` prints a human-readable view of a `.ast` file produced by `cc_parse`.

Host usage:
```
bin/ast_dump_darwin tests/simple_return.ast
```

Zeal usage:
```
ast_dump h:/tests/simple_return.ast
```

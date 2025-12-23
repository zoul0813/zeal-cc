# Usage

## Build

Desktop (macOS/Linux):
```
make
```

ZOS target (SDCC):
```
zde cmake
```
Verbose build (defines `VERBOSE` for `#ifdef VERBOSE`):
```
zde cmake --target verbose
```

## Run

Desktop:
```
./bin/cc input.c output.asm
```

ZOS:
```
cc input.c output.asm
```

The compiler reads `runtime/crt0.asm` and `runtime/runtime.asm` at runtime
when emitting output, so ensure the `runtime/` directory is available on the
target filesystem (for headless tests, `H:/runtime/...` from the repo root).

The compiler emits Z80 assembly compatible with Zealasm. The output is
assembled and linked separately by your toolchain.

## Testing

All tests and artifacts must stay in `tests/`. Never write outputs to `/tmp`.

Run desktop tests:
```
./test.py
```

Headless target testing uses `test.zs`. If you add a new test, update
`test.zs` so it runs on target and emits a return code.

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

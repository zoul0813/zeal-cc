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

## Run

Desktop:
```
./bin/cc input.c output.asm
```

ZOS:
```
cc input.c output.asm
```

The compiler emits Z80 assembly compatible with Zealasm. The output is
assembled and linked separately by your toolchain.

## Testing

All tests and artifacts must stay in `tests/`. Never write outputs to `/tmp`.

Run desktop tests:
```
./test.sh
```

Headless target testing uses `test.zs`. If you add a new test, update
`test.zs` so it runs on target and emits a return code.

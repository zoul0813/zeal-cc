# Building, testing, and debugging

Install ZDE with coreutils, ZShell, Zeal-Disk-Tool and Zeal-NativeEmulator.
Initialize the Zealasm submodule. Native tests use the dedicated
`zeal-headless` and `zdt` on PATH. Use `zeal-headless --version` and
`zeal-headless --skill` to see the installed debugger's capabilities.
The graphical `zeal-native` frontend is available for manual inspection.

```sh
make test                     # host infrastructure, sanitizers and generation
make test-media               # UART ROM and Zealasm, built inside ZDE
make accept                   # build, stage and execute the installed compiler
make debug-smoke              # verify a symbolic parser breakpoint
make debug CASE=simple_return STAGE=parse
```

The media build writes to `build/test-media`. For an existing UART ROM
with ZShell booting B:/autoexec.zs and the coreutils return command, set
`ZEAL_CC_ROM` and `ZEALASM`. Legacy `.zeal8bit/headless.img` and
`.zeal8bit/zealasm` are also recognized. Every run creates its own EEPROM
from the tracked autoexec, explicit TF image, HostFS tree, and guest script.
The only compiler binaries available to the guest come from the staged install.
On systems where the emulator and disk tool are installed only inside ZDE,
run `make dist test-media`, then `zde --no-tty exec python3 test.py --headless-only --no-build`.

The `TEST_CASES` manifest in `test.py` supplies every fixture's expected ZOS
exit byte or rejection stage. Add a C fixture and its manifest entry. The guest
driver executes each stage and stops on failure. The runner checks ordered
stage output, attributed execution, decimal exit bytes and suite completion.
Use in-program comparisons for full-width results. The example checks include
exact stdout and its missing-file behavior. Host generation never runs guest code.

`./test.py` builds and checks both platforms. `--host-only` and
`--headless-only` select one. `--no-build` uses existing host binaries or
the staged installation. `--headless-log` streams output and
`--headless-log-file FILE` retains a transcript. Failed guest runs retain
their media and command information under `tests/audit/guest-*/`.

## Debug a compiler stage

`make debug` loads the stage's SDCC map into the stdin debugger, preparing
the selected fixture's prerequisites in the guest. Supported stages are
`parse`, `semantic`, and `codegen`. At the parser console:

```text
bp _parser_parse_next
run 200000000
regs
mmu
rb _parser_parse_next 8
bc _parser_parse_next
run 50000000
quit
```

Check the PC against the matching map and the bytes against the packaged
binary. A successful emulator exit alone does not establish a breakpoint hit.
Batch acceptance uses `zeal-headless --no-reset` so the guest reset ends the run.
Debug sessions add `--console`. Use `--headless N` for a fixed T-state budget.

## Parser and AST safety limits

Integer literals use the supported 16-bit unsigned-default model: decimal,
octal (`010` is eight), hexadecimal and an optional `u`/`U` suffix, up to 65535.
Floating-point and long literals, malformed suffixes, missing quotes/comments,
unsupported escapes and overlong tokens are rejected. Identifiers allow 31 bytes;
string literals allow 95 decoded bytes. Embedded NUL strings are rejected because
the current AST stores C strings; character literal `'\0'` remains supported.

AST input must use format version 1 and its defined zero reserved fields, with
at most 512 strings, valid type/operator/tag/reference encodings, exact node and
section boundaries, and no trailing bytes. Pointer depth is limited to eight.
Tree depth is limited to 32 including the program node. The parser also bounds
active expression/unary/statement recursion to 32; parentheses consume several
parser calls, so this does not mean 32 nested parentheses are supported. Inputs
that exceed these resource limits fail rather than exhausting the target stack.
For a simple return expression, this allows 28 terms in a left-associative
binary chain, or 27 unbraced `if`/`else if` arms. Other surrounding constructs
consume depth too. Standalone codegen still requires a successful semantic pass:
binary-AST schema checks are not a substitute for language-level semantic checks.

`make test` includes ASan/UBSan checks for numeric boundaries, poisoned partial
nodes, allocator alignment/overflow/coalescing, malformed and truncated ASTs,
depth limits and deterministic AST mutations. Sanitizers run in native host
binaries; SDCC remains inside ZDE.

`zde cmake` checks the target maps as part of its default build. To print the
same memory report separately:

```sh
python3 tests/check_target_memory.py
```

The pool ends at `0xC300`; the 512-byte reader occupies `0xC300`–`0xC4FF`.
ZOS maps a private user stack page at program entry. The remaining stack region
starts at `0xC500`, giving 15,104 bytes through `0xFFFF`. The map check rejects
images/static storage that reach the reader buffer. Successful target execution
is also required to establish that a remaining pool is large enough.

For measured pool/stack stress checks, prepare instrumented stage builds:

```sh
python3 tests/check_target_memory.py --prepare-probes
zde -i
# Inside ZDE:
cd /src/tests/audit/target-probes
cmake -B build && cmake --build build
```

These four wrappers include the real stage and allocator sources, report peak allocated
block bytes (including headers), and sample stack canaries between `0xC500` and
`0xF000`. They require at least 8 KB of untouched canaries above the file buffer.
They are test artifacts, not replacement release binaries. Run them against the
normal fixtures and depth-boundary sources through HostFS. Their extra code makes
their pools slightly smaller than release builds. Canary measurements describe
the exercised inputs; they do not prove a bound for every possible source.

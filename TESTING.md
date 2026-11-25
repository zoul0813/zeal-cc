# Local Testing configuration

* Compile a ZOS kernel using headless.conf, this disables ZVB, TF, and uses the UART Video driver (rename os_with_romdisk.img to headless.img)
* Copy autoexec.zs to your B:/autoexec.zs (ensure H:/bin is in PATH, and you run H:/test.zs)
* Copy eeprom.img and headless.img to ./.zeal8bit/ in the repo root
* Copy zealasm to H:/bin (!!! headless native does NOT support tf.img !!!)

Put the dedicated `zeal-headless` executable on PATH (ZDE builds it in
`build-headless`). The runner uses `zeal-headless --headless --no-reset`;
the desktop `zeal-native` executable is not used for automated target tests.
!!! Make sure you have the latest version of ZShell, commit 1961d0d1a3a6d6d6a3bde3188bed61a5d18008af or newer !!!

`test.py` will automatically add a `reset` at the end of test.zs for automated testing
`test.zs` also prints `TEST: h:/tests/<name>.c` before each case to help
attribute headless errors; `test.py` uses these markers when parsing results.


The contents of my ./.zeal8bit folder in the repo looks like this

```sh
.zeal8bit
├── default.img
├── eeprom.img
├── headless.img
├── tf.img
└── zealasm
```

Within Zeal Native, the contents of H:/bin looks like this

```sh
H:/>tree bin
├─ cc_darwin
├─ cc
├─ zealasm
0 dirs, 3 files
```

You can run `zeal-native -r .zeal8bit/default.img -e .zeal8bit/eeprom.img -t .zeal8bit/tf.img` to run Native normally, with a ZVB enabled kernel build (`zde kernel zealemu`),
this will automatically run test.zs after starting, and allow you to run the various h:/tests/*.bin files manually to verify the output.  You can also run `h:/bin/cc` or `h:/bin/zealasm` manually.

For automated testing, run `./test.py` from the repository root. It builds the
host stages with Make and the ZOS stages with `zde cmake`, checks host generation,
and runs the target scripts in the emulator. A failed build stops the run before
stale binaries can be tested. Missing tools or images fail a requested target run.
SDCC stays inside ZDE; do not install or invoke a Mac SDCC toolchain.

Run host-only infrastructure and generation checks with:

```sh
make test
```

For generation checks against existing host binaries, use `./test.sh`. To build
and check host generation without ZDE, use `./test.py --host-only`.
Host generation checks do not execute Z80 binaries.

Use `./test.py --headless-only` to build and test only ZOS, `--headless-log` to
stream emulator output, and `--headless-log-file tests/headless.log` to retain it.
`--no-build` explicitly tests existing binaries. ZDE commands require Podman
access; run them with elevated permissions when necessary. Special target-tool
operations can be performed with `zde -i`.

The manifest `TEST_CASES` in `test.py` supplies the expected ZOS exit byte or the
expected rejection stage. Supported cases must show successful parser, semantic
and codegen stages, an attributed execution record, and the expected return.
The installed coreutils `return` command prints **decimal**, even though it prefixes
its value with `$`. ZOS exit status is eight bits; test a full 16-bit result inside
the C program and return a distinct failure byte when it differs.

All manifest cases must be present, and the suite must reach `!!! Complete !!!`.
Compile-only logs and missing results fail. Expected rejections require the
correct compiler stage and its normal rejection diagnostic; crashes or missing
input files are failures. Host and target cases share this manifest. Adding a C
fixture also requires its `.zs` script and an entry in `test.zs`; inventory checks
catch omissions.

The runner restores the exact original `test.zs` bytes after execution, including
on timeout, SIGINT, SIGTERM or SIGHUP. Emulator output is read incrementally, so a partial
line cannot block the timeout. If `.zeal8bit/tf.img` exists, it is passed explicitly
to avoid the emulator selecting an unrelated user-global TF image.

The Python infrastructure tests exercise missing/duplicate results, failed stages,
partial output timeouts, script restoration, and compiler I/O failures. Host-built
C harnesses inject syscall faults into both adapters; the ZOS harness uses mock
SDK headers and does not run SDCC on the Mac. These supplement actual ZOS execution.

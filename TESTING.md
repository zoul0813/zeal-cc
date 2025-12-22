# Local Testing configuration

* Compile a ZOS kernel using headless.conf, this disables ZVB, TF, and uses the UART Video driver (rename os_with_romdisk.img to headless.img)
* Copy autoexec.zs to your B:/autoexec.zs (ensure H:/bin is in PATH, and you run H:/test.zs)
* Copy eeprom.img and headless.img to ./.zeal8bit/ in the repo root
* Copy zealasm to H:/bin (!!! headless native does NOT support tf.img !!!)

!!! Make sure you have the "headless" version of Zeal Native, and `--headless --no-reset` is supported !!!
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

For automated testing, you can just run `./test.py` from the repo root on your host machine. This will compile zeal-cc for ZOS, and your host, then run the host version on all the C files in `tests/*.c`.
It will then run zeal-native in headless mode, forcing a system reset (ie; `--no-reset`) after executing `test.zs`

Use `./test.py --headless-only` to run just the headless test, `--headless-log` to print the full headless output, and `--headless-log-file <path>` to save it for debugging.

Note: `./test.py` and any `zde` commands require Podman access; run them with elevated permissions when prompted.

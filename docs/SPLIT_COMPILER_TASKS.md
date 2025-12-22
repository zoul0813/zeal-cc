Compiler Split Tasks

Goal: split the Zeal compiler into three binaries:
- `cc` (driver): invokes parser/codegen via ZOS exec so users still run `cc input.c output.asm`.
- `cc_parse`: lexer/parser only, writes a serialized AST file.
- `cc_codegen`: reads the AST file and emits ASM.

Tasks

1) Define the AST binary format
- Decide on a versioned header, endianness, and alignment.
- Add a string table (identifiers + literals) with index references.
- Define node tags and payloads for all supported AST node kinds.
- Document the format in `docs/AST_FORMAT.md`.

2) Shared code layout
- Move shared structs/helpers to `src/common/` (types, AST structs, symbol table, errors, reader/writer utilities).
- Introduce an AST serializer/deserializer module in `src/common/` (or a new `src/ast/`).
- Add small IO helpers for writing/reading the AST file (streaming).

3) `cc_parse` binary (lexer/parser)
- New entry point that parses `input.c` and writes `output.ast`.
- Serialize AST as it is produced to minimize memory.
- Emit non-fatal parse errors and exit non-zero if parsing fails.
- Keep only lexer, parser, and AST serialization code linked in.

4) `cc_codegen` binary
- New entry point that reads `input.ast` and writes `output.asm`.
- Deserialize the AST (stream or minimal in-memory) and generate code.
- Reuse codegen/runtime emission modules.

5) `cc` driver binary
- Parse arguments: `cc input.c output.asm`.
- Generate `output.ast` next to `input.c` (keep it as a build artifact).
- Invoke `cc_parse` then `cc_codegen` via ZOS exec.
- Ensure errors propagate (no cleanup of `output.ast`).

6) Build system changes
- CMake: add three executables for Zeal target: `cc`, `cc_parse`, `cc_codegen`.
- Makefile: add matching targets for host so it also produces three binaries.
- Keep host and Zeal behavior aligned (same CLI flow and artifacts).

7) Test/tooling changes
- Update `test.py` and `test.zs` to call the driver or directly call parse/codegen.
- Ensure `.ast` artifacts are cleaned between host/zeal runs.

8) Documentation
- Update `docs/USAGE.md` and `README.md` with the new flow.
- Add troubleshooting notes for `.ast` artifacts.

9) Target execution helper
- Add a target-specific `exec` helper in `src/target/*` so `cc` can invoke
  `cc_parse` and `cc_codegen` on both host and Zeal.

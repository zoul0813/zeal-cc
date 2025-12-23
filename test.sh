#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

UNAME_S="$(uname -s)"
case "$UNAME_S" in
  Darwin) ARCH="darwin" ;;
  Linux) ARCH="linux" ;;
  *) ARCH="$UNAME_S" ;;
esac

CC_PARSE="bin/cc_parse_${ARCH}"
CC_CODEGEN="bin/cc_codegen_${ARCH}"

if [[ ! -x "$CC_PARSE" ]]; then
  echo "Missing $CC_PARSE. Build host binaries first."
  exit 1
fi
if [[ ! -x "$CC_CODEGEN" ]]; then
  echo "Missing $CC_CODEGEN. Build host binaries first."
  exit 1
fi

rm -f tests/*.asm tests/*.bin tests/*.ast

TESTS=(
  assign
  array
  char
  comp
  compares
  do_while
  expr
  for
  global
  if
  math
  params
  pointer
  simple_return
  struct
  ternary
  unary
  while
)

EXPECTED_FAILS=(
  array
  do_while
  struct
  ternary
  unary
)

FAILED=0

run_test() {
  local name="$1"
  local src="tests/${name}.c"
  local ast="tests/${name}.ast"
  local asm="tests/${name}.asm"
  local expect_fail=0

  for fail in "${EXPECTED_FAILS[@]}"; do
    if [[ "$name" == "$fail" ]]; then
      expect_fail=1
      break
    fi
  done

  echo "TEST: ${src}"
  if ! "$CC_PARSE" "$src" "$ast"; then
    if [[ "$expect_fail" -eq 1 ]]; then
      echo "Failed to compile ${src}"
      echo "Expected failure: ${src}"
    else
      echo "Failed to compile ${src}"
      FAILED=1
    fi
    return
  fi
  if ! "$CC_CODEGEN" "$ast" "$asm"; then
    if [[ "$expect_fail" -eq 1 ]]; then
      echo "Failed to compile ${src}"
      echo "Expected failure: ${src}"
    else
      echo "Failed to compile ${src}"
      FAILED=1
    fi
    return
  fi
  if [[ "$expect_fail" -eq 1 ]]; then
    echo "Unexpected pass: ${src}"
    FAILED=1
    return
  fi
  echo "OK: ${src}"
}

for test_name in "${TESTS[@]}"; do
  run_test "$test_name"
done

echo "!!! Complete !!!"
exit "$FAILED"

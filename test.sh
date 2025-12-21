#!/bin/bash

# set -x

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Function to print test section headers
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

# Function to print test results
print_result() {
    local test_name="$1"
    local result="$2"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ "$result" -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Function to run a compilation test
test_compile() {
    local compiler="$1"
    local test_file="$2"
    local test_name="$3"

    if [ ! -f "$test_file" ]; then
        echo -e "${YELLOW}⚠${NC}  Skipping $test_name (file not found: $test_file)"
        return
    fi

    # Generate output .asm filename from input .c filename
    local asm_file="${test_file%.c}.asm"
    $compiler "$test_file" "$asm_file"
    print_result "$test_name" $?
}

ensure_reset_in_test_zs() {
    if [ -f "test.zs" ]; then
        # Remove trailing blanks, drop commented "; reset", ensure final line is "reset"
        tmpfile=$(mktemp)
        awk '{
            sub(/\r$/, "");
            lines[NR] = $0;
        }
        END {
            n = NR;
            while (n > 0 && lines[n] ~ /^[[:space:]]*$/) n--;
            while (n > 0 && lines[n] ~ /^[[:space:]]*;?[[:space:]]*reset[[:space:]]*$/) n--;
            for (i = 1; i <= n; i++) print lines[i];
            print "reset";
        }' test.zs > "$tmpfile" && mv "$tmpfile" test.zs
    fi
}

comment_reset_in_test_zs() {
    if [ -f "test.zs" ]; then
        tmpfile=$(mktemp)
        awk '{
            sub(/\r$/, "");
            lines[NR] = $0;
        }
        END {
            n = NR;
            while (n > 0 && lines[n] ~ /^[[:space:]]*$/) n--;
            while (n > 0 && lines[n] ~ /^[[:space:]]*;?[[:space:]]*reset[[:space:]]*$/) n--;
            for (i = 1; i <= n; i++) print lines[i];
            print "; reset";
        }' test.zs > "$tmpfile" && mv "$tmpfile" test.zs
    fi
}

run_headless_emulator() {
    local img="$1"
    local eeprom="$2"
    local test_name="$3"

    if ! command -v zeal-native >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠${NC}  Skipping $test_name (zeal-native not found)"
        return
    fi

    if [ ! -f "$img" ] || [ ! -f "$eeprom" ]; then
        echo -e "${YELLOW}⚠${NC}  Skipping $test_name (image(s) missing)"
        return
    fi

    local log
    log=$(zeal-native --headless --no-reset -r "$img" -e "$eeprom" 2>&1)
    local status=$?

    if echo "$log" | grep -qi "error occurred"; then
        echo "$log"
        print_result "$test_name" 1
        return
    fi

    local return_fail=0
    local return_report
    return_report=$(parse_return_results "$log")
    if [ -n "$return_report" ]; then
        echo "$return_report"
        if echo "$return_report" | grep -q "^RETURN_MISMATCH"; then
            return_fail=1
        fi
    fi

    if [ $return_fail -ne 0 ]; then
        print_result "$test_name" 1
    else
        print_result "$test_name" $status
    fi
}

expected_return_hex() {
    case "$1" in
        test1) echo "0C" ;;
        test2) echo "0F" ;;
        test_add) echo "0F" ;;
        test_comp) echo "78" ;;
        test_div) echo "03" ;;
        test_expr) echo "1C" ;;
        test_for) echo "0A" ;;
        test_if) echo "2A" ;;
        test_mod) echo "01" ;;
        test_mul) echo "0F" ;;
        test_params) echo "05" ;;
        test_while) echo "0A" ;;
        *) echo "" ;;
    esac
}

parse_return_results() {
    local log="$1"
    local lines=""
    local mismatches=0
    local output=""
    local exec_file=""
    local line
    local hex

    while IFS= read -r line; do
        line=${line%$'\r'}
        line=${line## }
        case "$line" in
            *Exec\ \'*\')
                exec_file=${line#*Exec \'}
                exec_file=${exec_file%\'}
                ;;
            *Returned\ \$*)
                hex=${line#*Returned \$}
                hex=${hex%% *}
                if [ -n "$exec_file" ] && [ -n "$hex" ]; then
                    lines="${lines}${exec_file} ${hex}"$'\n'
                    exec_file=""
                fi
                ;;
        esac
    done <<< "$log"

    if [ -z "$lines" ]; then
        return 0
    fi

    while IFS= read -r line; do
        [ -z "$line" ] && continue
        local path hex base expected
        path=$(echo "$line" | awk '{print $1}')
        hex=$(echo "$line" | awk '{print $2}')
        base=$(basename "$path" .bin)
        expected=$(expected_return_hex "$base")
        hex=$(echo "$hex" | tr '[:lower:]' '[:upper:]')
        if [ -n "$expected" ] && [ "$hex" != "$expected" ]; then
            output="${output}RETURN_MISMATCH ${base}: expected \$${expected}, got \$${hex}\n"
            mismatches=1
        else
            output="${output}RETURN_OK ${base}: \$${hex}\n"
        fi
    done <<< "$lines"

    if [ $mismatches -ne 0 ]; then
        printf "%b" "$output"
        return 0
    fi

    printf "%b" "$output"
    return 0
}

# Start testing
echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Zeal C Compiler Test Suite        ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"

# Test 1: Build ZOS target
print_header "Building ZOS Target"
zde cmake > /dev/null 2>&1
print_result "ZOS compilation (bin/cc)" $?

# Test 2: Build macOS target
print_header "Building macOS Target"
make clean > /dev/null 2>&1
make > /dev/null 2>&1
print_result "macOS compilation (bin/cc_darwin)" $?

# Test 3: Run compiler tests with macOS binary
print_header "Running Compiler Tests (macOS)"

if [ -f "bin/cc_darwin" ]; then
    # Test with example files if they exist
    for test_file in tests/*.c; do
        if [ -f "$test_file" ]; then
            test_name="Compile $(basename "$test_file")"
            test_compile "./bin/cc_darwin" "$test_file" "$test_name"
        fi
    done

    # If no test files found, run a simple inline test
    if [ $TESTS_RUN -eq 2 ]; then
        echo "Creating inline test..."
        cat > /tmp/test_inline.c << 'EOF'
int main() {
    int x = 42;
    return 0;
}
EOF
        test_compile "./bin/cc_darwin" "/tmp/test_inline.c" "Inline test code"
        rm -f /tmp/test_inline.c
    fi
else
    echo -e "${RED}✗${NC} macOS binary not found, skipping tests"
fi

# Test 4: Verify binary outputs exist
print_header "Verifying Build Artifacts"

if [ -f "bin/cc" ]; then
    print_result "ZOS binary exists (bin/cc)" 0
else
    print_result "ZOS binary exists (bin/cc)" 1
fi

if [ -f "bin/cc_darwin" ]; then
    print_result "macOS binary exists (bin/cc_darwin)" 0
else
    print_result "macOS binary exists (bin/cc_darwin)" 1
fi

if [ -f "debug/cc.cdb" ]; then
    print_result "ZOS debug symbols exist" 0
else
    print_result "ZOS debug symbols exist" 1
fi

# Test 5: Zeal-native headless smoke test (if available)
print_header "Zeal-native Headless Smoke Test"
ensure_reset_in_test_zs
run_headless_emulator ".zeal8bit/headless.img" ".zeal8bit/eeprom.img" "zeal-native headless boot"
comment_reset_in_test_zs

# Print summary
print_header "Test Summary"
echo -e "Total tests run:    ${BLUE}$TESTS_RUN${NC}"
echo -e "Tests passed:       ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed:       ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}╔════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║          All Tests Passed! ✓           ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════╝${NC}\n"
    exit 0
else
    echo -e "\n${RED}╔════════════════════════════════════════╗${NC}"
    echo -e "${RED}║         Some Tests Failed ✗            ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════╝${NC}\n"
    exit 1
fi

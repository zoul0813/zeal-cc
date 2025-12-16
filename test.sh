#!/bin/bash

set -x

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

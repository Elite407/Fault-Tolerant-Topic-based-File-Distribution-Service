#!/usr/bin/env bash
# ==============================================================================
# Comprehensive Integration & Fault-Tolerance Test Suite
# Topic-based File Distribution Service (Part 1, Part 2 Case 1 & Case 2, Enhanced)
# ==============================================================================

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

PASSED_TESTS=0
FAILED_TESTS=0
TOTAL_TESTS=0

TEST_DIR="$ROOT_DIR/tests_tmp"
DATASET_DIR="$TEST_DIR/dataset"
OUTPUT_DIR="$TEST_DIR/output"

SERVER_PID=0

cleanup() {
    if [ "$SERVER_PID" -ne 0 ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -9 "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=0
    fi
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT INT TERM

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    PASSED_TESTS=$((PASSED_TESTS + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    FAILED_TESTS=$((FAILED_TESTS + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

wait_for_port() {
    local port="$1"
    local retries=30
    while [ $retries -gt 0 ]; do
        if (echo > /dev/tcp/127.0.0.1/"$port") 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        retries=$((retries - 1))
    done
    return 1
}

stop_server() {
    if [ "$SERVER_PID" -ne 0 ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=0
    fi
    sleep 0.2
}

setup_dataset() {
    log_info "Creating synthetic test dataset in $DATASET_DIR..."
    rm -rf "$TEST_DIR"
    mkdir -p "$DATASET_DIR/topicA"
    mkdir -p "$DATASET_DIR/topicB"
    mkdir -p "$OUTPUT_DIR"

    # topicA files:
    # 1. Text file
    cat << 'EOF' > "$DATASET_DIR/topicA/file1.txt"
Antigravity Topic-Based File Distribution Service Test
This file tests basic ASCII payload delivery and buffer management.
Line 1
Line 2
Line 3
EOF

    # 2. Medium binary file (128 KB)
    if command -v head >/dev/null 2>&1 && [ -e /dev/urandom ]; then
        head -c 131072 /dev/urandom > "$DATASET_DIR/topicA/file2.bin"
    else
        dd if=/dev/zero of="$DATASET_DIR/topicA/file2.bin" bs=1024 count=128 status=none
    fi

    # 3. 1 MB binary file (multiple chunks)
    if command -v head >/dev/null 2>&1 && [ -e /dev/urandom ]; then
        head -c 1048576 /dev/urandom > "$DATASET_DIR/topicA/file3.bin"
    else
        dd if=/dev/zero of="$DATASET_DIR/topicA/file3.bin" bs=1024 count=1024 status=none
    fi

    # topicB files:
    echo "Topic B document test content." > "$DATASET_DIR/topicB/doc.txt"
}

# ------------------------------------------------------------------------------
# Test 1: Part 1 Build & Functional Transfer
# ------------------------------------------------------------------------------
test_part1() {
    log_info "=== Test Suite: Part 1 (Single/Multi-threaded) ==="
    
    make -C "$ROOT_DIR/Part1" clean >/dev/null
    if make -C "$ROOT_DIR/Part1" >/dev/null; then
        log_pass "Part 1 compiled successfully."
    else
        log_fail "Part 1 failed to compile."
        return 1
    fi

    local port=19001
    local out="$OUTPUT_DIR/p1_clean"
    mkdir -p "$out"

    "$ROOT_DIR/Part1/server" "$port" "$DATASET_DIR" multi > "$TEST_DIR/p1_server.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 1 server failed to bind port $port"
        stop_server
        return 1
    fi

    if "$ROOT_DIR/Part1/client" 127.0.0.1 "$port" topicA "$out" > "$TEST_DIR/p1_client.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$out/topicA" >/dev/null 2>&1; then
            log_pass "Part 1 file transfer matched test dataset exactly."
        else
            log_fail "Part 1 downloaded files do not match original files."
        fi
    else
        log_fail "Part 1 client exited with error."
    fi

    # Negative test: Non-existent topic
    local neg_out="$OUTPUT_DIR/p1_neg"
    mkdir -p "$neg_out"
    if "$ROOT_DIR/Part1/client" 127.0.0.1 "$port" non_existent_topic "$neg_out" > "$TEST_DIR/p1_neg.log" 2>&1; then
        log_pass "Part 1 client gracefully handled non-existent topic."
    else
        log_fail "Part 1 client crashed or errored ungracefully on non-existent topic."
    fi

    stop_server
}

# ------------------------------------------------------------------------------
# Test 2: Part 2 Case 1 Build, Baseline & Fault-Tolerance
# ------------------------------------------------------------------------------
test_part2_case1() {
    log_info "=== Test Suite: Part 2 Case 1 (Abort & Restart from Scratch) ==="

    make -C "$ROOT_DIR/Part2_Case1" clean >/dev/null
    if make -C "$ROOT_DIR/Part2_Case1" >/dev/null; then
        log_pass "Part 2 Case 1 compiled successfully."
    else
        log_fail "Part 2 Case 1 failed to compile."
        return 1
    fi

    local port=19002
    local out="$OUTPUT_DIR/p2c1_clean"
    mkdir -p "$out"

    # Subtest 2a: Baseline (failure_prob = 0.0)
    "$ROOT_DIR/Part2_Case1/server" "$port" "$DATASET_DIR" 0.0 > "$TEST_DIR/p2c1_clean_srv.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 2 Case 1 server failed to bind port $port"
        stop_server
        return 1
    fi

    if "$ROOT_DIR/Part2_Case1/client" 127.0.0.1 "$port" topicA "$out" > "$TEST_DIR/p2c1_clean_cli.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$out/topicA" >/dev/null 2>&1; then
            log_pass "Part 2 Case 1 baseline (0% failure) matched test dataset."
        else
            log_fail "Part 2 Case 1 baseline downloaded files differ from source."
        fi
    else
        log_fail "Part 2 Case 1 baseline client exited with error."
    fi
    stop_server

    # Subtest 2b: Fault Tolerance with Abort & Directory Wipe (failure_prob = 0.02)
    port=19003
    local fault_out="$OUTPUT_DIR/p2c1_fault"
    mkdir -p "$fault_out"

    "$ROOT_DIR/Part2_Case1/server" "$port" "$DATASET_DIR" 0.02 > "$TEST_DIR/p2c1_fault_srv.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 2 Case 1 fault server failed to bind port $port"
        stop_server
        return 1
    fi

    if "$ROOT_DIR/Part2_Case1/client" 127.0.0.1 "$port" topicA "$fault_out" > "$TEST_DIR/p2c1_fault_cli.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$fault_out/topicA" >/dev/null 2>&1; then
            log_pass "Part 2 Case 1 fault tolerance & directory wipe completed successfully."
        else
            log_fail "Part 2 Case 1 fault tolerance files differ from source."
        fi
    else
        log_fail "Part 2 Case 1 fault tolerance client exited with error."
    fi
    stop_server
}

# ------------------------------------------------------------------------------
# Test 3: Part 2 Case 2 Build, Baseline & Resumption Checkpoint
# ------------------------------------------------------------------------------
test_part2_case2() {
    log_info "=== Test Suite: Part 2 Case 2 (Stateful Session Resumption) ==="

    make -C "$ROOT_DIR/Part2_Case2" clean >/dev/null
    if make -C "$ROOT_DIR/Part2_Case2" >/dev/null; then
        log_pass "Part 2 Case 2 compiled successfully."
    else
        log_fail "Part 2 Case 2 failed to compile."
        return 1
    fi

    local port=19004
    local out="$OUTPUT_DIR/p2c2_clean"
    mkdir -p "$out"

    # Subtest 3a: Baseline (failure_prob = 0.0)
    "$ROOT_DIR/Part2_Case2/server" "$port" "$DATASET_DIR" 0.0 > "$TEST_DIR/p2c2_clean_srv.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 2 Case 2 server failed to bind port $port"
        stop_server
        return 1
    fi

    if "$ROOT_DIR/Part2_Case2/client" 127.0.0.1 "$port" topicA "$out" > "$TEST_DIR/p2c2_clean_cli.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$out/topicA" >/dev/null 2>&1; then
            log_pass "Part 2 Case 2 baseline (0% failure) matched test dataset."
        else
            log_fail "Part 2 Case 2 baseline files differ from source."
        fi
    else
        log_fail "Part 2 Case 2 baseline client exited with error."
    fi
    stop_server

    # Subtest 3b: Fault Tolerance & Checkpoint Resumption (failure_prob = 0.02)
    port=19005
    local fault_out="$OUTPUT_DIR/p2c2_fault"
    mkdir -p "$fault_out"

    "$ROOT_DIR/Part2_Case2/server" "$port" "$DATASET_DIR" 0.02 > "$TEST_DIR/p2c2_fault_srv.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 2 Case 2 fault server failed to bind port $port"
        stop_server
        return 1
    fi

    if "$ROOT_DIR/Part2_Case2/client" 127.0.0.1 "$port" topicA "$fault_out" > "$TEST_DIR/p2c2_fault_cli.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$fault_out/topicA" >/dev/null 2>&1; then
            log_pass "Part 2 Case 2 session resumption completed successfully with exact file match."
        else
            log_fail "Part 2 Case 2 resumed files differ from source."
        fi
    else
        log_fail "Part 2 Case 2 resumption client exited with error."
    fi
    stop_server
}

# ------------------------------------------------------------------------------
# Test 4: Part 2 Case 2 Enhanced (Multi-connection / Parallel Workers)
# ------------------------------------------------------------------------------
test_part2_case2_enhanced() {
    log_info "=== Test Suite: Part 2 Case 2 Enhanced (Multi-Worker Transfer) ==="

    make -C "$ROOT_DIR/Part2_Case2_Enhanced" clean >/dev/null
    if make -C "$ROOT_DIR/Part2_Case2_Enhanced" >/dev/null; then
        log_pass "Part 2 Case 2 Enhanced compiled successfully."
    else
        log_fail "Part 2 Case 2 Enhanced failed to compile."
        return 1
    fi

    local port=19006
    local out="$OUTPUT_DIR/p2c2e_clean"
    mkdir -p "$out"

    "$ROOT_DIR/Part2_Case2_Enhanced/server" "$port" "$DATASET_DIR" 0.0 > "$TEST_DIR/p2c2e_clean_srv.log" 2>&1 &
    SERVER_PID=$!

    if ! wait_for_port "$port"; then
        log_fail "Part 2 Case 2 Enhanced server failed to bind port $port"
        stop_server
        return 1
    fi

    # Run with 2 worker connections
    if "$ROOT_DIR/Part2_Case2_Enhanced/client" 127.0.0.1 "$port" topicA "$out" 2 > "$TEST_DIR/p2c2e_clean_cli.log" 2>&1; then
        if diff -r "$DATASET_DIR/topicA" "$out/topicA" >/dev/null 2>&1; then
            log_pass "Part 2 Case 2 Enhanced 2-worker transfer matched test dataset."
        else
            log_fail "Part 2 Case 2 Enhanced files differ from source."
        fi
    else
        log_fail "Part 2 Case 2 Enhanced client exited with error."
    fi
    stop_server
}

# ------------------------------------------------------------------------------
# Test 5: Verify FinalSubmission Directory Compiles Cleanly
# ------------------------------------------------------------------------------
test_final_submission_build() {
    log_info "=== Test Suite: FinalSubmission_Group_100_113 Compilation ==="
    local submission_dir="$ROOT_DIR/FinalSubmission_Group_100_113"

    for part in Part1 Part2_Case1 Part2_Case2 Part2_Case2_Enhanced; do
        if make -C "$submission_dir/$part" clean >/dev/null && make -C "$submission_dir/$part" >/dev/null; then
            log_pass "FinalSubmission $part compiled successfully."
        else
            log_fail "FinalSubmission $part failed to compile."
        fi
    done
}

# ------------------------------------------------------------------------------
# Main Execution
# ------------------------------------------------------------------------------
main() {
    echo -e "${CYAN}================================================================${NC}"
    echo -e "${CYAN}   Topic-based File Distribution Service - Automated Test Suite  ${NC}"
    echo -e "${CYAN}================================================================${NC}"

    setup_dataset

    test_part1
    test_part2_case1
    test_part2_case2
    test_part2_case2_enhanced
    test_final_submission_build

    echo -e "\n${CYAN}================================================================${NC}"
    echo -e "${CYAN}                       Test Suite Summary                       ${NC}"
    echo -e "${CYAN}================================================================${NC}"
    echo -e "Total Tests  : $TOTAL_TESTS"
    echo -e "Passed Tests : ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed Tests : $([ "$FAILED_TESTS" -eq 0 ] && echo -e "${GREEN}0${NC}" || echo -e "${RED}$FAILED_TESTS${NC}")"

    if [ "$FAILED_TESTS" -eq 0 ]; then
        echo -e "\n${GREEN}🎉 ALL TESTS PASSED!${NC}\n"
        exit 0
    else
        echo -e "\n${RED}❌ SOME TESTS FAILED! Check log files in $TEST_DIR${NC}\n"
        exit 1
    fi
}

main "$@"

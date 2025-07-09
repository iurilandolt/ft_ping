#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[0;37m'
GRAY='\033[0;90m'
BOLD='\033[1m'
NC='\033[0m'

TARGET="127.0.0.1"


TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
FAILED_TEST_NAMES=()

if [ "$1" = "-t" ] && [ -n "$2" ]; then
    TARGET="$2"
fi

extract_ping_stats() {
    local output="$1"
    echo "$output" | grep -E "(packets transmitted|received|time [0-9]+ms)" | head -1
}

compare_outputs() {
    local ft_stats="$1"
    local ping_stats="$2"
    
    # Extract key metrics
    local ft_time=$(echo "$ft_stats" | grep -o "time [0-9]*ms" | grep -o "[0-9]*")
    local ping_time=$(echo "$ping_stats" | grep -o "time [0-9]*ms" | grep -o "[0-9]*")
    local ft_tx=$(echo "$ft_stats" | grep -o "[0-9]* packets transmitted" | grep -o "^[0-9]*")
    local ping_tx=$(echo "$ping_stats" | grep -o "[0-9]* packets transmitted" | grep -o "^[0-9]*")
    local ft_rx=$(echo "$ft_stats" | grep -o ", [0-9]* received" | grep -o "[0-9]*")
    local ping_rx=$(echo "$ping_stats" | grep -o ", [0-9]* received" | grep -o "[0-9]*")
    
    if [ -n "$ft_time" ] && [ -n "$ping_time" ] && [ -n "$ft_tx" ] && [ -n "$ping_tx" ]; then
        if [ "$ft_tx" = "$ping_tx" ] && [ "$ft_rx" = "$ping_rx" ]; then
            local time_diff=$((ft_time - ping_time))
            if [ ${time_diff#-} -le 100 ]; then  # Allow 100ms difference
                echo -e "${GREEN}📊 Output comparison: ${BOLD}GOOD${NC} (tx: $ft_tx=$ping_tx, rx: $ft_rx=$ping_rx, time diff: ${time_diff}ms)"
            else
                echo -e "${YELLOW}📊 Output comparison: ${BOLD}TIMING DIFF${NC} (tx: $ft_tx=$ping_tx, rx: $ft_rx=$ping_rx, time diff: ${time_diff}ms)"
            fi
        else
            echo -e "${RED}📊 Output comparison: ${BOLD}PACKET MISMATCH${NC} (ft_ping: tx:$ft_tx/rx:$ft_rx, ping: tx:$ping_tx/rx:$ping_rx)"
        fi
    else
        echo -e "${GRAY}📊 Output comparison: ${BOLD}UNABLE TO COMPARE${NC}"
    fi
}

run_test() {
    local test_name="$1"
    local ft_command="$2"
    local ping_command="$3"
    local expected_exit_code="${4:-0}"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -e "\n${BOLD}${BLUE}🧪 $test_name${NC}"
    echo -e "${GRAY}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    echo -e "${CYAN}💻 Command:${NC} ${MAGENTA}$ft_command${NC}"
    
    # Run ft_ping and show full output
    echo -e "\n${CYAN}📤 ft_ping output:${NC}"
    local ft_output
    ft_output=$(timeout 10 $ft_command 2>&1)
    local ft_exit=$?
    echo "$ft_output"
    echo -e "${YELLOW}(exit code: $ft_exit)${NC}"
    
    # Run system ping and show full output
    echo -e "\n${CYAN}📤 system ping output:${NC}"
    local ping_output
    ping_output=$(timeout 10 $ping_command 2>&1)
    local ping_exit=$?
    echo "$ping_output"
    echo -e "${YELLOW}(exit code: $ping_exit)${NC}"
    
    # Extract statistics for comparison (keep the fancy stats)
    local ft_stats=$(extract_ping_stats "$ft_output")
    local ping_stats=$(extract_ping_stats "$ping_output")
    
    # Compare outputs
    compare_outputs "$ft_stats" "$ping_stats"
    
    # Test result
    if [ $ft_exit -eq $ping_exit ] || [ $ft_exit -eq $expected_exit_code ]; then
        echo -e "${GREEN}✅ PASS${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ FAIL${NC} - Exit codes don't match (expected: $expected_exit_code, ft_ping: $ft_exit, ping: $ping_exit)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("$test_name")
    fi
}

show_summary() {
    echo -e "\n${BOLD}${MAGENTA}📊 TEST SUMMARY${NC}"
    echo -e "${GRAY}════════════════════════════════════════════════════════════════════════════════${NC}"
    
    
    local success_rate=0
    if [ $TOTAL_TESTS -gt 0 ]; then
        success_rate=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    fi
    
    
    echo -e "${BOLD}${BLUE}📈 Overall Results:${NC}"
    echo -e "${CYAN}🎯 Target tested: ${BOLD}$TARGET${NC}"
    echo -e "${WHITE}📝 Total tests run: ${BOLD}$TOTAL_TESTS${NC}"
    echo -e "${GREEN}✅ Tests passed: ${BOLD}$PASSED_TESTS${NC}"
    echo -e "${RED}❌ Tests failed: ${BOLD}$FAILED_TESTS${NC}"
    echo -e "${YELLOW}📊 Success rate: ${BOLD}$success_rate%${NC}"
    
    
    echo ""
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${BOLD}${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    else
        echo -e "${BOLD}${RED}⚠️  SOME TESTS FAILED ⚠️${NC}"
        echo -e "${RED}$FAILED_TESTS out of $TOTAL_TESTS tests need attention.${NC}"
        
        
        echo -e "\n${RED}❌ Failed tests:${NC}"
        for failed_test in "${FAILED_TEST_NAMES[@]}"; do
            echo -e "${RED}   • $failed_test${NC}"
        done
    fi
}

echo -e "${BOLD}${MAGENTA}🏓 ft_ping Test Suite${NC}"
echo -e "${CYAN}🎯 Target: $TARGET${NC}"
echo -e "${GRAY}════════════════════════════════════════════════════════════════════════════════${NC}"

echo -e "\n${BOLD}${YELLOW}📏 Testing SIZE FLAG (-s)${NC}"
run_test "Size: default (56)" "./ft_ping -s 56 -c 1 $TARGET" "ping -s 56 -c 1 $TARGET"
run_test "Size: minimum (0)" "./ft_ping -s 0 -c 1 $TARGET" "ping -s 0 -c 1 $TARGET"
run_test "Size: large (1000)" "./ft_ping -s 1000 -c 1 $TARGET" "ping -s 1000 -c 1 $TARGET"
run_test "Size: invalid negative" "./ft_ping -s -1 -c 1 $TARGET" "ping -s -1 -c 1 $TARGET" 1
run_test "Size: invalid large negative" "./ft_ping -s -100 -c 1 $TARGET" "ping -s -100 -c 1 $TARGET" 1
run_test "Size: invalid too large (65508)" "./ft_ping -s 65508 -c 1 $TARGET" "ping -s 65508 -c 1 $TARGET" 1
run_test "Size: invalid INT_MAX" "./ft_ping -s 2147483647 -c 1 $TARGET" "ping -s 2147483647 -c 1 $TARGET" 1
run_test "Size: invalid non-numeric" "./ft_ping -s abc -c 1 $TARGET" "ping -s abc -c 1 $TARGET" 1

# TIMEOUT FLAG TESTS (8 tests)
echo -e "\n${BOLD}${YELLOW}⏱️  Testing TIMEOUT FLAG (-W)${NC}"
run_test "Timeout: default (4)" "./ft_ping -W 4 -c 1 $TARGET" "ping -W 4 -c 1 $TARGET"
run_test "Timeout: short (1)" "./ft_ping -W 1 -c 1 $TARGET" "ping -W 1 -c 1 $TARGET"
run_test "Timeout: unreachable host" "./ft_ping -W 2 -c 1 192.0.2.1" "ping -W 2 -c 1 192.0.2.1" 1
run_test "Timeout: invalid zero" "./ft_ping -W 0 -c 1 $TARGET" "ping -W 0 -c 1 $TARGET" 1
run_test "Timeout: invalid negative" "./ft_ping -W -1 -c 1 $TARGET" "ping -W -1 -c 1 $TARGET" 1
run_test "Timeout: invalid large negative" "./ft_ping -W -100 -c 1 $TARGET" "ping -W -100 -c 1 $TARGET" 1
run_test "Timeout: invalid too large" "./ft_ping -W 2147483647 -c 1 $TARGET" "ping -W 2147483647 -c 1 $TARGET" 1
run_test "Timeout: invalid non-numeric" "./ft_ping -W abc -c 1 $TARGET" "ping -W abc -c 1 $TARGET" 1

# COUNT FLAG TESTS (8 tests)
echo -e "\n${BOLD}${YELLOW}🔢 Testing COUNT FLAG (-c)${NC}"
run_test "Count: single packet" "./ft_ping -c 1 $TARGET" "ping -c 1 $TARGET"
run_test "Count: multiple packets" "./ft_ping -c 3 $TARGET" "ping -c 3 $TARGET"
run_test "Count: many packets" "./ft_ping -c 10 $TARGET" "ping -c 10 $TARGET"
run_test "Count: invalid zero" "./ft_ping -c 0 $TARGET" "ping -c 0 $TARGET" 1
run_test "Count: invalid negative" "./ft_ping -c -1 $TARGET" "ping -c -1 $TARGET" 1
run_test "Count: invalid large negative" "./ft_ping -c -100 $TARGET" "ping -c -100 $TARGET" 1
run_test "Count: very large valid" "./ft_ping -c 1000000 $TARGET" "ping -c 1000000 $TARGET"
run_test "Count: invalid non-numeric" "./ft_ping -c abc $TARGET" "ping -c abc $TARGET" 1

# PRELOAD FLAG TESTS (8 tests)
echo -e "\n${BOLD}${YELLOW}📦 Testing PRELOAD FLAG (-l)${NC}"
run_test "Preload: minimum (1)" "./ft_ping -l 1 -c 3 $TARGET" "ping -l 1 -c 3 $TARGET"
run_test "Preload: maximum (3)" "./ft_ping -l 3 -c 5 $TARGET" "ping -l 3 -c 5 $TARGET"
run_test "Preload: equals count" "./ft_ping -l 2 -c 2 $TARGET" "ping -l 2 -c 2 $TARGET"
run_test "Preload: invalid zero" "./ft_ping -l 0 -c 3 $TARGET" "ping -l 0 -c 3 $TARGET" 1
run_test "Preload: invalid negative" "./ft_ping -l -1 -c 3 $TARGET" "ping -l -1 -c 3 $TARGET" 1
run_test "Preload: invalid large negative" "./ft_ping -l -10 -c 3 $TARGET" "ping -l -10 -c 3 $TARGET" 1
run_test "Preload: invalid too high" "./ft_ping -l 5 -c 3 $TARGET" "ping -l 5 -c 3 $TARGET" 1
run_test "Preload: invalid non-numeric" "./ft_ping -l abc -c 3 $TARGET" "ping -l abc -c 3 $TARGET" 1

# VERBOSE FLAG TESTS (2 tests)
echo -e "\n${BOLD}${YELLOW}🔊 Testing VERBOSE FLAG (-v)${NC}"
run_test "Verbose: basic" "./ft_ping -v -c 2 $TARGET" "ping -v -c 2 $TARGET"
run_test "Verbose: with other flags" "./ft_ping -v -c 2 -s 100 $TARGET" "ping -v -c 2 -s 100 $TARGET"

# COMBINED FLAGS TESTS (10 tests)
echo -e "\n${BOLD}${YELLOW}🎯 Testing FLAG COMBINATIONS${NC}"
run_test "Combo: count + size" "./ft_ping -c 2 -s 100 $TARGET" "ping -c 2 -s 100 $TARGET"
run_test "Combo: count + timeout" "./ft_ping -c 2 -W 3 $TARGET" "ping -c 2 -W 3 $TARGET"
run_test "Combo: count + preload" "./ft_ping -c 4 -l 2 $TARGET" "ping -c 4 -l 2 $TARGET"
run_test "Combo: preload + timeout (timing test)" "./ft_ping -l 2 -c 2 -W 4 $TARGET" "ping -l 2 -c 2 -W 4 $TARGET"
run_test "Combo: size + timeout + verbose" "./ft_ping -s 200 -W 2 -v -c 2 $TARGET" "ping -s 200 -W 2 -v -c 2 $TARGET"
run_test "Combo: all flags normal" "./ft_ping -c 3 -s 64 -W 2 -l 1 -v $TARGET" "ping -c 3 -s 64 -W 2 -l 1 -v $TARGET"
run_test "Combo: preload = count (timing critical)" "./ft_ping -l 3 -c 3 -W 4 $TARGET" "ping -l 3 -c 3 -W 4 $TARGET"
run_test "Combo: timeout with unreachable" "./ft_ping -c 2 -W 1 -l 1 192.0.2.1" "ping -c 2 -W 1 -l 1 192.0.2.1" 1
run_test "Combo: large packet + preload" "./ft_ping -s 1000 -l 2 -c 3 $TARGET" "ping -s 1000 -l 2 -c 3 $TARGET"
run_test "Combo: invalid flag mix" "./ft_ping -c 0 -s -1 $TARGET" "ping -c 0 -s -1 $TARGET" 1

# ERROR HANDLING TESTS (5 tests)
echo -e "\n${BOLD}${YELLOW}🚫 Testing ERROR HANDLING${NC}"
run_test "Error: unknown flag" "./ft_ping -x $TARGET" "ping -x $TARGET" 1
run_test "Error: missing target" "./ft_ping -c 2" "ping -c 2" 1
run_test "Error: missing flag value" "./ft_ping -c $TARGET" "ping -c $TARGET" 1
run_test "Error: multiple invalid flags" "./ft_ping -c 0 -W 0 -l 5 $TARGET" "ping -c 0 -W 0 -l 5 $TARGET" 1
run_test "Error: flag without argument" "./ft_ping -s -c 1 $TARGET" "ping -s -c 1 $TARGET" 1

show_summary
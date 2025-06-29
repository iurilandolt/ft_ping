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

run_test() {
    local test_name="$1"
    local ft_command="$2"
    local ping_command="$3"
    local expected_exit_code="${4:-0}"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -e "\n${BOLD}${BLUE}🧪 Testing: $test_name${NC}"
    echo -e "${GRAY}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    
    echo -e "${CYAN}📤 ft_ping command:${NC} ${MAGENTA}$ft_command${NC}"
    echo -e "${CYAN}📤 system ping command:${NC} ${MAGENTA}$ping_command${NC}"
    echo -e "${YELLOW}🎯 Expected exit code:${NC} $expected_exit_code"
    echo ""
    
    
    echo -e "${BOLD}${BLUE}🚀 Running ft_ping...${NC}"
    local ft_output
    ft_output=$(timeout 10 $ft_command 2>&1)
    local ft_exit=$?
    
    echo -e "${CYAN}📋 ft_ping output:${NC}"
    echo -e "${WHITE}$ft_output${NC}"
    echo -e "${YELLOW}↩️  ft_ping exit code: ${BOLD}$ft_exit${NC}"
    echo ""
    
    
    echo -e "${BOLD}${BLUE}🚀 Running system ping...${NC}"
    local ping_output
    ping_output=$(timeout 10 $ping_command 2>&1)
    local ping_exit=$?
    
    echo -e "${CYAN}📋 system ping output:${NC}"
    echo -e "${WHITE}$ping_output${NC}"
    echo -e "${YELLOW}↩️  system ping exit code: ${BOLD}$ping_exit${NC}"
    echo ""
    
    
    echo -e "${BOLD}${BLUE}🔍 Test Result:${NC}"
    if [ $ft_exit -eq $ping_exit ] || [ $ft_exit -eq $expected_exit_code ]; then
        echo -e "${GREEN}✅ PASS${NC} - Exit codes match expectations!"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}❌ FAIL${NC} - Exit codes don't match!"
        echo -e "${RED}   Expected: $expected_exit_code${NC}"
        echo -e "${RED}   ft_ping got: $ft_exit${NC}"
        echo -e "${RED}   system ping got: $ping_exit${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("$test_name")
    fi
    
    echo -e "${GRAY}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
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

# echo -e "${BOLD}${MAGENTA}🏓 ft_ping Test Suite${NC}"
# echo -e "${CYAN}🎯 Target: $TARGET${NC}"
# echo -e "${GRAY}════════════════════════════════════════════════════════════════════════════════${NC}"

# echo -e "\n${BOLD}${YELLOW}📏 Testing SIZE FLAG (-s)${NC}"

# echo -e "\n${GREEN}✅ VALID SIZE TESTS${NC}"
# run_test "Size 0 (minimum)" "./ft_ping -s 0 -c 1 $TARGET" "ping -s 0 -c 1 $TARGET"
# run_test "Size 1" "./ft_ping -s 1 -c 1 $TARGET" "ping -s 1 -c 1 $TARGET"
# run_test "Size 8 (default ICMP data)" "./ft_ping -s 8 -c 1 $TARGET" "ping -s 8 -c 1 $TARGET"
# run_test "Size 56 (ping default)" "./ft_ping -s 56 -c 1 $TARGET" "ping -s 56 -c 1 $TARGET"
# run_test "Size 64" "./ft_ping -s 64 -c 1 $TARGET" "ping -s 64 -c 1 $TARGET"
# run_test "Size 100" "./ft_ping -s 100 -c 1 $TARGET" "ping -s 100 -c 1 $TARGET"
# run_test "Size 1000" "./ft_ping -s 1000 -c 1 $TARGET" "ping -s 1000 -c 1 $TARGET"
# run_test "Size 1472 (MTU safe)" "./ft_ping -s 1472 -c 1 $TARGET" "ping -s 1472 -c 1 $TARGET"
# run_test "Size 9000 (jumbo frame)" "./ft_ping -s 9000 -c 1 $TARGET" "ping -s 9000 -c 1 $TARGET"
# run_test "Size 65507 (theoretical max)" "./ft_ping -s 65507 -c 1 $TARGET" "ping -s 65507 -c 1 $TARGET"

# echo -e "\n${RED}❌ INVALID SIZE TESTS${NC}"
# run_test "Size -1 (negative)" "./ft_ping -s -1 -c 1 $TARGET" "ping -s -1 -c 1 $TARGET" 1
# run_test "Size -100 (negative)" "./ft_ping -s -100 -c 1 $TARGET" "ping -s -100 -c 1 $TARGET" 1
# run_test "Size -2147483648 (INT_MIN)" "./ft_ping -s -2147483648 -c 1 $TARGET" "ping -s -2147483648 -c 1 $TARGET" 1

# echo -e "\n${YELLOW}🔢 BOUNDARY SIZE TESTS${NC}"
# run_test "Size 65508 (max + 1)" "./ft_ping -s 65508 -c 1 $TARGET" "ping -s 65508 -c 1 $TARGET" 1
# run_test "Size 70000 (too large)" "./ft_ping -s 70000 -c 1 $TARGET" "ping -s 70000 -c 1 $TARGET" 1
# run_test "Size 2147483647 (INT_MAX)" "./ft_ping -s 2147483647 -c 1 $TARGET" "ping -s 2147483647 -c 1 $TARGET" 1
# run_test "Size 4294967295 (UINT_MAX)" "./ft_ping -s 4294967295 -c 1 $TARGET" "ping -s 4294967295 -c 1 $TARGET" 1

# echo -e "\n${MAGENTA}🔤 NON-NUMERIC SIZE TESTS${NC}"
# run_test "Size abc (letters)" "./ft_ping -s abc -c 1 $TARGET" "ping -s abc -c 1 $TARGET" 1
# run_test "Size 123abc (mixed)" "./ft_ping -s 123abc -c 1 $TARGET" "ping -s 123abc -c 1 $TARGET" 1
# run_test "Size '' (empty)" "./ft_ping -s '' -c 1 $TARGET" "ping -s '' -c 1 $TARGET" 1
# run_test "Size 1.5 (float)" "./ft_ping -s 1.5 -c 1 $TARGET" "ping -s 1.5 -c 1 $TARGET" 1
# run_test "Size +100 (plus sign)" "./ft_ping -s +100 -c 1 $TARGET" "ping -s +100 -c 1 $TARGET"

# echo -e "\n${CYAN}❓ MISSING ARGUMENT SIZE TESTS${NC}"
# run_test "Flag -s without value" "./ft_ping -s -c 1 $TARGET" "ping -s -c 1 $TARGET" 1

# echo -e "\n${GRAY}🔣 WHITESPACE/SPECIAL CHARS SIZE${NC}"
# run_test "Size with spaces '  100  '" "./ft_ping -s '  100  ' -c 1 $TARGET" "ping -s '  100  ' -c 1 $TARGET"
# run_test "Size 0x64 (hex)" "./ft_ping -s 0x64 -c 1 $TARGET" "ping -s 0x64 -c 1 $TARGET" 1
# run_test "Size 0100 (octal)" "./ft_ping -s 0100 -c 1 $TARGET" "ping -s 0100 -c 1 $TARGET"

# echo -e "\n${BOLD}${YELLOW}⏱️  Testing TIMEOUT FLAG (-W)${NC}"

# echo -e "\n${GREEN}✅ VALID TIMEOUT TESTS${NC}"
# run_test "Timeout 1 (default)" "./ft_ping -W 1 -c 1 $TARGET" "ping -W 1 -c 1 $TARGET"
# run_test "Timeout 2" "./ft_ping -W 2 -c 1 $TARGET" "ping -W 2 -c 1 $TARGET"
# run_test "Timeout 5" "./ft_ping -W 5 -c 1 $TARGET" "ping -W 5 -c 1 $TARGET"
# run_test "Timeout 10" "./ft_ping -W 10 -c 1 $TARGET" "ping -W 10 -c 1 $TARGET"
# run_test "Timeout 30" "./ft_ping -W 30 -c 1 $TARGET" "ping -W 30 -c 1 $TARGET"

# echo -e "\n${RED}❌ INVALID TIMEOUT TESTS${NC}"
# run_test "Timeout -1 (negative)" "./ft_ping -W -1 -c 1 $TARGET" "ping -W -1 -c 1 $TARGET" 1
# run_test "Timeout 0 (zero)" "./ft_ping -W 0 -c 1 $TARGET" "ping -W 0 -c 1 $TARGET" 1
# run_test "Timeout -100 (negative)" "./ft_ping -W -100 -c 1 $TARGET" "ping -W -100 -c 1 $TARGET" 1

# echo -e "\n${YELLOW}🔢 BOUNDARY TIMEOUT TESTS${NC}"
# run_test "Timeout 2147483647 (INT_MAX)" "./ft_ping -W 2147483647 -c 1 $TARGET" "ping -W 2147483647 -c 1 $TARGET" 1
# run_test "Timeout 4294967295 (UINT_MAX)" "./ft_ping -W 4294967295 -c 1 $TARGET" "ping -W 4294967295 -c 1 $TARGET" 1

# echo -e "\n${MAGENTA}🔤 NON-NUMERIC TIMEOUT TESTS${NC}"
# run_test "Timeout abc (letters)" "./ft_ping -W abc -c 1 $TARGET" "ping -W abc -c 1 $TARGET" 1
# run_test "Timeout 123abc (mixed)" "./ft_ping -W 123abc -c 1 $TARGET" "ping -W 123abc -c 1 $TARGET" 1
# run_test "Timeout '' (empty)" "./ft_ping -W '' -c 1 $TARGET" "ping -W '' -c 1 $TARGET" 1
# run_test "Timeout 1.5 (float)" "./ft_ping -W 1.5 -c 1 $TARGET" "ping -W 1.5 -c 1 $TARGET" 1
# run_test "Timeout +5 (plus sign)" "./ft_ping -W +5 -c 1 $TARGET" "ping -W +5 -c 1 $TARGET"

# echo -e "\n${CYAN}❓ MISSING ARGUMENT TIMEOUT TESTS${NC}"
# run_test "Flag -W without value" "./ft_ping -W -c 1 $TARGET" "ping -W -c 1 $TARGET" 1

# echo -e "\n${GRAY}🔣 WHITESPACE/SPECIAL CHARS TIMEOUT${NC}"
# run_test "Timeout 0x5 (hex)" "./ft_ping -W 0x5 -c 1 $TARGET" "ping -W 0x5 -c 1 $TARGET" 1
# run_test "Timeout 05 (octal)" "./ft_ping -W 05 -c 1 $TARGET" "ping -W 05 -c 1 $TARGET"

echo -e "\n${BLUE}🌐 TIMEOUT BEHAVIOR TESTS${NC}"
run_test "Short timeout unreachable" "./ft_ping -W 1 -c 1 192.0.2.1" "ping -W 1 -c 1 192.0.2.1" 1
run_test "Long timeout unreachable" "./ft_ping -W 3 -c 1 192.0.2.1" "ping -W 3 -c 1 192.0.2.1" 1


show_summary
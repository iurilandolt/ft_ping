#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
WHITE='\033[0;37m'
NC='\033[0m'

TARGET="127.0.0.1"

if [ "$1" = "-t" ] && [ -n "$2" ]; then
    TARGET="$2"
fi

run_test() {
    local test_name="$1"
    local ft_command="$2"
    local ping_command="$3"
    local expected_exit_code="${4:-0}"
    
    printf "%-60s" "$test_name: "
    
    # Run our ft_ping
    local ft_output
    ft_output=$(timeout 10 $ft_command 2>&1)
    local ft_exit=$?
    
    # Run system ping
    local ping_output
    ping_output=$(timeout 10 $ping_command 2>&1)
    local ping_exit=$?
    
    # Compare exit codes
    if [ $ft_exit -eq $ping_exit ] || [ $ft_exit -eq $expected_exit_code ]; then
        echo -e "${GREEN}✓${NC}"
    else
        echo -e "${RED}✗${NC}"
        echo -e "${WHITE}ft_ping command: $ft_command${NC}"
        echo -e "${WHITE}ping command: $ping_command${NC}"
        echo -e "${WHITE}ft_ping exit: $ft_exit, ping exit: $ping_exit${NC}"
        if [ -n "$ft_output" ]; then
            echo -e "${WHITE}ft_ping output:${NC}"
            echo -e "${WHITE}$ft_output${NC}"
        fi
        if [ -n "$ping_output" ]; then
            echo -e "${WHITE}ping output:${NC}"
            echo -e "${WHITE}$ping_output${NC}"
        fi
        echo ""
    fi
}

echo "Testing ft_ping SIZE FLAG (-s) with target: $TARGET"
echo ""

# Valid size tests
echo "=== VALID SIZE TESTS ==="
run_test "Size 0 (minimum)" "./ft_ping -s 0 -c 1 $TARGET" "ping -s 0 -c 1 $TARGET"
run_test "Size 1" "./ft_ping -s 1 -c 1 $TARGET" "ping -s 1 -c 1 $TARGET"
run_test "Size 8 (default ICMP data)" "./ft_ping -s 8 -c 1 $TARGET" "ping -s 8 -c 1 $TARGET"
run_test "Size 56 (ping default)" "./ft_ping -s 56 -c 1 $TARGET" "ping -s 56 -c 1 $TARGET"
run_test "Size 64" "./ft_ping -s 64 -c 1 $TARGET" "ping -s 64 -c 1 $TARGET"
run_test "Size 100" "./ft_ping -s 100 -c 1 $TARGET" "ping -s 100 -c 1 $TARGET"
run_test "Size 1000" "./ft_ping -s 1000 -c 1 $TARGET" "ping -s 1000 -c 1 $TARGET"
run_test "Size 1472 (MTU safe)" "./ft_ping -s 1472 -c 1 $TARGET" "ping -s 1472 -c 1 $TARGET"
run_test "Size 9000 (jumbo frame)" "./ft_ping -s 9000 -c 1 $TARGET" "ping -s 9000 -c 1 $TARGET"
run_test "Size 65507 (theoretical max)" "./ft_ping -s 65507 -c 1 $TARGET" "ping -s 65507 -c 1 $TARGET"

echo ""
echo "=== INVALID SIZE TESTS ==="
run_test "Size -1 (negative)" "./ft_ping -s -1 -c 1 $TARGET" "ping -s -1 -c 1 $TARGET" 1
run_test "Size -100 (negative)" "./ft_ping -s -100 -c 1 $TARGET" "ping -s -100 -c 1 $TARGET" 1
run_test "Size -2147483648 (INT_MIN)" "./ft_ping -s -2147483648 -c 1 $TARGET" "ping -s -2147483648 -c 1 $TARGET" 1

echo ""
echo "=== BOUNDARY TESTS ==="
run_test "Size 65508 (max + 1)" "./ft_ping -s 65508 -c 1 $TARGET" "ping -s 65508 -c 1 $TARGET" 1
run_test "Size 70000 (too large)" "./ft_ping -s 70000 -c 1 $TARGET" "ping -s 70000 -c 1 $TARGET" 1
run_test "Size 2147483647 (INT_MAX)" "./ft_ping -s 2147483647 -c 1 $TARGET" "ping -s 2147483647 -c 1 $TARGET" 1
run_test "Size 4294967295 (UINT_MAX)" "./ft_ping -s 4294967295 -c 1 $TARGET" "ping -s 4294967295 -c 1 $TARGET" 1

echo ""
echo "=== NON-NUMERIC TESTS ==="
run_test "Size abc (letters)" "./ft_ping -s abc -c 1 $TARGET" "ping -s abc -c 1 $TARGET" 1
run_test "Size 123abc (mixed)" "./ft_ping -s 123abc -c 1 $TARGET" "ping -s 123abc -c 1 $TARGET" 1
run_test "Size '' (empty)" "./ft_ping -s '' -c 1 $TARGET" "ping -s '' -c 1 $TARGET" 1
run_test "Size 1.5 (float)" "./ft_ping -s 1.5 -c 1 $TARGET" "ping -s 1.5 -c 1 $TARGET" 1
run_test "Size +100 (plus sign)" "./ft_ping -s +100 -c 1 $TARGET" "ping -s +100 -c 1 $TARGET"

echo ""
echo "=== MISSING ARGUMENT TESTS ==="
run_test "Flag -s without value" "./ft_ping -s -c 1 $TARGET" "ping -s -c 1 $TARGET" 1
run_test "Flag -s at end" "./ft_ping -c 1 $TARGET -s" "ping -c 1 $TARGET -s" 1

echo ""
echo "=== WHITESPACE/SPECIAL CHARS ==="
run_test "Size with spaces '  100  '" "./ft_ping -s '  100  ' -c 1 $TARGET" "ping -s '  100  ' -c 1 $TARGET"
run_test "Size 0x64 (hex)" "./ft_ping -s 0x64 -c 1 $TARGET" "ping -s 0x64 -c 1 $TARGET" 1
run_test "Size 0100 (octal)" "./ft_ping -s 0100 -c 1 $TARGET" "ping -s 0100 -c 1 $TARGET"

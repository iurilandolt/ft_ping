#!/bin/bash

# chmod +x tests.sh

# # Basic run with default target (8.8.8.8 / google DNS server)
# ./tests.sh

# # Run with debug mode
# ./tests.sh -d

# # Run with custom target
# ./tests.sh -t google.com

# # Run with both debug and custom target
# ./tests.sh -d -t google.com

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TARGET="8.8.8.8"
DEBUG=0

usage() {
    echo "Usage: $0 [-d] [-t target_address]"
    echo "  -d: Enable debug mode"
    echo "  -t: Specify target address (default: 8.8.8.8)"
    exit 1
}

while getopts "dt:" opt; do
    case $opt in
        d) DEBUG=1 ;;
        t) TARGET="$OPTARG" ;;
        ?) usage ;;
    esac
done

debug() {
    if [ $DEBUG -eq 1 ]; then
        echo -e "${YELLOW}[DEBUG] $1${NC}"
    fi
}

run_test() {
    local test_name="$1"
    local command="$2"
    
    echo -e "\n${YELLOW}Running test: ${test_name}${NC}"
    debug "Command: $command"
    
    if eval "$command"; then
        echo -e "${GREEN}✓ Test passed: ${test_name}${NC}"
        return 0
    else
        echo -e "${RED}✗ Test failed: ${test_name}${NC}"
        return 1
	fi
}

echo -e "${YELLOW}Starting ft_ping tests...${NC}"
debug "Target address: $TARGET"

run_test "Usage Test" "./ft_ping"
run_test "Basic Connectivity" "./ft_ping 127.0.0.1"
run_test "Error Handling" "./ft_ping invalid.host"
run_test "Verbose Mode" "./ft_ping -v localhost"
run_test "Count Option" "./ft_ping -c 3 $TARGET"

echo -e "\n${YELLOW}Comparing with real ping...${NC}"
debug "Generating output files..."

./ft_ping "$TARGET" > our_output.txt &
ping "$TARGET" -c 4 > real_output.txt &

wait

echo -e "\n${YELLOW}Diff results:${NC}"
if diff our_output.txt real_output.txt; then
    echo -e "${GREEN}✓ Outputs match${NC}"
else
    echo -e "${RED}✗ Outputs differ${NC}"
fi

debug "Cleaning up temporary files..."
rm -f our_output.txt real_output.txt

echo -e "\n${GREEN}Tests completed!${NC}"
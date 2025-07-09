# ft_ping - A Network Diagnostic Tool Implementation

## Overview
`ft_ping` is a custom implementation of the classic `ping` utility, designed to test network connectivity and measure round-trip times between hosts. This implementation supports both IPv4 and IPv6 protocols and demonstrates low-level network programming concepts including raw sockets, ICMP protocol handling, and packet timing.

## Architecture & OSI Model Layers

### Layer 3 - Network Layer (IP)
**What we handle:**
- **DNS Resolution**: Converting hostnames (FQDNs) to IP addresses using `getaddrinfo()`
- **IP Protocol Selection**: Automatic detection and handling of IPv4 vs IPv6
- **Address Management**: Storing and managing both IPv4 and IPv6 addresses

**What the kernel handles:**
- **IP Header Creation**: Kernel automatically adds IP headers (20 bytes for IPv4, 40 bytes for IPv6)
- **Routing**: Kernel determines the best path to destination
- **Fragmentation**: Kernel handles packet fragmentation if needed

### Layer 3 - Network Layer (ICMP)
**What we handle:**
- **ICMP Header Creation**: Manual construction of ICMP echo request/reply headers
- **Packet Identification**: Setting process ID and sequence numbers
- **Checksum Calculation**: RFC 792 compliant checksum computation
- **Packet Tracking**: Maintaining sent packet list for reply matching

### Layer 2 & 1 - Data Link & Physical
**Completely handled by kernel and hardware:**
- Ethernet framing, MAC addresses, physical transmission

## Core Components

### 1. Argument Parsing (`io.c`)
```c
// Command line options with getopt()
getopt(argc, argv, "vc:s:l:W:")
```
- `v`: Verbose output (flag, no argument)
- `c:`: Count limit (requires argument)
- `s:`: Packet size (requires argument)
- `l:`: Preload packets (requires argument)
- `W:`: Timeout in seconds (requires argument)

### 2. Network Resolution (`network.c`)
```c
// DNS resolution using getaddrinfo()
int resolveHost(t_ping_state *state, char **argv)
```
- Converts FQDN to IP address (one-time DNS lookup)
- Supports both IPv4 and IPv6 addresses
- **Important**: No reverse DNS lookup on packet replies (per requirements)

### 3. Socket Creation (`network.c`)
```c
// Raw socket creation for both protocols
socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);      // IPv4
socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);   // IPv6
```
- Raw sockets allow direct ICMP packet construction
- Non-blocking sockets for asynchronous I/O

### 4. Packet Management (`packet.c`)

#### Packet Structure
```c
typedef struct s_ping_pkg {
    struct icmphdr header;  // 8 bytes ICMP header
    char msg[];            // Variable size data payload
} t_ping_pkg;
```

#### ICMP Header Fields
- **Type**: `ICMP_ECHO` (IPv4) or `ICMP6_ECHO_REQUEST` (IPv6)
- **Code**: Always 0 for echo requests
- **Checksum**: Calculated using RFC 792 algorithm
- **Identifier**: Process ID for packet ownership
- **Sequence**: Incremental number for packet ordering

#### Packet Data Payload
- **Timestamp**: `struct timeval` for RTT calculation
- **Pattern Data**: Repeating pattern `0x10 + (i % 48)` for remaining space

### 5. Packet Size Calculation

#### IPv4 Packets
```
User Data (56) + ICMP Header (8) + IP Header (20) = 84 bytes total
Display: "PING host (IP) 56(84) bytes of data"
```

#### IPv6 Packets
```
User Data (56) + ICMP Header (8) + IP Header (40) = 104 bytes total
Display: "PING host (IP) 56 data bytes" (IP header not shown in display)
```

### 6. Polling System (`poll.c`)
```c
// Event-driven I/O with poll()
int poll_result = poll(fds, 2, poll_timeout);
```
- Monitors both IPv4 and IPv6 sockets simultaneously
- Dynamic timeout calculation based on send intervals
- Handles packet reception without blocking

### 7. Packet Transmission Flow

#### Sending Process
1. **Timing Check**: Ensure 1-second interval between packets
2. **Packet Creation**: Allocate and initialize ICMP packet
3. **Data Population**: Fill with timestamp and pattern
4. **Checksum**: Calculate and set ICMP checksum
5. **Transmission**: Send via `sendto()` through raw socket
6. **Tracking**: Add to sent packet list for reply matching

#### Receiving Process
1. **Socket Monitoring**: `poll()` detects incoming data
2. **Packet Reception**: `recvfrom()` receives raw packet
3. **Header Parsing**: Extract IP header (IPv4 only) and ICMP header
4. **Validation**: Verify packet type, ID, and sequence
5. **RTT Calculation**: Compare timestamps for round-trip time
6. **Statistics Update**: Update min/max/avg RTT values
7. **Cleanup**: Remove packet from sent list

### 8. Signal Handling (`signals.c`)
```c
// Graceful shutdown on SIGINT (Ctrl+C)
void handleSignals(int signum, siginfo_t *info, void *ptr)
```
- Captures termination signals
- Ensures proper cleanup and statistics display

## Key Implementation Details

### Packet Verification by Kernel
- **Checksum Validation**: Kernel verifies ICMP checksums
- **Protocol Filtering**: Only delivers packets matching our protocol
- **Source Validation**: Ensures packets come from expected source

### FQDN Handling
- **Forward Resolution**: Done once at startup (`getaddrinfo()`)
- **No Reverse Lookup**: Packet replies show IP addresses, not hostnames
- **Compliance**: Follows requirement to avoid DNS resolution in packet returns

### Memory Management
- **Packet Tracking**: Linked list of sent packets for timeout handling
- **Dynamic Allocation**: Packets allocated per transmission
- **Cleanup**: Proper deallocation on timeouts and replies

### Error Handling
- **Socket Errors**: Graceful handling of network failures
- **Timeout Management**: Remove expired packets from tracking
- **Signal Safety**: Clean shutdown on interruption

## Usage Examples

```bash
# Basic ping
./ft_ping google.com

# Verbose output with packet details
./ft_ping -v google.com

# Limited packet count
./ft_ping -c 5 google.com

# Custom packet size
./ft_ping -s 1000 google.com

# Preload packets and custom timeout
./ft_ping -l 3 -W 2 google.com
```

## Statistics Output
```
--- google.com ping statistics ---
5 packets transmitted, 5 received, 0% packet loss, time 4001ms
rtt min/avg/max = 14.123/15.456/17.890 ms
```

## Technical Requirements
- **Root Privileges**: Required for raw socket creation
- **IPv4/IPv6 Support**: Automatic protocol detection
- **RFC Compliance**: Follows ICMP standards (RFC 792/4443)
- **POSIX Compliance**: Uses standard system calls and libraries

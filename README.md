# ft_ping Documentation

## Argument Parsing

- **`-v`**: Verbose output - Shows detailed socket information and packet identifiers in output
- **`-c <count>`**: Stop after sending count packets - Limits total packets sent, program exits after reaching this number
- **`-s <size>`**: Set packet payload size - Controls payload size (excludes ICMP/IP headers), affects total packet size
- **`-l <preload>`**: Send preload packets immediately - Sends multiple packets rapidly at start, then continues with normal 1-second intervals
- **`-W <timeout>`**: Set timeout per packet in seconds - How long to wait for each packet response before considering it lost
- **`-t <ttl>`**: Set Time-To-Live for packets - Maximum number of network hops before packet is discarded
- **`-h`**: Show help/usage - Displays usage information and exits
- **`<destination>`**: Target hostname or IP (required) - Target to ping, can be hostname (google.com) or IP address (8.8.8.8)

### Tools
- **`getopt()`**: Standard POSIX function that processes command-line arguments systematically. It takes the argument count, argument vector, and an option string (`"vhc:s:l:W:t:"`) where letters represent valid options and colons indicate options that require arguments. `getopt()` returns each option character one by one, sets `optarg` to point to the option's argument (if any), and handles error cases like unknown options or missing required arguments. It automatically manages the `optind` global variable to track position in the argument list.

- **`parse_int_range()`**: Custom validation function that safely converts string arguments to integers using `strtol()` and enforces min/max boundaries. It performs comprehensive error checking: ensures the entire string is a valid number, detects overflow/underflow conditions, and validates the result falls within acceptable ranges.

## Host Resolution

The `resolveHost()` Convert user-provided hostname or IP address into network operations format suitable for. 

### Key Functions
- **`getaddrinfo()`**: DNS Resolution. Takes a hostname/IP string and service name, returning a linked list of address structures. 

- **`inet_ntop()`**: Converts binary network addresses back to human-readable string format. Used to create the display string that shows the resolved IP address in program output.

## Socket Creation

The `createSocket()` establishes raw network sockets for ICMP communication. Creates both IPv4 and IPv6 sockets, sets non-blocking mode, and configures TTL.

### Key Functions
- **`socket()`**: Creates raw sockets for ICMP (IPv4) and ICMPv6 (IPv6) protocols.
- **`fcntl()`**: Sets sockets to non-blocking mode for asynchronous operation.
- **`setsockopt()`**: Configures TTL (IP_TTL/IPV6_UNICAST_HOPS) for packet hop limits.

## Packet System

```
[ IP Header (20 bytes) ][ ICMP Header (8 bytes) ][ Payload Data (variable) ]
    	Kernel           	ft_ping           			ft_ping
```

```c
typedef struct s_ping_pkg {
    struct icmphdr  header;  // ICMP header (8 bytes)
    char           msg[];    // Variable payload data
} t_ping_pkg;
```

- **Kernel**: Automatically adds IP header (source/dest IP, protocol, TTL, checksum)
- **Our Code**: Constructs ICMP header + payload, calculates ICMP checksum

### Packet Construction (`create_packet`)
1. **ICMP Header Setup**:
   - `type`: ICMP_ECHO (IPv4) or ICMP6_ECHO_REQUEST (IPv6)
   - `id`: Process ID for packet identification
   - `sequence`: Incremental sequence number
   - `checksum`: RFC 792 Internet checksum

2. **Payload Data**:
   - **Timestamp**: `struct timeval` at start for RTT calculation
   - **Pattern Data**: Repeating pattern (0x10 + offset) for packet validation

### Key Functions
- **`gettimeofday()`**: Embeds send timestamp in packet payload for RTT measurement
- **`calculate_checksum()`**: Computes RFC 792 checksum for packet integrity
- **`htons()`**: Converts host byte order to network byte order for headers

## Event Loop & Polling System

```c
struct pollfd fds[2];  
```
- **fds[0]**: IPv4 ICMP socket (`state->conn.ipv4.sockfd`)
- **fds[1]**: IPv6 ICMP socket (`state->conn.ipv6.sockfd`)
- **events**: `POLLIN` - Monitor for incoming data
- **revents**: Reset to 0, filled by poll() with actual events

### Timeout Calculation 

1. **Active Transmission**: Returns milliseconds until next 1-second send interval


2. **Transmission Complete**: Returns 100ms timeout for cleanup phase


3. **Preload Phase**: Returns 0 for immediate sending


### Main Event Loop
The program runs until all packets are sent AND all responses received (or timed out):

1. **Send Phase**: Attempt to send a ping packet if timing allows
2. **Calculate Timeout**: Determine how long to wait for network activity
3. **Poll for Events**: Block until data arrives on either socket or timeout expires
4. **Handle Results**:
   - **Data Available**: Process incoming ICMP response
   - **Timeout**: Clean up packets that exceeded response timeout
   - **Error**: Break loop on unrecoverable errors





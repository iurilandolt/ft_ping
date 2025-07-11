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
   - **Timestamp**: `struct timeval` (16 bytes) at start for RTT calculation
   - **Pattern Data**: Repeating pattern `0x10 + (offset % 48)` for packet validation

### Payload Construction Details
- **Default Size**: 56 bytes (ICMP header + payload)
- **Size Range**: 0-65507 bytes
- **Timestamp Handling**: Only included if payload ≥ 16 bytes (`sizeof(struct timeval)`)
- **Pattern Generation**: 
  - **0x10 (16)**: Base value ensuring printable ASCII range
  - **% 48**: Cycles through 48 characters (0x10-0x3F, covering digits, letters, symbols)

### Size Scenarios (`-s` flag)
- **Minimum (0 bytes payload)**: 28 bytes total (20 IP + 8 ICMP), no timestamp or pattern
- **Small (1-15 bytes payload)**: 29-43 bytes total, pattern data only, no RTT measurement possible
- **Normal (16+ bytes payload)**: 44+ bytes total, timestamp + pattern data, full functionality
- **Maximum (65507 bytes payload)**: 65535 bytes total, maximum IP packet size

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

1. **Send Phase**: Attempt to send a ping packet if timing/preload allows
2. **Calculate Timeout**: Determine how long to wait for network activity
3. **Poll for Events**: Block until data arrives on either socket or timeout expires
4. **Handle Results**:
   - **Data Available**: Process incoming ICMP response
   - **Timeout**: Clean up packets that exceeded response timeout
   - **Error**: Break loop on unrecoverable errors

## Sending Ping Packets

During each loop iteration, if timing and packet count allow, the program attempts to send a new ICMP Echo Request packet to the target.

- **Packet Creation:** A new packet is initialized with the correct ICMP header, sequence number, process ID, and payload (timestamp + pattern).
- **Socket Selection:** The appropriate socket (IPv4 or IPv6) is chosen based on the resolved address family.
- **Sending:** The packet is sent using `sendto()`. The current timestamp is recorded for RTT calculation.
- **Tracking:** The sent packet is added to a tracking list for later matching with replies or timeouts.

**Key Points:**
- Only sends if allowed by timing (interval/preload) and packet count.
- Updates statistics for packets sent.
- Handles errors gracefully (e.g., network unreachable, permission denied).

## Receiving Ping Replies

When data is available on a socket, the program processes incoming ICMP responses:

- **Packet Reception:** Reads the incoming packet using `recvfrom()` into a buffer.
- **Validation:** Checks that the response matches a sent packet (by process ID and sequence number).
- **Type Handling:** Distinguishes between echo replies (success) and ICMP errors (like time exceeded or unreachable).
- **RTT Calculation:** If a valid reply, extracts the timestamp from the payload and computes the round-trip time.
- **Statistics Update:** Updates counters for received packets, errors, and RTT statistics.
- **Cleanup:** Removes the matching packet from the tracking list.


> We use `sendto()` and `recvfrom()` instead of `send()` and `recv()` because ICMP operates over raw sockets without a connection-oriented protocol. 
These functions allow specifying the destination and source address explicitly, which is necessary since there is no established socket connection as with TCP.

### ICMP Reply vs. Error Buffer Structure

When receiving ICMP packets, the buffer layout and size differ significantly depending on whether the packet is a success reply or an error message.

#### Success Replies

[ IP Header ][ ICMP Echo Reply Header ][ Payload (timestamp + pattern) ]

- **Size:**  
  - Minimum: 28 bytes (20 IP + 8 ICMP, no payload)
  - Typical: 64 bytes (20 IP + 8 ICMP + 36 payload)

  The reply is a direct response to our echo request. The kernel fills in the IP header, and our code matches the reply by process ID and sequence number.

#### Error Messages

[ IP Header ][ ICMP Error Header ][ Embedded Original IP Header ][ Embedded Original ICMP Header ][ (Partial Original Payload) ]

- **Size:**  
  - Minimum: 48 bytes (20 IP + 8 ICMP error + 20 embedded og IP + 8 embedded og ICMP)
  - Can be larger if more of the original payload is included.
  
  ICMP error messages (like "Time Exceeded" or "Destination Unreachable") must include the header and at least 8 bytes of the original packet that caused the error (per RFC 792). This allows us to identify which of our packets triggered the error by extracting the embedded headers and matching the process ID and sequence number.

### Packet Validation

**For Success Replies:**
- Check that the ICMP type is `ICMP_ECHOREPLY` (IPv4) or `ICMP6_ECHO_REPLY` (IPv6)
- Check that the `id` field matches our process ID
- Check that the sequence number exists in our sent packets list

**For Error Messages:**
- Check that the ICMP type is `ICMP_TIME_EXCEEDED` or `ICMP_DEST_UNREACH`
- Extract the embedded original packet from the error message payload
- Check that the embedded ICMP type is `ICMP_ECHO` (our original request)
- Check that the embedded `id` matches our process ID
- Check that the embedded sequence number exists in our sent packets list

## RTT Statistics and mdev

**RTT (Round-Trip Time)** is the time it takes for a packet to travel from your machine to the target and back. It is measured in milliseconds (ms) and is a key indicator of network latency.

- **How is RTT measured?**
  - When sending a packet, the program embeds the current timestamp in the payload.
  - When a reply is received, the program extracts the timestamp and subtracts it from the current time to get the RTT for that packet.

### What statistics do we track?
- **min RTT:** The lowest RTT observed during the session.
- **max RTT:** The highest RTT observed.
- **avg RTT:** The average RTT, calculated as the sum of all RTTs divided by the number of replies received.
- **mdev:** The mean absolute deviation of RTTs from the average RTT, representing the typical "jitter" or variability in round-trip times.

### How is mdev calculated?
1. Compute the average RTT (`avg_rtt`).
2. For each RTT, calculate the absolute difference from the average.
3. mdev is the mean (average) of these absolute differences.

**Formula:**  
- mdev = (|rtt1 - avg| + |rtt2 - avg| + ... + |rttN - avg|) / N

### Example Output
- rtt min/avg/max/mdev = 0.035/0.046/0.060/0.007 ms
- **min:** 0.035 ms
- **avg:** 0.046 ms
- **max:** 0.060 ms
- **mdev:** 0.007 ms

## Output Modes: Standard vs. Verbose (`-v`)

### Standard Output Example

```
PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data. 
64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.042 ms 
64 bytes from 127.0.0.1: icmp_seq=2 ttl=64 time=0.064 ms 
64 bytes from 127.0.0.1: icmp_seq=3 ttl=64 time=0.063 ms 
64 bytes from 127.0.0.1: icmp_seq=4 ttl=64 time=0.060 ms

--- 127.0.0.1 ping statistics --- 4 packets transmitted, 4 received, 0% packet loss, time 3003ms rtt min/avg/max/mdev = 0.042/0.057/0.064/0.008 ms
```
### Verbose Output Example (`-v`)
With the `-v` flag, `ft_ping` prints additional diagnostic information:
- Socket file descriptors and types
- Address family and canonical name
- The ICMP identifier (`ident`) for each reply (process id)
```
ping: sock4.fd: 3 (socktype: SOCK_RAW), sock6.fd: 4 (socktype: SOCK_RAW), hints.ai_family: AF_UNSPEC ai->ai_family: AF_INET, ai->ai_canonname: '127.0.0.1' PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data. 
64 bytes from 127.0.0.1: icmp_seq=1 ident=36192 ttl=64 time=0.042 ms 
64 bytes from 127.0.0.1: icmp_seq=2 ident=36192 ttl=64 time=0.064 ms 
64 bytes from 127.0.0.1: icmp_seq=3 ident=36192 ttl=64 time=0.063 ms 
64 bytes from 127.0.0.1: icmp_seq=4 ident=36192 ttl=64 time=0.060 ms

--- 127.0.0.1 ping statistics --- 4 packets transmitted, 4 received, 0% packet loss, time 3003ms rtt min/avg/max/mdev = 0.042/0.057/0.064/0.008 ms
```

### `-l <preload>`: Preload Packets

The `-l` flag controls how many packets are sent immediately at the start of the ping session, before switching to the normal 1-second interval between packets.

- **Usage:** `-l <preload>`
- **Range:** 1–3 (default: 1)
- **Behavior:**  
  - If `-l 3` is specified, the first 3 packets are sent as quickly as possible, then the program continues sending at 1-second intervals.
  - Useful for quickly testing network burst handling or for simulating a short flood of packets.
- **Example:**  
  ```
  ./ft_ping -l 3 -c 5 8.8.8.8
  ```
  This sends 3 packets instantly, then 2 more at 1-second intervals.

### `-W <timeout>`: Per-Packet Timeout

The `-W` flag sets the maximum time (in seconds) to wait for each individual ping reply before considering it lost.

- **Usage:** `-W <timeout>`
- **Range:** 1–2099999 (default: 4)
- **Behavior:**  
  - After sending each packet, `ft_ping` waits up to `<timeout>` seconds for a reply.
  - If no reply is received within this time, the packet is counted as lost and the program continues.
  - This does not affect the interval between packet transmissions, only how long to wait for each reply.
- **Example:**  
  ```
  ./ft_ping -W 2 -c 3 8.8.8.8
  ```
  Each packet will be considered lost if no reply is received within 2 seconds.


## Time-To-Live (TTL) and Hop Limit (`-t`)

**TTL (Time-To-Live)** is a field in the IP header that limits the number of hops (routers) a packet can traverse before being discarded. Each router that forwards the packet decrements the TTL by 1. When TTL reaches zero, the packet is dropped and an ICMP "Time Exceeded" error is sent back to the sender.

- **Purpose:** Prevents packets from circulating endlessly in routing loops.

- **IPv4:** TTL is set using the `IP_TTL` socket option.
- **IPv6:** The equivalent is called **Hop Limit**, set using the `IPV6_UNICAST_HOPS` socket option.


- If a packet's TTL/hop limit reaches zero before reaching the destination, it is dropped by the router at that hop.
- The router sends back an ICMP "Time Exceeded" message, which ft_ping reports as a timeout or error.
- By adjusting TTL, you can trace the path to a host (like `traceroute` does), or test how far packets get before being dropped.

### Example
- `./ft_ping -t 1 8.8.8.8`  
  The packet will only survive one hop. If the destination is farther, you'll receive a "Time Exceeded" error from the first router.

**Summary:**  
- **TTL (IPv4)** and **Hop Limit (IPv6)** control how far packets can travel.
- Low TTL values help diagnose routing and network reachability issues.

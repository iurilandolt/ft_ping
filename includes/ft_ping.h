#ifndef FT_PING_H
#define FT_PING_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#define PING_PKT_S 56			// ping packet size

typedef struct s_ping_pkg {
    struct icmphdr header;
    char msg[];
} t_ping_pkg;

typedef struct s_packet_entry {
    uint16_t sequence;
    t_ping_pkg *packet;
    struct s_packet_entry *next;
} t_packet_entry;

typedef struct s_ping_state {
	t_packet_entry *sent_packets;  // Linked list of sent packets
	struct {
		int sockfd;
		char *target;
		void *addr;                    // pointer to address structure
		int family;                    // AF_INET or AF_INET6
		int protocol;
		uint16_t pid;                  // IPPROTO_ICMP or IPPROTO_ICMPV6
		socklen_t addr_len;            // sizeof(sockaddr_in) or sizeof(sockaddr_in6)
		struct sockaddr_in ipv4;       // IPv4 address
		struct sockaddr_in6 ipv6;
		char addr_str[INET6_ADDRSTRLEN];
	} conn;
	struct {
        long packets_sent;
        long packets_received;
        double min_rtt; // round-trip time
        double max_rtt; 
		double avg_rtt;
        double sum_rtt;
        struct timeval start_time;
	} stats;
	struct {
        int verbose;     // -v flag
        int count;       // -c flag
        size_t psize;    // -s flag
        int preload;     // -l flag
        int timeout;     // -W flag (in seconds)
	} opts;
} t_ping_state;

// signals
void handleSignals(int signum, siginfo_t *info, void *ptr);
void setupSignals(t_ping_state *state);
// network
void *get_addr_ptr(t_ping_state *state);
int resolveHost(t_ping_state *state, char **argv);
int createSocket(t_ping_state *state, char **argv);
int send_ping(t_ping_state *state, uint16_t sequence);
int receive_ping(t_ping_state *state, uint16_t sequence);
// packet
int allocate_packet(t_ping_state *state);
void free_packet(t_ping_state *state);
void create_icmp_packet(t_ping_state *state, uint16_t sequence);
uint16_t calculate_checksum(t_ping_state *state, uint16_t sequence);
void fill_packet_data(t_ping_state *state, uint16_t sequence);
int parse_icmp_reply(char *buffer, ssize_t bytes_received, uint16_t expected_sequence, t_ping_state *state);
t_packet_entry* find_sent_packet(t_ping_state *state, uint16_t sequence);
void remove_sent_packet(t_ping_state *state, uint16_t sequence);
// io
int parseArgs(t_ping_state *state, int argc, char **argv);
void print_verbose_info(t_ping_state *state);
void print_default_info(t_ping_state *state);
void print_stats(t_ping_state *state);
void print_ping_reply(t_ping_state *state, size_t icmp_size, struct icmphdr *icmp_header, int ttl, double rtt);

#endif




#ifndef FT_PING_H
#define FT_PING_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/in.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <sys/time.h>

#define PING_PKT_S 64			// ping packet size

typedef struct s_ping_pkg {
	 struct icmphdr header;
	 char msg[PING_PKT_S - sizeof(struct icmphdr)]; // packet size(64) - size of icmps struct(8) = 56 bytes for message
} t_ping_pkg;

typedef struct s_ping_state {
	t_ping_pkg packet;		// ping package structure
	struct {
		int sockfd;
		char *target;
		void *addr;                    // pointer to address structure
		int family;                    // AF_INET or AF_INET6
		int protocol;
		int pid;                  // IPPROTO_ICMP or IPPROTO_ICMPV6
		socklen_t addr_len;            // sizeof(sockaddr_in) or sizeof(sockaddr_in6)
		struct sockaddr_in ipv4;       // IPv4 address
		struct sockaddr_in6 ipv6;
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

void handleSignals(int signum, siginfo_t *info, void *ptr);
void setupSignals(void);

void *get_addr_ptr(t_ping_state *state);
int resolveHost(t_ping_state *state, char **argv);
int createSocket(t_ping_state *state, char **argv);
int send_ping(t_ping_state *state);
int receive_ping(t_ping_state *state, uint16_t sequence);

void create_icmp_packet(t_ping_state *state, uint16_t sequence);
uint16_t calculate_checksum(t_ping_state *state);
void fill_packet_data(t_ping_state *state);
int parse_icmp_reply(char *buffer, ssize_t bytes_received, uint16_t expected_sequence, t_ping_state *state);

#endif




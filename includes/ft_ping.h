#ifndef FT_PING_H
#define FT_PING_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/ip_icmp.h>
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
		int protocol;                  // IPPROTO_ICMP or IPPROTO_ICMPV6
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

#endif




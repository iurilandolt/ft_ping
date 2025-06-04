#ifndef FT_PING_H
#define FT_PING_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PING_PKT_S 64			// ping packet size
#define PORT_NO 0				// automatic port number
#define PING_SLEEP_RATE 1000000	// ping sleep rate (in microseconds)
#define RECV_TIMEOUT 1			// timeout for receiving packets (in seconds)

typedef struct s_ping_pkg {
	 struct icmphdr header;
	 char msg[PING_PKT_S - sizeof(struct icmphdr)];
} t_ping_pkg;

void handleSignals(int signum, siginfo_t *info, void *ptr);
void setupSignals(void);

#endif
#include "../includes/ft_ping.h"


/**
 * @param state - ping state containing statistics and target info
 * 
 * Prints final ping statistics including packet loss and RTT measurements
 */
void print_stats(t_ping_state *state) {
	double total_time = 0.0;
	if (state->stats.packets_sent > 0) {
		if (state->stats.packets_sent == 1) {
			total_time = 0.0;
		} else {
			total_time = (state->stats.last_packet_time.tv_sec - state->stats.first_packet_time.tv_sec) * 1000.0 + 
						 (state->stats.last_packet_time.tv_usec - state->stats.first_packet_time.tv_usec) / 1000.0;
		}
	}

	fprintf(stdout, "\n--- %s ping statistics ---\n", state->conn.target);
	fprintf(stdout, "%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms\n",
		   state->stats.packets_sent, state->stats.packets_received,
		   ((double)(state->stats.packets_sent - state->stats.packets_received) / 
			state->stats.packets_sent) * 100.0, total_time);
	
	if (state->stats.packets_received > 0) {
		state->stats.avg_rtt = state->stats.sum_rtt / state->stats.packets_received;
		if (state->stats.avg_rtt > 0) {
			double mdev = calculate_mean_deviation(state);
			fprintf(stdout, "rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", 
				state->stats.min_rtt, state->stats.avg_rtt, state->stats.max_rtt, mdev); 
		}
	}
	if (state->stats.errors > 0) {
		fprintf(stdout, "+%d errors.\n", state->stats.errors);
	}
}

/**
 * @param state - ping state containing verbose flag and socket info
 * 
 * Prints verbose socket and connection information when verbose flag is set
 */
void print_verbose_info(t_ping_state *state) {
	if (!state->opts.verbose) {
		return;
	}
	
	int socktype;
	socklen_t optlen = sizeof(socktype);
	getsockopt(state->conn.ipv4.sockfd, SOL_SOCKET, SO_TYPE, &socktype, &optlen);
	
	const char *socktype_str = (socktype == SOCK_RAW) ? "SOCK_RAW" : 
							  (socktype == SOCK_DGRAM) ? "SOCK_DGRAM" : "UNKNOWN";
	
	if (state->conn.target_family == AF_INET) {
		fprintf(stdout, "ping: sock4.fd: %d (socktype: %s), sock6.fd: %d (socktype: SOCK_RAW), hints.ai_family: AF_UNSPEC\n",
			   state->conn.ipv4.sockfd, socktype_str, state->conn.ipv6.sockfd);
	} else {
		fprintf(stdout, "ping: sock4.fd: %d (socktype: SOCK_RAW), sock6.fd: %d (socktype: %s), hints.ai_family: AF_UNSPEC\n",
			   state->conn.ipv4.sockfd, state->conn.ipv6.sockfd, socktype_str);
	}
	
	const char *family_str = (state->conn.target_family == AF_INET) ? "AF_INET" : "AF_INET6";
	fprintf(stdout, "\nai->ai_family: %s, ai->ai_canonname: '%s'\n",
		   family_str, state->conn.target);
}

/**
 * @param state - ping state containing target and packet size info
 * 
 * Prints initial ping header with target address and packet size information
 */
void print_default_info(t_ping_state *state) {
	size_t icmp_header_size = (state->conn.target_family == AF_INET) ? 
							sizeof(struct icmphdr) : 
							sizeof(struct icmp6_hdr);
	size_t data_size = state->opts.psize - icmp_header_size;
	
	char *addr_str = (state->conn.target_family == AF_INET) ?
					 state->conn.ipv4.addr_str : 
					 state->conn.ipv6.addr_str;
	
	if (state->conn.target_family == AF_INET) {
		size_t total_with_ip = data_size + icmp_header_size + 20;
		fprintf(stdout, "PING %s (%s) %zu(%zu) bytes of data.\n", 
			state->conn.target, addr_str, 
			data_size, total_with_ip);
	} else {
		fprintf(stdout, "PING %s (%s) %zu data bytes\n", 
			state->conn.target, addr_str, data_size);
	}
}

/**
 * @param state - ping state containing verbose flag and address info
 * @param icmp_size - size of received ICMP packet
 * @param icmp_header - ICMP header containing sequence and ID
 * @param ttl - time-to-live value
 * @param rtt - round-trip time in milliseconds
 * 
 * Prints formatted ping reply message with packet details
 */
void print_ping_reply(t_ping_state *state, size_t icmp_size, 
					 struct icmphdr *icmp_header, int ttl, double rtt) {
	uint16_t sequence = ntohs(icmp_header->un.echo.sequence);
	uint16_t id = ntohs(icmp_header->un.echo.id);
	
	char *addr_str = (state->conn.target_family == AF_INET) ?
					 state->conn.ipv4.addr_str : 
					 state->conn.ipv6.addr_str;
	
	if (rtt >= 0.0) {
		if (state->opts.verbose) {
			fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ident=%d ttl=%d time=%.3f ms\n",
					icmp_size, addr_str, sequence, id, ttl, rtt);
		} else {
			fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
					icmp_size, addr_str, sequence, ttl, rtt);
		}
	} else {
		if (state->opts.verbose) {
			fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ident=%d ttl=%d\n",
					icmp_size, addr_str, sequence, id, ttl);
		} else {
			fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ttl=%d\n",
					icmp_size, addr_str, sequence, ttl);
		}
	}
}

/**
 * @param ctx - ICMP context containing error packet information
 * @param error_message - the error message to display
 * 
 * Prints formatted ICMP error message with sender address information
 */
void print_icmp_error(t_icmp_context *ctx, const char *error_message) {
	char hostname[NI_MAXHOST];
	char sender_ip[INET_ADDRSTRLEN];
	struct sockaddr_in *from_addr = (struct sockaddr_in*)ctx->from;
	
	inet_ntop(AF_INET, &from_addr->sin_addr, sender_ip, INET_ADDRSTRLEN);
	
	if (getnameinfo((struct sockaddr*)from_addr, sizeof(*from_addr), 
					hostname, sizeof(hostname), NULL, 0, 0) == 0) {
		fprintf(stdout, "From %s (%s): icmp_seq=%d %s\n", 
			   hostname, sender_ip, ctx->sequence, error_message);
	} else {
		fprintf(stdout, "From %s: icmp_seq=%d %s\n", 
			   sender_ip, ctx->sequence, error_message);
	}
}

void print_usage(char *arg, char opt) {
	(void)opt;
	//fprintf(stderr, "%s: usage error: Unknown option '-%c'\n", arg, opt);
	fprintf(stdout, "Usage:\n %s [options] <destination>\n", arg);
	fprintf(stdout, "Options:\n");
	fprintf(stdout, "  -v		Verbose output\n");
	fprintf(stdout, "  -c <count> 	Stop after sending <count> packets\n");
	fprintf(stdout, "  -s <size>	Set packet size to <size> bytes\n");
	fprintf(stdout, "  -l <preload>	Preload <preload> packets before starting\n");
	fprintf(stdout, "  -W <timeout>	Set timeout for each packet in seconds\n");
	fprintf(stdout, "  -t <ttl>	Set time-to-live for packets\n");
}
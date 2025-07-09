#include "../includes/ft_ping.h"

/**
 * @param state - ping state to populate with resolved address info
 * @param argv - command line arguments for error reporting
 * @return 0 on success, 1 on failure
 * 
 * Resolves target hostname to IP address and populates connection info
 */
int resolveHost(t_ping_state *state, char **argv) {
	struct addrinfo hints, *result;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	
	if (getaddrinfo(state->conn.target, NULL, &hints, &result) != 0) {
		fprintf(stderr, "%s: %s: Name or service not known\n", 
				argv[0], state->conn.target);
		return 1;
	}
	
	state->conn.ipv4.family = AF_INET;
	state->conn.ipv4.protocol = IPPROTO_ICMP;
	state->conn.ipv4.addr_len = sizeof(struct sockaddr_in);
	
	state->conn.ipv6.family = AF_INET6;
	state->conn.ipv6.protocol = IPPROTO_ICMPV6;
	state->conn.ipv6.addr_len = sizeof(struct sockaddr_in6);
	
	if (result->ai_family == AF_INET) {
		memcpy(&state->conn.ipv4.addr, result->ai_addr, sizeof(struct sockaddr_in));
		inet_ntop(AF_INET, &((struct sockaddr_in*)&state->conn.ipv4.addr)->sin_addr, 
				state->conn.ipv4.addr_str, INET_ADDRSTRLEN);
	} else if (result->ai_family == AF_INET6) {
		memcpy(&state->conn.ipv6.addr, result->ai_addr, sizeof(struct sockaddr_in6));
		inet_ntop(AF_INET6, &((struct sockaddr_in6*)&state->conn.ipv6.addr)->sin6_addr, 
				state->conn.ipv6.addr_str, INET6_ADDRSTRLEN);
	}
	state->conn.target_family = result->ai_family;
	freeaddrinfo(result);
	return 0;
}

/**
 * @param state - ping state to store socket file descriptors
 * @param argv - command line arguments for error reporting
 * @return 0 on success, 1 on failure
 * 
 * Creates IPv4 and IPv6 raw sockets and sets them to non-blocking mode
 */
int createSocket(t_ping_state *state, char **argv) {
	int flags;
	state->conn.ipv4.sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	state->conn.ipv6.sockfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	
	if (state->conn.ipv4.sockfd < 0 || state->conn.ipv6.sockfd < 0) {
		fprintf(stderr, "%s: Cannot create socket\n", argv[0]);
		return 1;
	}
	
	flags = fcntl(state->conn.ipv4.sockfd, F_GETFL, 0);
	fcntl(state->conn.ipv4.sockfd, F_SETFL, flags | O_NONBLOCK);
	flags = fcntl(state->conn.ipv6.sockfd, F_GETFL, 0);
	fcntl(state->conn.ipv6.sockfd, F_SETFL, flags | O_NONBLOCK);
	
	return 0;
}

/**
 * @param state - ping state containing packet tracking and statistics
 * @param sockfd - socket file descriptor to receive from
 * @return 0 on successful packet reception, 1 on failure or no packet
 * 
 * Receives ICMP reply packet and processes it if it matches a sent packet
 */
int receive_packet(t_ping_state *state, int sockfd) {
	char buffer[1024];
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	uint16_t received_sequence;

	ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT, 
									 (struct sockaddr*)&from, &fromlen);
	
	if (bytes_received < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 1;
		} else {
			perror("recvfrom");
			return 1;
		}
	}
	
	if (parse_icmp_reply(buffer, bytes_received, &received_sequence, state) == 0) {
		if (find_packet(state, received_sequence)) {
			remove_packet(state, received_sequence);
			return 0; 
		}
	}
	return 1;
}

/**
 * @param state - ping state containing options and timing info
 * @param sequence - sequence number of packet to check
 * @return 1 if packet should be sent, 0 if not
 * 
 * Determines if a packet should be sent based on count limits and timing
 */
static int can_send(t_ping_state *state, uint16_t sequence) {
	struct timeval now;
	gettimeofday(&now, NULL);
	
	if (state->opts.count != -1 && sequence > state->opts.count) {
		return 0;
	}
	
	if (state->stats.preload_sent < state->opts.preload) {
		return 1;
	} else if (state->stats.last_packet_time.tv_sec == 0) {
		return 1;
	} else {
		long elapsed = timeval_diff_ms(&state->stats.last_packet_time, &now);
		return (elapsed >= 1000);
	}
}

/**
 * @param state - ping state containing connection info
 * @param packet - packet entry to send
 * @param sockfd - socket file descriptor to send through
 * @return 0 on success, 1 on failure
 * 
 * Sends ICMP packet through the specified socket
 */
static int send_packet(t_ping_state *state, t_packet_entry *packet, int sockfd) {
	struct sockaddr *addr;
	socklen_t addr_len;
	
	if (sockfd == state->conn.ipv4.sockfd) {
		addr = (struct sockaddr*)&state->conn.ipv4.addr;
		addr_len = state->conn.ipv4.addr_len;
	} else {
		addr = (struct sockaddr*)&state->conn.ipv6.addr;
		addr_len = state->conn.ipv6.addr_len;
	}
	
	ssize_t bytes_sent = sendto(sockfd, packet->packet, state->opts.psize, 0, addr, addr_len);
	if (bytes_sent < 0) {
		perror("sendto");
		return 1;
	}
	
	if ((size_t)bytes_sent != state->opts.psize) {
		fprintf(stderr, "sendto: partial packet sent (%zd of %zu bytes)\n", 
				bytes_sent, state->opts.psize);
		return 1;
	}
	return 0;
}

/**
 * @param state - ping state to update with send statistics
 * @param packet - packet entry that was sent
 * @param sequence - pointer to sequence number to increment
 * 
 * Updates packet timing, send statistics, and transmission completion status
 */
static void update_stats(t_ping_state *state, t_packet_entry *packet, uint16_t *sequence) {
	struct timeval now;
	gettimeofday(&now, NULL);
	
	packet->send_time = now;
	
	if (state->stats.packets_sent == 0) {
		gettimeofday(&state->stats.first_packet_time, NULL);
	}
	state->stats.packets_sent++;
	if (state->stats.preload_sent < state->opts.preload) {
		state->stats.preload_sent++;
	}
	state->stats.last_packet_time = now;
	(*sequence)++;
	
	if (state->opts.count != -1 && *sequence > state->opts.count) {
		state->stats.transmission_complete = 1;
	}
}

/**
 * @param state - ping state containing options and packet tracking
 * @param sequence - pointer to current sequence number
 * @param target_sockfd - socket file descriptor for target protocol
 * @return 0 on success, 1 on failure
 * 
 * Main packet sending function - validates, creates, and sends ICMP packets
 */
int send_ping(t_ping_state *state, uint16_t *sequence, int target_sockfd) {
	if (!can_send(state, *sequence)) {
		if (state->opts.count != -1 && *sequence > state->opts.count) {
			state->stats.transmission_complete = 1;
		}
		return 0;
	}
	
	t_packet_entry *packet = create_packet(state, *sequence);
	if (!packet) {
		fprintf(stderr, "Failed to create packet %d\n", *sequence);
		cleanup_packets(state);
		close(state->conn.ipv4.sockfd);
		close(state->conn.ipv6.sockfd);
		exit(1); 
	}
	
	if (send_packet(state, packet, target_sockfd) == 0) {
		update_stats(state, packet, sequence);
		return 0;
	}
	
	return 1;
}
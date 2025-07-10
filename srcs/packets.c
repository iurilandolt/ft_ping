#include "../includes/ft_ping.h"

/**
 * @param state - ping state to initialize packet system for
 * @return 0 on success
 * 
 * Initializes packet tracking system and adjusts packet size for headers
 */
int init_packet_system(t_ping_state *state) {
	state->sent_packets = NULL;
	state->stats.rtt_list = NULL;
	size_t header_size = (state->conn.target_family == AF_INET) ? 
						sizeof(struct icmphdr) : 
						sizeof(struct icmp6_hdr);    
	state->opts.psize += header_size; 
	return 0;
}

/**
 * @param state - ping state containing packet list and connection info
 * @param sequence - sequence number for the new packet
 * @return pointer to created packet entry, NULL on failure
 * 
 * Creates new ICMP packet with specified sequence number and adds to tracking list
 */
t_packet_entry* create_packet(t_ping_state *state, uint16_t sequence) {
	t_packet_entry *entry = malloc(sizeof(t_packet_entry));
	if (!entry) {
		fprintf(stderr, "malloc failed for packet entry\n");
		return NULL;
	}
	entry->packet = malloc(state->opts.psize);
	if (!entry->packet) {
		fprintf(stderr, "malloc failed for packet %d\n", sequence);
		free(entry);
		return NULL;
	}
	
	entry->sequence = sequence;
	entry->next = state->sent_packets;
	state->sent_packets = entry;
	
	struct icmphdr *icmp = &entry->packet->header;
	
	if (state->conn.target_family == AF_INET) {
		state->conn.ipv4.pid = getpid();
		icmp->type = ICMP_ECHO;
		icmp->un.echo.id = htons(state->conn.ipv4.pid);
	} else {
		state->conn.ipv6.pid = getpid();
		icmp->type = ICMP6_ECHO_REQUEST;
		icmp->un.echo.id = htons(state->conn.ipv6.pid);
	}
	
	icmp->code = 0;
	icmp->un.echo.sequence = htons(sequence);
	icmp->checksum = 0;
	fill_packet_data(state, sequence);
	icmp->checksum = calculate_checksum(state, sequence);
	
	return entry;
}

/**
 * @param state - ping state containing packet list
 * @param sequence - sequence number to search for
 * @return pointer to packet entry if found, NULL otherwise
 * 
 * Searches for packet entry with specified sequence number in tracking list
 */
t_packet_entry* find_packet(t_ping_state *state, uint16_t sequence) {
	t_packet_entry *current = state->sent_packets;
	while (current) {
		if (current->sequence == sequence) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

/**
 * @param state - ping state containing packet list
 * @param sequence - sequence number of packet to remove
 * 
 * Removes and frees packet entry with specified sequence number from tracking list
 */
void remove_packet(t_ping_state *state, uint16_t sequence) {
	t_packet_entry **current = &state->sent_packets;
	while (*current) {
		if ((*current)->sequence == sequence) {
			t_packet_entry *to_remove = *current;
			*current = (*current)->next;
			free(to_remove->packet);
			free(to_remove);
			return;
		}
		current = &(*current)->next;
	}
}

/**
 * @param state - ping state containing packet list
 * 
 * Frees all remaining packets in the tracking list and resets list pointer
 */
void cleanup_packets(t_ping_state *state) {
	t_packet_entry *current = state->sent_packets;
	while (current) {
		t_packet_entry *next = current->next;
		free(current->packet);
		free(current);
		current = next;
	}
	cleanup_rtt_list(state);
	state->sent_packets = NULL;
}

/**
 * @param state - ping state containing packet options
 * @param sequence - sequence number of packet to fill
 * 
 * Fills packet data payload with timestamp and pattern data
 */
void fill_packet_data(t_ping_state *state, uint16_t sequence) {
	t_packet_entry *entry = find_packet(state, sequence);
	if (!entry) return;
	
	size_t icmp_header_size = (state->conn.target_family == AF_INET) ? 
							 sizeof(struct icmphdr) : 
							 sizeof(struct icmp6_hdr);
	size_t data_size = state->opts.psize - icmp_header_size;
	size_t start_index = 0;
	
	if (data_size >= sizeof(struct timeval)) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		memcpy(&entry->packet->msg, &tv, sizeof(tv));
		start_index = sizeof(struct timeval);
	}
	
	for (size_t i = start_index; i < data_size; i++) {
		entry->packet->msg[i] = 0x10 + (i % 48);
	}
}

/**
 * @param state - ping state containing packet list and size info
 * @param sequence - sequence number of packet to calculate checksum for
 * @return calculated checksum value, 0 if packet not found
 * 
 * Calculates RFC 792 Internet checksum for ICMP packet
 */
uint16_t calculate_checksum(t_ping_state *state, uint16_t sequence) {
	t_packet_entry *entry = find_packet(state, sequence);
	if (!entry) return 0;
	
	uint16_t *ptr = (uint16_t*)entry->packet;
	int bytes = state->opts.psize;
	uint32_t sum = 0;
	
	while (bytes > 1) {
		sum += *ptr++;
		bytes -= 2;
	}
	
	if (bytes == 1) {
		sum += *(uint8_t*)ptr;
	}
	
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	return ~sum;
}

/**
 * @param buffer - received packet buffer
 * @param bytes_received - total bytes received
 * @param state - ping state containing connection and statistics info
 * @param from - source address from recvfrom() call
 * @return initialized ICMP context structure
 * 
 * Creates and initializes ICMP context with parsed headers and common data
 */
t_icmp_context create_icmp_context(char *buffer, ssize_t bytes_received, t_ping_state *state, struct sockaddr_storage *from) {
	t_icmp_context ctx = {
		.buffer = buffer,
		.bytes_received = bytes_received,
		.state = state,
		.from = from,
		.ip_header = (state->conn.target_family == AF_INET) ? (struct iphdr*)buffer : NULL,
		.icmp_header = (state->conn.target_family == AF_INET) ? 
					   (struct icmphdr*)(buffer + ((struct iphdr*)buffer)->ihl * 4) : 
					   (struct icmphdr*)buffer,
		.expected_pid = (state->conn.target_family == AF_INET) ? 
						state->conn.ipv4.pid : state->conn.ipv6.pid,
		.packet_id = 0,
		.sequence = 0
	};
	return ctx;
}


int is_icmp_error(uint8_t icmp_type, int family) {
	if (family == AF_INET) {
		return (icmp_type == ICMP_TIME_EXCEEDED || 
				icmp_type == ICMP_DEST_UNREACH);
	} else {
		return (icmp_type == ICMP6_TIME_EXCEEDED || 
				icmp_type == ICMP6_DST_UNREACH);
	}
}


int is_icmp_reply(uint8_t icmp_type, int family) {
	if (family == AF_INET) {
		return (icmp_type == ICMP_ECHOREPLY);
	} else {
		return (icmp_type == ICMP6_ECHO_REPLY);
	}
}

int handle_icmp_errors(t_icmp_context *ctx) {
	size_t error_min_size = sizeof(struct iphdr) + sizeof(struct icmphdr) + 
							sizeof(struct iphdr) + sizeof(struct icmphdr);
	if ((unsigned long)ctx->bytes_received < error_min_size) {
		return 1;
	}
	
	struct iphdr *orig_ip = (struct iphdr*)((char*)ctx->icmp_header + sizeof(struct icmphdr));
	struct icmphdr *orig_icmp = (struct icmphdr*)((char*)orig_ip + (orig_ip->ihl * 4));
	
	ctx->packet_id = ntohs(orig_icmp->un.echo.id);
	ctx->sequence = ntohs(orig_icmp->un.echo.sequence);
	
	uint8_t expected_type = (ctx->state->conn.target_family == AF_INET) ? 
							ICMP_ECHO : ICMP6_ECHO_REQUEST;
	
	if (orig_icmp->type != expected_type || ctx->packet_id != ctx->expected_pid) {
		return 1;
	}
	
	t_packet_entry *packet_entry = find_packet(ctx->state, ctx->sequence);
	if (!packet_entry) {
		return 1;
	}
	
	char hostname[NI_MAXHOST];
	char sender_ip[INET_ADDRSTRLEN];
	struct sockaddr_in *from_addr = (struct sockaddr_in*)ctx->from;
	
	inet_ntop(AF_INET, &from_addr->sin_addr, sender_ip, INET_ADDRSTRLEN);
	
	if (getnameinfo((struct sockaddr*)from_addr, sizeof(*from_addr), 
					hostname, sizeof(hostname), NULL, 0, 0) == 0) {
		printf("From %s (%s): icmp_seq=%d Time to live exceeded\n", 
			   hostname, sender_ip, ctx->sequence);
	} else {
		printf("From %s: icmp_seq=%d Time to live exceeded\n", 
			   sender_ip, ctx->sequence);
	}
	
	remove_packet(ctx->state, ctx->sequence);
	return 0;
}

int handle_icmp_replies(t_icmp_context *ctx) {
	ctx->packet_id = ntohs(ctx->icmp_header->un.echo.id);
	ctx->sequence = ntohs(ctx->icmp_header->un.echo.sequence);
	
	if (ctx->packet_id != ctx->expected_pid) {
		return 1;
	}
	
	t_packet_entry *packet_entry = find_packet(ctx->state, ctx->sequence);
	if (!packet_entry) {
		return 1;
	}
	
	size_t icmp_size = (ctx->state->conn.target_family == AF_INET) ? 
					ctx->bytes_received - (ctx->ip_header->ihl * 4) : 
					ctx->bytes_received;
	size_t icmp_data_size = icmp_size - sizeof(struct icmphdr);
	int ttl = (ctx->state->conn.target_family == AF_INET) ? ctx->ip_header->ttl : 64;
	
	double rtt = calculate_rtt(ctx->buffer, ctx->ip_header, icmp_data_size, ctx->state->conn.target_family);
	update_rtt_stats(ctx->state, rtt);
	print_ping_reply(ctx->state, icmp_size, ctx->icmp_header, ttl, rtt);
	
	remove_packet(ctx->state, ctx->sequence);
	return 0;
}

/**
 * @param buffer - received packet buffer
 * @param bytes_received - total bytes received
 * @param state - ping state containing connection and statistics info
 * @param from - source address from recvfrom() call
 * @return 0 if valid reply packet processed, 1 otherwise
 * 
 * Parses ICMP reply packet and dispatches to appropriate handler
 */
int parse_icmp_reply(char *buffer, ssize_t bytes_received, t_ping_state *state, struct sockaddr_storage *from) {

	size_t min_size = (state->conn.target_family == AF_INET) ? 
					sizeof(struct iphdr) + sizeof(struct icmphdr) :
					sizeof(struct icmphdr);
	if ((unsigned long)bytes_received < min_size) {
		return 1;
	}
	
	t_icmp_context ctx = create_icmp_context(buffer, bytes_received, state, from);
	
	if (is_icmp_error(ctx.icmp_header->type, state->conn.target_family)) {
		return handle_icmp_errors(&ctx);
	} else if (is_icmp_reply(ctx.icmp_header->type, state->conn.target_family)) {
		return handle_icmp_replies(&ctx);
	} else {
		return 1; 
	}
}


#include "../includes/ft_ping.h"


/**
 * @param buffer - received packet buffer
 * @param bytes_received - total bytes received
 * @param state - ping state containing connection and statistics info
 * @param from - source address from recvfrom() call
 * @return initialized ICMP context structure
 * 
 * Creates and initializes ICMP context with parsed headers and common data
 */
static t_icmp_context create_icmp_context(char *buffer, ssize_t bytes_received, t_ping_state *state, struct sockaddr_storage *from) {
	t_icmp_context ctx = {
		.buffer = buffer,
		.bytes_received = bytes_received,
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


static int get_icmp_packet_type(uint8_t icmp_type, int family) {
	if (family == AF_INET) {
		if (icmp_type == ICMP_ECHOREPLY) return 1; 
		if (icmp_type == ICMP_TIME_EXCEEDED || icmp_type == ICMP_DEST_UNREACH) return 2; 
	} else {
		if (icmp_type == ICMP6_ECHO_REPLY) return 1; 
		if (icmp_type == ICMP6_TIME_EXCEEDED || icmp_type == ICMP6_DST_UNREACH) return 2; 
	}
	return 0; 
}

static int handle_icmp_errors(t_icmp_context *ctx, t_ping_state *state) {
	size_t error_min_size = sizeof(struct iphdr) + sizeof(struct icmphdr) * 2; // sizeof(struct ipv6hdr) * 2 + sizeof(struct icmp6_hdr) * 2;
	if ((unsigned long)ctx->bytes_received < error_min_size) {
		return 1;
	}
	
	struct iphdr *orig_ip = (struct iphdr*)((char*)ctx->icmp_header + sizeof(struct icmphdr));
	struct icmphdr *orig_icmp = (struct icmphdr*)((char*)orig_ip + (orig_ip->ihl * 4));
	
	ctx->packet_id = ntohs(orig_icmp->un.echo.id);
	ctx->sequence = ntohs(orig_icmp->un.echo.sequence);
	
	uint8_t expected_type = (state->conn.target_family == AF_INET) ? 
							ICMP_ECHO : ICMP6_ECHO_REQUEST;
	
	if (orig_icmp->type != expected_type || ctx->packet_id != ctx->expected_pid) {
		return 1;
	}
	
	t_packet_entry *packet_entry = find_packet(state, ctx->sequence);
	if (!packet_entry) {
		return 1;
	}
	
	print_icmp_error(ctx, "Time to live exceeded");
	state->stats.errors++;
	remove_packet(state, ctx->sequence);
	return 0;
}

static int handle_icmp_replies(t_icmp_context *ctx, t_ping_state *state) {
	ctx->packet_id = ntohs(ctx->icmp_header->un.echo.id);
	ctx->sequence = ntohs(ctx->icmp_header->un.echo.sequence);
	
	if (ctx->packet_id != ctx->expected_pid) {
		return 1;
	}
	
	t_packet_entry *packet_entry = find_packet(state, ctx->sequence);
	if (!packet_entry) {
		return 1;
	}
	
	size_t icmp_size = (state->conn.target_family == AF_INET) ? 
					ctx->bytes_received - (ctx->ip_header->ihl * 4) : 
					ctx->bytes_received;
	size_t icmp_data_size = icmp_size - sizeof(struct icmphdr);
	
	int ttl = (state->conn.target_family == AF_INET) ? ctx->ip_header->ttl : 64; // ((struct ipv6hdr*)ctx->buffer)->hop_limit
	
	double rtt = calculate_rtt(ctx->buffer, ctx->ip_header, icmp_data_size, state->conn.target_family);
	update_rtt_stats(state, rtt);
	print_ping_reply(state, icmp_size, ctx->icmp_header, ttl, rtt);
	
	remove_packet(state, ctx->sequence);
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
	
	switch (get_icmp_packet_type(ctx.icmp_header->type, state->conn.target_family)) {
		case 1: // reply
			return handle_icmp_replies(&ctx, state);
		case 2: // error
			return handle_icmp_errors(&ctx, state);
		default: // unknown
			return 1;
	}
}
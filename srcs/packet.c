#include "../includes/ft_ping.h"

void create_icmp_packet(t_ping_state *state, uint16_t sequence) {
	struct icmphdr *icmp = &state->packet.header;
	
	state->conn.pid = getpid();

	icmp->type = (state->conn.family == AF_INET) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;

	icmp->code = 0;                   //  0 for ping
	icmp->un.echo.id = htons(getpid()); // PID in network byte order
	icmp->un.echo.sequence = htons(sequence); // sequence number in network byte order
	icmp->checksum = 0;               
	fill_packet_data(state);
	icmp->checksum = calculate_checksum(state);
}

uint16_t calculate_checksum(t_ping_state *state) {
	uint16_t *ptr = (uint16_t*)&state->packet;
	int bytes = state->opts.psize;
	uint32_t sum = 0;
	
	// Sum all 16-bit words
	while (bytes > 1) {
		sum += *ptr++;
		bytes -= 2;
	}
	
	// Add odd byte if present
	if (bytes == 1) {
		sum += *(uint8_t*)ptr;
	}
	
	// Add carry and invert
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	
	return ~sum;
}

void fill_packet_data(t_ping_state *state) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	// timestamp at beginning of data
	memcpy(state->packet.msg, &tv, sizeof(tv));
	
	// fill with a pattern
	for (size_t i = sizeof(tv); i < state->opts.psize - sizeof(struct icmphdr); i++) {
		state->packet.msg[i] = 0x10 + (i % 48); // 0x10, 0x11, 0x12...
	}
}

int parse_icmp_reply(char *buffer, ssize_t bytes_received, uint16_t expected_sequence, t_ping_state *state) {
    // size validation
    if ((unsigned long)bytes_received < sizeof(struct iphdr) + sizeof(struct icmphdr)) {
        return -1;
    }
    
    // parse packet 
	struct iphdr *ip_header = (state->conn.family == AF_INET) ? 
					(struct iphdr*)buffer : NULL;
	struct icmphdr *icmp_header = (state->conn.family == AF_INET) ? 
					(struct icmphdr*)(buffer + (ip_header->ihl * 4)) : (struct icmphdr*)buffer;
    // its our ping reply ?
	int expected_type = (state->conn.family == AF_INET) ? ICMP_ECHOREPLY : ICMP6_ECHO_REPLY;
	if (icmp_header->type != expected_type ||
		ntohs(icmp_header->un.echo.id) != state->conn.pid ||
		ntohs(icmp_header->un.echo.sequence) != expected_sequence) {
		return -1;
	}
    
    // calc RTT 
    struct timeval now, *sent_time;
    gettimeofday(&now, NULL);
	if (state->conn.family == AF_INET) {
		sent_time = (struct timeval*)(buffer + (ip_header->ihl * 4) + sizeof(struct icmphdr));
	} else {
		sent_time = (struct timeval*)(buffer + sizeof(struct icmphdr)); // IPv6: no IP header
	}
    
    double rtt = (now.tv_sec - sent_time->tv_sec) * 1000.0 + 
                 (now.tv_usec - sent_time->tv_usec) / 1000.0;
    
    // update stats 
    if (state->stats.packets_received == 0 || rtt < state->stats.min_rtt) {
        state->stats.min_rtt = rtt;
    }
    if (rtt > state->stats.max_rtt) {
        state->stats.max_rtt = rtt;
    }
    state->stats.sum_rtt += rtt;
    
	size_t icmp_size = (state->conn.family == AF_INET) ? 
					   bytes_received - (ip_header->ihl * 4) : 
					   bytes_received;
	
	int ttl = (state->conn.family == AF_INET) ? ip_header->ttl : 64;
	
	if (state->opts.verbose) {
		fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ident=%d ttl=%d time=%.3f ms\n",
			icmp_size, state->conn.addr_str, ntohs(icmp_header->un.echo.sequence),
			ntohs(icmp_header->un.echo.id), ttl, rtt);
	} else {
		fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
			icmp_size, state->conn.addr_str, ntohs(icmp_header->un.echo.sequence), 
			ttl, rtt);
	}
	
    return 0;
}

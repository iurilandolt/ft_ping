#include "../includes/ft_ping.h"

void create_icmp_packet(t_ping_state *state, uint16_t sequence) {
	struct icmphdr *icmp = &state->packet->header;
	
	state->conn.pid = getpid();
	icmp->code = 0;                
	icmp->type = (state->conn.family == AF_INET) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
	icmp->un.echo.id = htons(state->conn.pid); 
	icmp->un.echo.sequence = htons(sequence);
	icmp->checksum = 0;               
	fill_packet_data(state);
	icmp->checksum = calculate_checksum(state);
}

int allocate_packet(t_ping_state *state) {
    size_t header_size = (state->conn.family == AF_INET) ? 
                        sizeof(struct icmphdr) : 
                        sizeof(struct icmp6_hdr);    
    
    state->opts.psize += header_size;
    
    state->packet = malloc(state->opts.psize);
    if (!state->packet) {
        perror("malloc");
        return 1;
    }
    return 0;
}

void free_packet(t_ping_state *state) {
    if (state->packet) {
        free(state->packet);
        state->packet = NULL;
    }
}

uint16_t calculate_checksum(t_ping_state *state) {
	uint16_t *ptr = (uint16_t*)state->packet;
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
    size_t icmp_header_size = (state->conn.family == AF_INET) ? 
                             sizeof(struct icmphdr) : 
                             sizeof(struct icmp6_hdr);
    size_t data_size = state->opts.psize - icmp_header_size;
    
    if (data_size >= sizeof(struct timeval)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        memcpy(&state->packet->msg, &tv, sizeof(tv));
        

        for (size_t i = sizeof(tv); i < data_size; i++) {
            state->packet->msg[i] = 0x10 + (i % 48);
        }
    } else {
        for (size_t i = 0; i < data_size; i++) {
            state->packet->msg[i] = 0x10 + (i % 48);
        }
    }
}

static double calculate_rtt(char *buffer, struct iphdr *ip_header, size_t icmp_data_size, 
						int family) {
	if (icmp_data_size < sizeof(struct timeval)) {
		return -1.0; 
	}
	
	struct timeval now, *sent_time;
	gettimeofday(&now, NULL);
	
	if (family == AF_INET) {
		sent_time = (struct timeval*)(buffer + (ip_header->ihl * 4) + sizeof(struct icmphdr));
	} else {
		sent_time = (struct timeval*)(buffer + sizeof(struct icmphdr));
	}
	
	return (now.tv_sec - sent_time->tv_sec) * 1000.0 + 
		(now.tv_usec - sent_time->tv_usec) / 1000.0;
}

static void update_rtt_stats(t_ping_state *state, double rtt) {
	if (state->stats.packets_received == 0 || rtt < state->stats.min_rtt) {
		state->stats.min_rtt = rtt;
	}
	if (rtt > state->stats.max_rtt) {
		state->stats.max_rtt = rtt;
	}
	state->stats.sum_rtt += rtt;
}

int parse_icmp_reply(char *buffer, ssize_t bytes_received, uint16_t expected_sequence, t_ping_state *state) {
    size_t min_size = (state->conn.family == AF_INET) ? 
                      sizeof(struct iphdr) + sizeof(struct icmphdr) :
                      sizeof(struct icmphdr);
    if ((unsigned long)bytes_received < min_size) {
        return -1;
    }
	
	struct iphdr *ip_header = (state->conn.family == AF_INET) ? 
							(struct iphdr*)buffer : NULL;
	struct icmphdr *icmp_header = (state->conn.family == AF_INET) ? 
								(struct icmphdr*)(buffer + (ip_header->ihl * 4)) : 
								(struct icmphdr*)buffer;
	
	int expected_type = (state->conn.family == AF_INET) ? ICMP_ECHOREPLY : ICMP6_ECHO_REPLY;
	if (icmp_header->type != expected_type ||
		ntohs(icmp_header->un.echo.id) != state->conn.pid ||
		ntohs(icmp_header->un.echo.sequence) != expected_sequence) {
		return -1;
	}
	
	// should verify the checksum here, but skipping for brevity?

	size_t icmp_size = (state->conn.family == AF_INET) ? 
					bytes_received - (ip_header->ihl * 4) : 
					bytes_received;
	size_t icmp_data_size = icmp_size - sizeof(struct icmphdr);
	int ttl = (state->conn.family == AF_INET) ? ip_header->ttl : 64;
	double rtt = calculate_rtt(buffer, ip_header, icmp_data_size, state->conn.family);
	
	update_rtt_stats(state, rtt);
	print_ping_reply(state, icmp_size, icmp_header, ttl, rtt);
	
	return 0;
}

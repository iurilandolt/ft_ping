#include "../includes/ft_ping.h"

int init_packet_system(t_ping_state *state) {
	state->sent_packets = NULL;
	size_t header_size = (state->conn.target_family == AF_INET) ? 
						sizeof(struct icmphdr) : 
						sizeof(struct icmp6_hdr);    
	state->opts.psize += header_size; 
	return 0;
}

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

void cleanup_packets(t_ping_state *state) {
	t_packet_entry *current = state->sent_packets;
	while (current) {
		t_packet_entry *next = current->next;
		free(current->packet);
		free(current);
		current = next;
	}
	state->sent_packets = NULL;
}

void fill_packet_data(t_ping_state *state, uint16_t sequence) {
    t_packet_entry *entry = find_packet(state, sequence);
    if (!entry) return;
    
    size_t icmp_header_size = (state->conn.target_family == AF_INET) ? 
                             sizeof(struct icmphdr) : 
                             sizeof(struct icmp6_hdr);
    size_t data_size = state->opts.psize - icmp_header_size;
    
    if (data_size >= sizeof(struct timeval)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        memcpy(&entry->packet->msg, &tv, sizeof(tv));
        
        for (size_t i = sizeof(tv); i < data_size; i++) {
            entry->packet->msg[i] = 0x10 + (i % 48);
        }
    } else {
        for (size_t i = 0; i < data_size; i++) {
            entry->packet->msg[i] = 0x10 + (i % 48);
        }
    }
}

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

static double calculate_rtt(char *buffer, struct iphdr *ip_header, size_t icmp_data_size, int family) {
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

int parse_icmp_reply(char *buffer, ssize_t bytes_received, uint16_t *found_sequence, t_ping_state *state) {
	size_t min_size = (state->conn.target_family == AF_INET) ? 
					sizeof(struct iphdr) + sizeof(struct icmphdr) :
					sizeof(struct icmphdr);
					
	if ((unsigned long)bytes_received < min_size) {
		return 1;
	}
	
	struct iphdr *ip_header = (state->conn.target_family == AF_INET) ? 
							(struct iphdr*)buffer : NULL;
	struct icmphdr *icmp_header = (state->conn.target_family == AF_INET) ? 
								(struct icmphdr*)(buffer + (ip_header->ihl * 4)) : 
								(struct icmphdr*)buffer;
	
	int expected_type = (state->conn.target_family == AF_INET) ? ICMP_ECHOREPLY : ICMP6_ECHO_REPLY;
	uint16_t expected_pid = (state->conn.target_family == AF_INET) ? 
						state->conn.ipv4.pid : state->conn.ipv6.pid;
	
	// Check if this is our packet (correct type and PID)
	if (icmp_header->type != expected_type ||
		ntohs(icmp_header->un.echo.id) != expected_pid) {
		return 1;
	}
	
	// Extract the sequence number
	*found_sequence = ntohs(icmp_header->un.echo.sequence);
	
	size_t icmp_size = (state->conn.target_family == AF_INET) ? 
					bytes_received - (ip_header->ihl * 4) : 
					bytes_received;
	size_t icmp_data_size = icmp_size - sizeof(struct icmphdr);
	int ttl = (state->conn.target_family == AF_INET) ? ip_header->ttl : 64;
	double rtt = calculate_rtt(buffer, ip_header, icmp_data_size, state->conn.target_family);
	
	update_rtt_stats(state, rtt);
	print_ping_reply(state, icmp_size, icmp_header, ttl, rtt);
	
	return 0;
}
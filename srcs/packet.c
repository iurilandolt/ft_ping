#include "../includes/ft_ping.h"

/**
 * @param state - ping state to initialize packet system for
 * @return 0 on success
 * 
 * Initializes packet tracking system and adjusts packet size for headers
 */
int init_packet_system(t_ping_state *state) {
	state->sent_packets = NULL;
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
 * @param ip_header - IP header pointer for IPv4 packets
 * @param icmp_data_size - size of ICMP data payload
 * @param family - address family (AF_INET or AF_INET6)
 * @return round-trip time in milliseconds, -1.0 if no timestamp available
 * 
 * Calculates round-trip time from embedded timestamp in packet payload
 */
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

/**
 * @param state - ping state containing RTT statistics
 * @param rtt - new round-trip time measurement
 * 
 * Updates min/max/sum RTT statistics with new measurement
 */
static void update_rtt_stats(t_ping_state *state, double rtt) {
    if (state->stats.packets_received == 0 || rtt < state->stats.min_rtt) {
        state->stats.min_rtt = rtt;
    }
    if (rtt > state->stats.max_rtt) {
        state->stats.max_rtt = rtt;
    }
    state->stats.sum_rtt += rtt;
}

/**
 * @param buffer - received packet buffer
 * @param bytes_received - total bytes received
 * @param found_sequence - pointer to store extracted sequence number
 * @param state - ping state containing connection and statistics info
 * @return 0 if valid reply packet processed, 1 otherwise
 * 
 * Parses ICMP reply packet and extracts sequence number and timing information
 */
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
	
	if (icmp_header->type != expected_type ||
		ntohs(icmp_header->un.echo.id) != expected_pid) {
		return 1;
	}
	
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
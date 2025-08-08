#include "../includes/ft_ping.h"

/**
 * @param buffer - received packet buffer
 * @param ip_header - IP header pointer for IPv4 packets
 * @param icmp_data_size - size of ICMP data payload
 * @param family - address family (AF_INET or AF_INET6)
 * @return round-trip time in milliseconds, -1.0 if no timestamp available
 * 
 * Calculates round-trip time from embedded timestamp in packet payload
 */
double calculate_rtt(char *buffer, struct iphdr *ip_header, size_t icmp_data_size, int family) {
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
 * @param state - ping state containing RTT list
 * @param rtt - RTT value to insert in sorted order
 * 
 * Inserts RTT value into sorted linked list maintaining ascending order
 */

static void insert_rtt_sorted(t_ping_state *state, double rtt) {
	t_rtt_entry *new_entry = malloc(sizeof(t_rtt_entry));
	if (!new_entry) {
		fprintf(stderr, "malloc failed for RTT entry\n");
		cleanup_packets(state);
		close(state->conn.ipv4.sockfd);
		close(state->conn.ipv6.sockfd);
		exit(1); 
	}
	
	new_entry->rtt = rtt;
	if (!state->stats.rtt_list || rtt <= state->stats.rtt_list->rtt) {
		new_entry->next = state->stats.rtt_list;
		state->stats.rtt_list = new_entry;
		return;
	}
	
	t_rtt_entry *current = state->stats.rtt_list;
	while (current->next && current->next->rtt < rtt) {
		current = current->next;
	}
	
	new_entry->next = current->next;
	current->next = new_entry;
}

void update_rtt_stats(t_ping_state *state, double rtt) {
	if (state->stats.packets_received == 0 || rtt < state->stats.min_rtt) {
		state->stats.min_rtt = rtt;
	}
	if (rtt > state->stats.max_rtt) {
		state->stats.max_rtt = rtt;
	}
	state->stats.sum_rtt += rtt;
	insert_rtt_sorted(state, rtt);
}

/**
 * @param state - ping state containing RTT list
 * 
 * Frees all RTT entries in the list and resets list pointer
 */

void cleanup_rtt_list(t_ping_state *state) {
	t_rtt_entry *current = state->stats.rtt_list;
	while (current) {
		t_rtt_entry *next = current->next;
		free(current);
		current = next;
	}
	state->stats.rtt_list = NULL;
}

double calculate_mean_deviation(t_ping_state *state) {
    if (state->stats.packets_received == 0) {
        return 0.0;
    }
    double sum_dev = 0.0;
    double avg = state->stats.avg_rtt;
    t_rtt_entry *current = state->stats.rtt_list;
    while (current) {
        sum_dev += fabs(current->rtt - avg);
        current = current->next;
    }
    return sum_dev / state->stats.packets_received;
}
#include "../includes/ft_ping.h"

long timeval_diff_ms(struct timeval *start, struct timeval *end) {
    if (start->tv_sec == 0) {
        return 0; // No previous time set
    }
    return (end->tv_sec - start->tv_sec) * 1000 + 
           (end->tv_usec - start->tv_usec) / 1000;
}

int setupPoll(t_ping_state *state, struct pollfd *fds) {
	int sockets[] = {state->conn.ipv4.sockfd, state->conn.ipv6.sockfd};
	
	for (int i = 0; i < 2; i++) {
		fds[i].fd = sockets[i];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}
	return (state->conn.target_family == AF_INET) ? 
			state->conn.ipv4.sockfd : 
			state->conn.ipv6.sockfd;
}

int get_next_poll_timeout(t_ping_state *state) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // If we need to send more packets, use send interval timing
    if (!state->stats.transmission_complete && state->stats.preload_sent >= state->opts.preload) {
        if (state->stats.last_send_time.tv_sec != 0) {
            long since_last = timeval_diff_ms(&state->stats.last_send_time, &now);
            int until_next_send = 1000 - since_last;
            return (until_next_send > 0) ? until_next_send : 0;
        }
        return 0; // Send immediately if no last send time
    }
    
    // When all packets are sent, give minimal grace period for replies
    if (state->stats.transmission_complete) {
        return 100; // Short grace period for late replies
    }
    
    // Still in preload phase - don't wait, send immediately
    return 0;
}


void handle_timeouts(t_ping_state *state) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    t_packet_entry *current = state->sent_packets;
    t_packet_entry *prev = NULL;
    
    while (current != NULL) {
        long elapsed = timeval_diff_ms(&current->send_time, &now);
        
        if (elapsed >= state->opts.timeout * 1000) {
            // Remove timed out packet
            if (prev == NULL) {
                state->sent_packets = current->next;
            } else {
                prev->next = current->next;
            }
            
            // if (state->opts.verbose) {
            //     printf("Request timeout for icmp_seq %d\n", current->sequence);
            // }
            
            t_packet_entry *to_free = current;
            current = current->next;
            free(to_free->packet);
            free(to_free);
        } else {
            prev = current;
            current = current->next;
        }
    }
}
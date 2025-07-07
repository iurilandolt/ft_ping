#include "../includes/ft_ping.h"

static long timeval_diff_ms(struct timeval *start, struct timeval *end) {
    if (start->tv_sec == 0) {
        return 0; // No previous time set
    }
    return (end->tv_sec - start->tv_sec) * 1000 + 
           (end->tv_usec - start->tv_usec) / 1000;
}

static int send_next_packet(t_ping_state *state, uint16_t *sequence, int target_sockfd, long time_since_last_send) {
    // Check if we've reached the count limit (if any)
    if (state->opts.count != -1 && *sequence > state->opts.count) {
        return 1; // No more packets to send
    }
    
    // Send preload packets first (no delay)
    if (state->stats.preload_sent < state->opts.preload) {
        // Send packet immediately
    }
    // Send normal packets with 1 second interval
    else if (state->stats.last_send_time.tv_sec == 0 || time_since_last_send >= 1000) {
        // Send packet after delay
    }
    else {
        return 1; // Not time to send yet
    }
    
    // Send the packet
    struct timeval now;
    gettimeofday(&now, NULL);
    
    create_packet(state, *sequence);
    if (send_packet(state, *sequence, target_sockfd) == 0) {
        if (state->stats.packets_sent == 0) {
            gettimeofday(&state->stats.first_packet_time, NULL);
        }
        state->stats.packets_sent++;
        if (state->stats.preload_sent < state->opts.preload) {
            state->stats.preload_sent++;
        }
        state->stats.last_send_time = now;
        (*sequence)++;
        return 0; // Success
    }
    return 1; // Send failed
}

static int calculate_poll_timeout(t_ping_state *state, int all_packets_sent, long time_since_last_send) {
    if (!all_packets_sent && time_since_last_send < 1000) {
        return 1000 - time_since_last_send;
    } else if (state->sent_packets != NULL) {
        return state->opts.timeout * 1000;
    }
    return -1; // Signal to break
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

int main(int argc, char **argv) {
    t_ping_state state;
    struct pollfd fds[2];
    int ret = 0;
    uint16_t sequence = 1;

    if (parseArgs(&state, argc, argv) ||
        resolveHost(&state, argv) || 
        createSocket(&state, argv) ||
        init_packet_system(&state)) {
        return ret = 1;
    }
    setupSignals(&state);
    
    memset(&state.stats, 0, sizeof(state.stats));
    gettimeofday(&state.stats.start_time, NULL);
    
    print_verbose_info(&state);
    print_default_info(&state);
    
    int target_sockfd = setupPoll(&state, fds);
    
    int all_packets_sent = 0;

    while (!all_packets_sent || state.sent_packets != NULL) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        // Calculate time since last send in milliseconds
        long time_since_last_send = timeval_diff_ms(&state.stats.last_send_time, &now);
        
        // Try to send next packet
        if (send_next_packet(&state, &sequence, target_sockfd, time_since_last_send) != 0) {
            // No packet was sent (either not time yet or send failed)
        }
        
        // Mark that we've sent all packets
        if (state.opts.count != -1 && sequence > state.opts.count) {
            all_packets_sent = 1;
        }
        
        // Calculate appropriate poll timeout
		int poll_timeout;
        if ((poll_timeout = calculate_poll_timeout(&state, all_packets_sent, time_since_last_send)) < 0) {
            break;
        }
        
        int poll_result = poll(fds, 2, poll_timeout);
        
        if (poll_result > 0) {
            for (int i = 0; i < 2; i++) {
                if (fds[i].revents & POLLIN) {
                    if ((ret = receive_packet(&state, fds[i].fd)) == 0) {
                        state.stats.packets_received++;
                    }
                    break;
                }
            }
        } else if (poll_result < 0 && errno != EINTR) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            break;
        }
    }

    print_stats(&state);
    cleanup_packets(&state);
	
    close(state.conn.ipv4.sockfd);
    close(state.conn.ipv6.sockfd);

    return ret;
}
#include "../includes/ft_ping.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static long timeval_diff_ms(struct timeval *start, struct timeval *end) {
    if (start->tv_sec == 0) {
        return 0; // No previous time set
    }
    return (end->tv_sec - start->tv_sec) * 1000 + 
           (end->tv_usec - start->tv_usec) / 1000;
}

static int should_send_packet_now(t_ping_state *state, uint16_t sequence) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // Check count limit
    if (state->opts.count != -1 && sequence > state->opts.count) {
        return 0;
    }
    
    // Preload packets - send immediately
    if (state->stats.preload_sent < state->opts.preload) {
        return 1;
    }
    
    // Regular packets - check 1 second interval
    if (state->stats.last_send_time.tv_sec == 0) {
        return 1; // First packet
    }
    
    long elapsed = timeval_diff_ms(&state->stats.last_send_time, &now);
    return (elapsed >= 1000);
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

static int send_packet_now(t_ping_state *state, uint16_t *sequence, int target_sockfd) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    create_packet(state, *sequence);
    if (send_packet(state, *sequence, target_sockfd) == 0) {
        // Set send_time for the packet we just sent
        t_packet_entry *sent_packet = find_packet(state, *sequence);
        if (sent_packet) {
            sent_packet->send_time = now;
        }
        
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

static void handle_timeouts(t_ping_state *state) {
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
            
            if (state->opts.verbose) {
                printf("Request timeout for icmp_seq %d\n", current->sequence);
            }
            
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

static int get_next_poll_timeout(t_ping_state *state, int all_packets_sent) {
    struct timeval now;
    gettimeofday(&now, NULL);
    
    // If we need to send more packets, use send interval timing
    if (!all_packets_sent && state->stats.preload_sent >= state->opts.preload) {
        if (state->stats.last_send_time.tv_sec != 0) {
            long since_last = timeval_diff_ms(&state->stats.last_send_time, &now);
            int until_next_send = 1000 - since_last;
            return (until_next_send > 0) ? until_next_send : 0;
        }
        return 0; // Send immediately if no last send time
    }
    
    // When all packets are sent, give minimal grace period for replies
    if (all_packets_sent) {
        return 100; // Short grace period for late replies
    }
    
    // Still in preload phase - don't wait, send immediately
    return 0;
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
    setup_alarm(&state);
    
    memset(&state.stats, 0, sizeof(state.stats));
    gettimeofday(&state.stats.start_time, NULL);
    
    print_verbose_info(&state);
    print_default_info(&state);
    
    int target_sockfd = setupPoll(&state, fds);
    
    int all_packets_sent = 0;

    while (!all_packets_sent || state.sent_packets != NULL) {
        // Try to send packet if it's time
        if (should_send_packet_now(&state, sequence)) {
            if ((ret = send_packet_now(&state, &sequence, target_sockfd) == 0)) {
                // Packet sent successfully
            }
        }
        
        // Update all_packets_sent flag
        if (state.opts.count != -1 && sequence > state.opts.count) {
            all_packets_sent = 1;
        }
        
        // Get appropriate timeout for poll
        int poll_timeout = get_next_poll_timeout(&state, all_packets_sent);
        if (poll_timeout < 0) {
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
        } else if (poll_result == 0) {
            handle_timeouts(&state);
        } else if (poll_result < 0 && errno != EINTR) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            break;
        }
    }

    print_stats(&state);
    cleanup_packets(&state);
	
    close(state.conn.ipv4.sockfd);
    close(state.conn.ipv6.sockfd);

    return (state.stats.packets_received == 0) ? 1 : ret;
}
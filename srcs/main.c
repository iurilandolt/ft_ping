#include "../includes/ft_ping.h"

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
    
    int preload_sent = 0;
    struct timeval last_send = {0, 0};
    int all_packets_sent = 0;

    while (!all_packets_sent || state.sent_packets != NULL) {
        struct timeval now;
        gettimeofday(&now, NULL);
        
        // Calculate time since last send in milliseconds
        long time_since_last_send = 0;
        if (last_send.tv_sec != 0) {
            time_since_last_send = (now.tv_sec - last_send.tv_sec) * 1000 + 
                                   (now.tv_usec - last_send.tv_usec) / 1000;
        }
        
        // Send preload packets first
        if (preload_sent < state.opts.preload && sequence <= (state.opts.count == -1 ? INT_MAX : state.opts.count)) {
            create_packet(&state, sequence);
            if ((ret = send_packet(&state, sequence, target_sockfd)) == 0) {
                if (state.stats.packets_sent == 0) {
                    // Record when we send the first packet
                    gettimeofday(&state.stats.first_packet_time, NULL);
                }
                state.stats.packets_sent++;
                preload_sent++;
                last_send = now;
                sequence++;
            }
        }
        // Send normal packets with 1 second interval (1000ms)
        else if (sequence <= (state.opts.count == -1 ? INT_MAX : state.opts.count) && 
                (last_send.tv_sec == 0 || time_since_last_send >= 1000)) {
            create_packet(&state, sequence);
            if ((ret = send_packet(&state, sequence, target_sockfd)) == 0) {
                if (state.stats.packets_sent == 0) {
                    // Record when we send the first packet
                    gettimeofday(&state.stats.first_packet_time, NULL);
                }
                state.stats.packets_sent++;
                last_send = now;
                sequence++;
            }
        }
        
        // Mark that we've sent all packets
        if (state.opts.count != -1 && sequence > state.opts.count) {
            all_packets_sent = 1;
        }
        
        // Calculate appropriate poll timeout
        int poll_timeout;
        if (!all_packets_sent && time_since_last_send < 1000) {
            // Wait until next send time
            poll_timeout = 1000 - time_since_last_send;
        } else if (state.sent_packets != NULL) {
            // Wait for remaining responses, but not too long
            poll_timeout = state.opts.timeout * 1000;
        } else {
            // No packets in flight and all sent
            break;
        }
        
        int poll_result = poll(fds, 2, poll_timeout);
        
        if (poll_result > 0) {
            for (int i = 0; i < 2; i++) {
                if (fds[i].revents & POLLIN) {
                    uint16_t received_sequence;
                    // Call receive_packet only ONCE per socket
                    if ((ret = receive_packet(&state, fds[i].fd, &received_sequence)) == 0) {
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
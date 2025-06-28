#include "../includes/ft_ping.h"

void print_stats(t_ping_state *state) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    
    double total_time = (end_time.tv_sec - state->stats.start_time.tv_sec) * 1000.0 + 
                       (end_time.tv_usec - state->stats.start_time.tv_usec) / 1000.0;
    
    // char addr_str[INET6_ADDRSTRLEN];
    // inet_ntop(state->conn.family, get_addr_ptr(state), state->conn.addr_str, INET6_ADDRSTRLEN);
    
    printf("\n--- %s ping statistics ---\n", state->conn.target);
    printf("%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms\n",
           state->stats.packets_sent, state->stats.packets_received,
           ((double)(state->stats.packets_sent - state->stats.packets_received) / 
            state->stats.packets_sent) * 100.0, total_time);
    
    if (state->stats.packets_received > 0) {
        state->stats.avg_rtt = state->stats.sum_rtt / state->stats.packets_received;
        printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", 
               state->stats.min_rtt, state->stats.avg_rtt, state->stats.max_rtt);
    }
}

int parseArgs(t_ping_state *state, int argc, char **argv) {
    int opt;
    
    state->opts.verbose = 0;
    state->opts.count = -1;    
    state->opts.psize = PING_PKT_S;
    state->opts.preload = 0;
    state->opts.timeout = 1;
    
    while ((opt = getopt(argc, argv, "vc:s:l:W:")) != -1) {
        switch (opt) {
            case 'v':
                state->opts.verbose = 1;
                break;
            case 'c':
                state->opts.count = atoi(optarg);
                if (state->opts.count <= 0) { // set a max value for count?
                    fprintf(stderr, "%s: bad number of packets to transmit\n", argv[0]);
                    return 1;
                }
                break;
			case 's':
				if ((state->opts.psize = atoi(optarg)) == 0) {
					state->opts.psize =  sizeof(struct icmphdr);
				}
				if (state->opts.psize > 65507) { // (t_size)state->opts.psize < 0 always false
					fprintf(stderr, "%s: illegal packet size: %zu\n", argv[0], state->opts.psize);
					return 1;
				}
				break;
            case 'l':
                state->opts.preload = atoi(optarg);
                if (state->opts.preload <= 0) {
                    fprintf(stderr, "%s: bad preload value\n", argv[0]);
                    return 1;
                }
                break;
            case 'W':
                state->opts.timeout = atoi(optarg);
                if (state->opts.timeout <= 0) {
                    fprintf(stderr, "%s: bad timeout value\n", argv[0]);
                    return 1;
                }
                break;
            case '?':
                return 1;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "%s: usage error: Destination address required\n", argv[0]);
        return 1;
    }
    
    state->conn.target = argv[optind];
    return 0;
}

int main(int argc, char **argv) {
	t_ping_state state;
	
	if (parseArgs(&state, argc, argv) || 
		resolveHost(&state, argv) || 
		createSocket(&state, argv)) {
		return 1;
	}

	setupSignals(&state);
	
	// Initialize stats
	memset(&state.stats, 0, sizeof(state.stats));
	gettimeofday(&state.stats.start_time, NULL);
	
    // char addr_str[INET6_ADDRSTRLEN];
    // inet_ntop(state.conn.family, get_addr_ptr(&state), addr_str, INET6_ADDRSTRLEN);
    printf("PING %s (%s) %zu(%zu) bytes of data.\n", 
           state.conn.target, state.conn.addr_str, 
           state.opts.psize - sizeof(struct icmphdr),  
           state.opts.psize + 20); // 84 total bytes (ICMP + IP)

	// Main ping loop
	uint16_t sequence = 1;
	while (state.opts.count == -1 || sequence <= state.opts.count) {
		// Create and send ping packet
		create_icmp_packet(&state, sequence);
		
		if (send_ping(&state)) {
			state.stats.packets_sent++;
			
			// Wait for reply or timeout
			if (receive_ping(&state, sequence)) {
				state.stats.packets_received++;
			}
		}
		
		sequence++;
		
		// Sleep 1 second between pings (unless preload)
		if (state.opts.preload == 0 || sequence > state.opts.preload) {
			sleep(1);
		}
	}
	
	print_stats(&state);
	close(state.conn.sockfd);
	return 0;
}
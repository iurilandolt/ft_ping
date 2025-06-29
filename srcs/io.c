#include "../includes/ft_ping.h"

static int parse_int_range(const char *str, const char *name, long min, long max, long *result) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    
    if (errno != 0 || endptr == str || *endptr != '\0' || val < min || val > max) {
        fprintf(stderr, "ft_ping: invalid %s: %s (must be %ld-%ld)\n", 
                name, str, min, max);
        return -1;
    }
    
    *result = val;
    return 0;
}


int parseArgs(t_ping_state *state, int argc, char **argv) {
    int opt;
    
    state->opts.verbose = 0;
    state->opts.count = -1;    
    state->opts.psize = PING_PKT_S;
    state->opts.preload = 0;
    state->opts.timeout = 4;
    
    while ((opt = getopt(argc, argv, "vc:s:l:W:")) != -1) {
        switch (opt) {
            case 'v':
                state->opts.verbose = 1;
                break;
			case 'c': {
				long count;
				if (parse_int_range(optarg, "count", 1, INT_MAX, &count) != 0) {
					return 1;
				}
				state->opts.count = count;
				break;
			}

			case 's': {
				long size;
				if (parse_int_range(optarg, "packet size", 0, 65507, &size) != 0) {
					return 1;
				}
				state->opts.psize = size; // + sizeof(struct icmphdr)
				break;
			}

			case 'l': {
				long preload;
				if (parse_int_range(optarg, "preload", 1, INT_MAX, &preload) != 0) {
					return 1;
				}
				state->opts.preload = preload;
				break;
			}

			case 'W': {
				long timeout; // what is the def state->opts.timeout ?
				if (parse_int_range(optarg, "timeout", 1, 2099999, &timeout) != 0) {
					return 1;
				}
				state->opts.timeout = timeout;
				break;
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

void print_stats(t_ping_state *state) {
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    
    double total_time = (end_time.tv_sec - state->stats.start_time.tv_sec) * 1000.0 + 
                       (end_time.tv_usec - state->stats.start_time.tv_usec) / 1000.0;
					   
	// if (state->stats.packets_sent > 0) {
	// 	total_time -= (state->stats.packets_sent - 1) * 1000.0;  // Remove 1s sleeps
    // }

    printf("\n--- %s ping statistics ---\n", state->conn.target);
    printf("%ld packets transmitted, %ld received, %.0f%% packet loss, time %.0fms\n",
           state->stats.packets_sent, state->stats.packets_received,
           ((double)(state->stats.packets_sent - state->stats.packets_received) / 
            state->stats.packets_sent) * 100.0, total_time);
    
    if (state->stats.packets_received > 0) {
        state->stats.avg_rtt = state->stats.sum_rtt / state->stats.packets_received;
		if (state->stats.avg_rtt > 0) {
			printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", 
				state->stats.min_rtt, state->stats.avg_rtt, state->stats.max_rtt); 
		}
    }
}


void print_verbose_info(t_ping_state *state) {
    if (!state->opts.verbose) {
        return;
    }
    
    int socktype;
    socklen_t optlen = sizeof(socktype);
    getsockopt(state->conn.sockfd, SOL_SOCKET, SO_TYPE, &socktype, &optlen);
    
    const char *socktype_str = (socktype == SOCK_RAW) ? "SOCK_RAW" : 
                              (socktype == SOCK_DGRAM) ? "SOCK_DGRAM" : "UNKNOWN";
    
    if (state->conn.family == AF_INET) {
        printf("ping: sock4.fd: %d (socktype: %s), sock6.fd: -1 (socktype: SOCK_RAW), hints.ai_family: AF_UNSPEC\n",
               state->conn.sockfd, socktype_str);
    } else {
        printf("ping: sock4.fd: -1 (socktype: SOCK_RAW), sock6.fd: %d (socktype: %s), hints.ai_family: AF_UNSPEC\n",
               state->conn.sockfd, socktype_str);
    }
    
    const char *family_str = (state->conn.family == AF_INET) ? "AF_INET" : "AF_INET6";
    printf("\nai->ai_family: %s, ai->ai_canonname: '%s'\n",
           family_str, state->conn.target);
}


void print_default_info(t_ping_state *state) {
	size_t icmp_header_size = (state->conn.family == AF_INET) ? 
							sizeof(struct icmphdr) : 
							sizeof(struct icmp6_hdr);
	size_t data_size = state->opts.psize - icmp_header_size;
	if (state->conn.family == AF_INET) {
		size_t total_with_ip = data_size + icmp_header_size + 20; // +20 for IPv4 header
		printf("PING %s (%s) %zu(%zu) bytes of data.\n", 
			state->conn.target, state->conn.addr_str, 
			data_size,        // 56
			total_with_ip);   // 84
	} else {
		printf("PING %s (%s) %zu data bytes\n", 
			state->conn.target, state->conn.addr_str, 
			data_size);       // 56
	}
}


void print_ping_reply(t_ping_state *state, size_t icmp_size, 
                            struct icmphdr *icmp_header, int ttl, double rtt) {
    uint16_t sequence = ntohs(icmp_header->un.echo.sequence);
    uint16_t id = ntohs(icmp_header->un.echo.id);
    
    if (rtt >= 0.0) {
        if (state->opts.verbose) {
            fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ident=%d ttl=%d time=%.3f ms\n",
                    icmp_size, state->conn.addr_str, sequence, id, ttl, rtt);
        } else {
            fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                    icmp_size, state->conn.addr_str, sequence, ttl, rtt);
        }
    } else {
        if (state->opts.verbose) {
            fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ident=%d ttl=%d\n",
                    icmp_size, state->conn.addr_str, sequence, id, ttl);
        } else {
            fprintf(stdout, "%zu bytes from %s: icmp_seq=%d ttl=%d\n",
                    icmp_size, state->conn.addr_str, sequence, ttl);
        }
    }
}
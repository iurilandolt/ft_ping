#include "../includes/ft_ping.h"

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
                if (state->opts.count <= 0) {
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

void *get_addr_ptr(t_ping_state *state) {
	return (state->conn.family == AF_INET) ? 
		(void*)&((struct sockaddr_in*)state->conn.addr)->sin_addr :
		(void*)&((struct sockaddr_in6*)state->conn.addr)->sin6_addr;
}

int resolveHost(t_ping_state *state, char **argv) {
	struct addrinfo hints, *result;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	
	if (getaddrinfo(state->conn.target, NULL, &hints, &result) != 0) {
		fprintf(stderr, "%s: %s: Name or service not known\n", 
				argv[0], state->conn.target);
		return 1;
	}
	
	if (result->ai_family == AF_INET) {
		memcpy(&state->conn.ipv4, result->ai_addr, sizeof(struct sockaddr_in));
		state->conn.addr = &state->conn.ipv4;
		state->conn.family = AF_INET;
		state->conn.protocol = IPPROTO_ICMP;
		state->conn.addr_len = sizeof(struct sockaddr_in);
	} else if (result->ai_family == AF_INET6) {
		memcpy(&state->conn.ipv6, result->ai_addr, sizeof(struct sockaddr_in6));
		state->conn.addr = &state->conn.ipv6;
		state->conn.family = AF_INET6;
		state->conn.protocol = IPPROTO_ICMPV6;
		state->conn.addr_len = sizeof(struct sockaddr_in6);
	}

	freeaddrinfo(result);
	
	if (state->opts.verbose) {
		char addr_str[INET6_ADDRSTRLEN];
		inet_ntop(state->conn.family, get_addr_ptr(state), addr_str, INET6_ADDRSTRLEN);
		printf("PING %s (%s): %lu data bytes\n", 
			state->conn.target, addr_str, 
			state->opts.psize - sizeof(struct icmphdr));
	}
	
	return 0;
}

int createSocket(t_ping_state *state, char **argv) {
	state->conn.sockfd = socket(state->conn.family, SOCK_RAW, state->conn.protocol);
	
	if (state->conn.sockfd < 0) {
		fprintf(stderr, "%s: %s: Cannot create socket\n", argv[0], state->conn.target);
		return 1;
	}

	
	// TODO: Set socket options (TTL, timeout, etc.)
	
	return 0;
}


int main(int argc, char **argv) {
	t_ping_state state;
	
	if (parseArgs(&state, argc, argv) || resolveHost(&state, argv)) {
		return 1;
	}

	if (createSocket(&state, argv)) {
		return 1;
	}
	
	setupSignals();
	while (1) {
		;
	}


	return(0);
}
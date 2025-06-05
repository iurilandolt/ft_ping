#include "../includes/ft_ping.h"


// getaddrinfo /  gethostbyname(3) / getservbyname(3)

int parseArgs(t_ping_state *state, int argc, char **argv) {
    int opt;
    
    // Initialize defaults
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
				state->opts.psize = atoi(optarg);
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
                fprintf(stderr, "%s: invalid option\n", argv[0]);
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
	
	if (parseArgs(&state, argc, argv)) {
		return 1;
	}



	setupSignals();
	while (1) {
		;
	}


	return(0);
}
#include "../includes/ft_ping.h"

/**
 * @param str - string to parse
 * @param name - parameter name for error messages
 * @param min - minimum allowed value
 * @param max - maximum allowed value
 * @param result - pointer to store parsed value
 * @return 0 on success, -1 on failure
 * 
 * Parses string as integer and validates it's within specified range
 */
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

/**
 * @param state - ping state to populate with parsed options
 * @param argc - argument count
 * @param argv - argument vector
 * @return 0 on success, 1 on failure
 * 
 * Parses command line arguments and populates ping options and target
 */
int parseArgs(t_ping_state *state, int argc, char **argv) {
	int opt;
	
	state->opts.verbose = 0;
	state->opts.count = -1;    
	state->opts.psize = PING_PKT_S;
	state->opts.preload = 0;
	state->opts.timeout = 4;
	state->opts.ttl = 64;

	while ((opt = getopt(argc, argv, "vhc:s:l:W:t:")) != -1) {
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
				state->opts.psize = size;
				break;
			}

			case 'l': {
				long preload;
				if (parse_int_range(optarg, "preload", 1, 3, &preload) != 0) {
					return 1;
				}
				state->opts.preload = preload;
				break;
			}

			case 'W': {
				long timeout;
				if (parse_int_range(optarg, "timeout", 1, 2099999, &timeout) != 0) {
					return 1;
				}
				state->opts.timeout = timeout;
				break;
			}
			case 't': {
				long ttl;
				if (parse_int_range(optarg, "ttl", 1, 255, &ttl) != 0) {
					return 1;
				}
				state->opts.ttl = ttl;
				break;
			}
			case 'h': {
				print_usage(argv[0], optopt);
				exit(0);
			}
			case '?':
				//fprintf(stderr, "%s: usage error: Unknown option '-%c'\n", argv[0], optopt);
				print_usage(argv[0], optopt);
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

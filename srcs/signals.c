#include "../includes/ft_ping.h"

static t_ping_state *state_ptr = NULL; 

static void setup_alarm(t_ping_state *state) {
	if (state->opts.count == -1) {
		// Infinite ping - no alarm
		return;
	}
	
	// Calculate total expected runtime
	// System ping waits: sending_time + timeout for last packet
	int remaining_packets = state->opts.count - state->opts.preload;
	if (remaining_packets <= 0) {
		remaining_packets = 0;
	}
	
	// Total time = time to send all packets + timeout for the last packet
	unsigned int total_seconds = remaining_packets + state->opts.timeout;
	
	// Set the alarm
	alarm(total_seconds);
}

void setupSignals(t_ping_state *state) { 
	static struct sigaction handler;
	state_ptr = state; 
	handler.sa_sigaction = handleSignals;
	sigemptyset(&handler.sa_mask);
	handler.sa_flags = 0;
	sigaction(SIGINT, &handler, NULL);
	sigaction(SIGTERM, &handler, NULL);
	sigaction(SIGQUIT, &handler, NULL);
	sigaction(SIGALRM, &handler, NULL);

	static struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	sigaction(SIGPIPE, &ignore, NULL);
	sigaction(SIGCHLD, &ignore, NULL);
	sigaction(SIGTSTP, &ignore, NULL);
	setup_alarm(state);
}

void handleSignals(int signum, siginfo_t *info, void *ptr) {
	(void)info;
	(void)ptr;
	
	if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT) {
		printf("\nReceived signal %d, exiting...\n", signum);

		cleanup_packets(state_ptr);
		close(state_ptr->conn.ipv4.sockfd);
		close(state_ptr->conn.ipv6.sockfd);

		exit(0); 
	} else if (signum == SIGALRM) {
		// Alarm fired - time to exit gracefully
		print_stats(state_ptr);
		
		cleanup_packets(state_ptr);
		close(state_ptr->conn.ipv4.sockfd);
		close(state_ptr->conn.ipv6.sockfd);

		exit((state_ptr->stats.packets_received == 0) ? 1 : 0);
	}
}



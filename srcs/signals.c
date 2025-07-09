#include "../includes/ft_ping.h"

static t_ping_state *state_ptr = NULL; 

/**
 * @param state - ping state containing count and timeout options
 * 
 * Sets up alarm for finite ping operations based on expected runtime
 */
static void setup_alarm(t_ping_state *state) {
	if (state->opts.count == -1) {
		return;
	}
	
	int remaining_packets = state->opts.count - state->opts.preload;
	if (remaining_packets <= 0) {
		remaining_packets = 0;
	}
	
	unsigned int total_seconds = remaining_packets + state->opts.timeout;
	alarm(total_seconds);
}

/**
 * @param state - ping state to store globally for signal handlers
 * 
 * Configures signal handlers for graceful termination and alarm timeout
 */
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

/**
 * @param signum - signal number received
 * @param info - signal info (unused)
 * @param ptr - signal context (unused)
 * 
 * Handles termination signals and alarm timeout with proper cleanup
 */
void handleSignals(int signum, siginfo_t *info, void *ptr) {	(void)info;
	(void)ptr;
	
	if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT) {
		printf("\nReceived signal %d, exiting...\n", signum);
		cleanup_packets(state_ptr);
		close(state_ptr->conn.ipv4.sockfd);
		close(state_ptr->conn.ipv6.sockfd);
		exit(0); 
	} else if (signum == SIGALRM) {
		print_stats(state_ptr);
		cleanup_packets(state_ptr);
		close(state_ptr->conn.ipv4.sockfd);
		close(state_ptr->conn.ipv6.sockfd);
		exit((state_ptr->stats.packets_received == 0) ? 1 : 0);
	}
}
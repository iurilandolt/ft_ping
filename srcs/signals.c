#include "../includes/ft_ping.h"

static t_ping_state *state_ptr = NULL; 

void handleSignals(int signum, siginfo_t *info, void *ptr) {
	if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT) {
		printf("\nReceived signal %d, exiting...\n", signum);
		close(state_ptr->conn.sockfd);
		print_stats(state_ptr);
		free_packet(state_ptr); 
		state_ptr = NULL; 
		exit(0); 
	}
	(void)info;
	(void)ptr;
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

	static struct sigaction ignore;
	ignore.sa_handler = SIG_IGN;
	sigemptyset(&ignore.sa_mask);
	sigaction(SIGPIPE, &ignore, NULL);
	sigaction(SIGCHLD, &ignore, NULL);
	sigaction(SIGTSTP, &ignore, NULL);
}
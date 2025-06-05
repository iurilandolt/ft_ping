#include "../includes/ft_ping.h"

void handleSignals(int signum, siginfo_t *info, void *ptr) {
	if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT) {
		printf("\nReceived signal %d, exiting...\n", signum); // print exit stats
		exit(0); // use ptr to access ping_sate struct and clean it
	}
	(void)info;
	(void)ptr;
}

void setupSignals(void) { // add ptr to ping_sate struct
	static struct sigaction handler;
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
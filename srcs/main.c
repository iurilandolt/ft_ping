#include "../includes/ft_ping.h"

void handleSignals(int signum, siginfo_t *info, void *ptr) {
	if (signum == SIGINT || signum == SIGTERM || signum == SIGQUIT) {
		printf("\nReceived signal %d, exiting...\n", signum);
		exit(0);
	}
	(void)info;
	(void)ptr;
}

void setupSignals(void) {
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

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	write(1, "ft_ping\n", 8);
	setupSignals();
	while (1) {
		;
	}
	return(0);
}
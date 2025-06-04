#include "../includes/ft_ping.h"

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	int r_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (r_sockfd < 0) {
		perror("raw socket");
		return 1;
	}


	write(1, "ft_ping\n", 8);
	setupSignals();

	while (1) {
		;
	}

	close(r_sockfd);
	return(0);
}
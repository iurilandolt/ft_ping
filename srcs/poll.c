#include "../includes/ft_ping.h"

/**
 * @param start - start time reference
 * @param end - end time reference
 * @return time difference in milliseconds, 0 if start time not set
 * 
 * Calculates millisecond difference between two timeval structures
 */
long timeval_diff_ms(struct timeval *start, struct timeval *end) {
	if (start->tv_sec == 0) {
		return 0;
	}
	return (end->tv_sec - start->tv_sec) * 1000 + 
		   (end->tv_usec - start->tv_usec) / 1000;
}

/**
 * @param state - ping state containing socket file descriptors
 * @param fds - poll file descriptor array to configure
 * @return target socket file descriptor for the resolved address family
 * 
 * Configures poll file descriptors for both IPv4 and IPv6 sockets
 */
int setupPoll(t_ping_state *state, struct pollfd *fds) {
	int sockets[] = {state->conn.ipv4.sockfd, state->conn.ipv6.sockfd};
	
	for (int i = 0; i < 2; i++) {
		fds[i].fd = sockets[i];
		fds[i].events = POLLIN;
		fds[i].revents = 0;
	}
	return (state->conn.target_family == AF_INET) ? 
			state->conn.ipv4.sockfd : 
			state->conn.ipv6.sockfd;
}

/**
 * @param state - ping state containing timing and completion info
 * @return timeout value in milliseconds for poll operation
 * 
 * Calculates appropriate timeout for poll based on send timing and completion status
 */
int get_next_poll_timeout(t_ping_state *state) {
	struct timeval now;
	gettimeofday(&now, NULL);
	
	if (!state->stats.transmission_complete && state->stats.preload_sent >= state->opts.preload) {
		if (state->stats.last_packet_time.tv_sec != 0) {
			long since_last = timeval_diff_ms(&state->stats.last_packet_time, &now);
			int until_next_send = 1000 - since_last;
			return (until_next_send > 0) ? until_next_send : 0;
		}
		return 0;
	}
	
	if (state->stats.transmission_complete) {
		return 100;
	}
	
	return 0;
}

/**
 * @param state - ping state containing sent packets and timeout settings
 * 
 * Removes packets from sent list that have exceeded the timeout period
 */
void handle_timeouts(t_ping_state *state) {
	struct timeval now;
	gettimeofday(&now, NULL);
	
	t_packet_entry *current = state->sent_packets;
	t_packet_entry *prev = NULL;
	
	while (current != NULL) {
		long elapsed = timeval_diff_ms(&current->send_time, &now);
		
		if (elapsed >= state->opts.timeout * 1000) {
			if (prev == NULL) {
				state->sent_packets = current->next;
			} else {
				prev->next = current->next;
			}
			
			t_packet_entry *to_free = current;
			current = current->next;
			free(to_free->packet);
			free(to_free);
		} else {
			prev = current;
			current = current->next;
		}
	}
}
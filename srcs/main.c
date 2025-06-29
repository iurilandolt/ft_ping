#include "../includes/ft_ping.h"

int main(int argc, char **argv) {
	t_ping_state state;
	int ret = 0;
	
	if (
		parseArgs(&state, argc, argv) ||
		resolveHost(&state, argv) || 
		createSocket(&state, argv) ||
		allocate_packet(&state)) {
		return ret = 1;
	}

	setupSignals(&state);
	
	memset(&state.stats, 0, sizeof(state.stats));
	gettimeofday(&state.stats.start_time, NULL);
	
	print_verbose_info(&state);
	print_default_info(&state);
	
	uint16_t sequence = 1;
	int preload_sent = 0;
	struct timeval last_send = {0, 0};
	
	while (state.opts.count == -1 || sequence <= state.opts.count) {
		struct timeval now;
		gettimeofday(&now, NULL);
		
		if (preload_sent < state.opts.preload && sequence <= (state.opts.count == -1 ? INT_MAX : state.opts.count)) {
			create_icmp_packet(&state, sequence);
			if ((ret = send_ping(&state, sequence)) == 0) {
				state.stats.packets_sent++;
				preload_sent++;
				sequence++;
			}
		}

		else if (sequence <= (state.opts.count == -1 ? INT_MAX : state.opts.count) && 
				(last_send.tv_sec == 0 || (now.tv_sec - last_send.tv_sec >= 1))) {
			create_icmp_packet(&state, sequence);
			if ((ret = send_ping(&state, sequence)) == 0) {
				state.stats.packets_sent++;
				last_send = now;
				sequence++;
			}
		}
		
		struct pollfd pfd = {state.conn.sockfd, POLLIN, 0};
		int poll_result = poll(&pfd, 1, state.opts.timeout * 1000);
		
		if (poll_result > 0 && (pfd.revents & POLLIN)) {
			t_packet_entry *current = state.sent_packets;
			while (current) {
				uint16_t seq_to_check = current->sequence;
				current = current->next;
				
				if ((ret = receive_ping(&state, seq_to_check)) == 0) {
					state.stats.packets_received++;
					remove_sent_packet(&state, seq_to_check);
					break;
				}
			}
		} else if (poll_result < 0 && errno != EINTR) {
			fprintf(stderr, "poll: %s\n", strerror(errno));
			break;
		}
	}
	
	print_stats(&state);
	free_packet(&state);
	
	close(state.conn.sockfd);
	return ret;
}
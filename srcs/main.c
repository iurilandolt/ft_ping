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
	while (state.opts.count == -1 || sequence <= state.opts.count) {
		create_icmp_packet(&state, sequence);
		if ((ret = send_ping(&state)) == 0) {
			state.stats.packets_sent++;
			if ((ret = receive_ping(&state, sequence)) == 0) {
				state.stats.packets_received++;
			}
		}
		sequence++;
		// Sleep 1 second between pings (unless preload)
		if ((state.opts.count == -1 || sequence <= state.opts.count) && 
			(state.opts.preload == 0 || sequence > state.opts.preload)) {
			sleep(1);
		}
	}
	
	print_stats(&state);
	free_packet(&state);
	
	close(state.conn.sockfd);
	return ret;
}
#include "../includes/ft_ping.h"

int main(int argc, char **argv) {
	t_ping_state state;
	
	if (
		parseArgs(&state, argc, argv) ||
		resolveHost(&state, argv) || 
		createSocket(&state, argv) ||
		allocate_packet(&state)) {
		return 1;
	}

	setupSignals(&state);
	
	memset(&state.stats, 0, sizeof(state.stats));
	gettimeofday(&state.stats.start_time, NULL);
	
	print_verbose_info(&state);
	
    printf("PING %s (%s) %zu(%zu) bytes of data.\n", 
           state.conn.target, state.conn.addr_str, 
           state.opts.psize - sizeof(struct icmphdr), //  
           state.opts.psize + 20); // 84 total bytes (ICMP + IP)

	// Main ping loop
	uint16_t sequence = 1;
	while (state.opts.count == -1 || sequence <= state.opts.count) {
		// Create and send ping packet
		create_icmp_packet(&state, sequence);
		
		if (send_ping(&state)) {
			state.stats.packets_sent++;
			
			// Wait for reply or timeout
			if (receive_ping(&state, sequence)) {
				state.stats.packets_received++;
			}
		}
		
		sequence++;
		
		// Sleep 1 second between pings (unless preload)
		if (state.opts.preload == 0 || sequence > state.opts.preload) {
			sleep(1);
		}
	}
	
	print_stats(&state);
	free_packet(&state);
	
	close(state.conn.sockfd);
	return 0;
}
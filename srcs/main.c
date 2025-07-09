#include "../includes/ft_ping.h"

static void ready(t_ping_state *state) {
    setupSignals(state);
    memset(&state->stats, 0, sizeof(state->stats));
    print_verbose_info(state);
    print_default_info(state);
}


static void end(t_ping_state *state) {
    print_stats(state);
    cleanup_packets(state);
    close(state->conn.ipv4.sockfd);
    close(state->conn.ipv6.sockfd);
}

int main(int argc, char **argv) {
    t_ping_state state;
    struct pollfd fds[2];
    uint16_t sequence = 1;
    int ret = 0;

    if (parseArgs(&state, argc, argv) ||
        resolveHost(&state, argv) || 
        createSocket(&state, argv) ||
        init_packet_system(&state)) {
        return ret = 1;
    }

	ready(&state);

    int target_sockfd = setupPoll(&state, fds);
    
    while (!state.stats.transmission_complete || state.sent_packets != NULL) {
        ret = send_ping(&state, &sequence, target_sockfd);
        int poll_timeout = get_next_poll_timeout(&state);
        if (poll_timeout < 0) {
            break;
        }
        
        int poll_result = poll(fds, 2, poll_timeout);
        if (poll_result > 0) {
            for (int i = 0; i < 2; i++) {
                if (fds[i].revents & POLLIN) {
                    if ((ret = receive_packet(&state, fds[i].fd)) == 0) {
                        state.stats.packets_received++;
                    }
                    break;
                }
            }
        } else if (poll_result == 0) {
            handle_timeouts(&state);
        } else if (poll_result < 0 && errno != EINTR) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            break;
        }
    }
	end(&state);
    return (state.stats.packets_received == 0) ? 1 : ret;
}
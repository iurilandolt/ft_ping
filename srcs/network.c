#include "../includes/ft_ping.h"


void *get_addr_ptr(t_ping_state *state) {
	return (state->conn.family == AF_INET) ? 
		(void*)&((struct sockaddr_in*)state->conn.addr)->sin_addr :
		(void*)&((struct sockaddr_in6*)state->conn.addr)->sin6_addr;
}


int resolveHost(t_ping_state *state, char **argv) {
	struct addrinfo hints, *result;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	// hints.ai_protocol = IPPROTO_ICMP;
	if (getaddrinfo(state->conn.target, NULL, &hints, &result) != 0) {
		fprintf(stderr, "%s: %s: Name or service not known\n", 
				argv[0], state->conn.target);
		return 1;
	}
	if (result->ai_family == AF_INET) {
		memcpy(&state->conn.ipv4, result->ai_addr, sizeof(struct sockaddr_in));
		state->conn.addr = &state->conn.ipv4;
		state->conn.family = AF_INET;
		// state->conn.protocol = IPPROTO_ICMP;
		state->conn.addr_len = sizeof(struct sockaddr_in);
	} else if (result->ai_family == AF_INET6) {
		memcpy(&state->conn.ipv6, result->ai_addr, sizeof(struct sockaddr_in6));
		state->conn.addr = &state->conn.ipv6;
		state->conn.family = AF_INET6;
		// state->conn.protocol = IPPROTO_ICMPV6;
		state->conn.addr_len = sizeof(struct sockaddr_in6);
	}
	inet_ntop(state->conn.family, get_addr_ptr(state), state->conn.addr_str, INET6_ADDRSTRLEN);
	freeaddrinfo(result);
	return 0;
}


int createSocket(t_ping_state *state, char **argv) {
	state->conn.protocol = (state->conn.family == AF_INET) ? IPPROTO_ICMP : IPPROTO_ICMPV6;
	state->conn.sockfd = socket(state->conn.family, SOCK_RAW, state->conn.protocol);
	
	if (state->conn.sockfd < 0) {
		fprintf(stderr, "%s: %s: Cannot create socket\n", argv[0], state->conn.target);
		return 1;
	}
	struct timeval timeout = {state->opts.timeout, 0}; // why 0 seconds?
	if (setsockopt(state->conn.sockfd, SOL_SOCKET, SO_RCVTIMEO, 
				&timeout, sizeof(timeout)) < 0) {
		fprintf(stderr, "setsockopt: %s\n", strerror(errno));
		return 1;
	}
	return 0;
}


int send_ping(t_ping_state *state) {	
	ssize_t bytes_sent = sendto(state->conn.sockfd, 
							   state->packet, 
							   state->opts.psize, 
							   0,
							   (struct sockaddr*)state->conn.addr, 
							   state->conn.addr_len);
	if (bytes_sent < 0) {
		fprintf(stderr, "sendto: %s\n", strerror(errno));
		return 0;
	}
	if ((size_t)bytes_sent != state->opts.psize) {
		fprintf(stderr, "sendto: partial packet sent (%zd of %zu bytes)\n", 
				bytes_sent, state->opts.psize);
		return 0;
	}
	return 1;
}

int receive_ping(t_ping_state *state, uint16_t expected_sequence) {
    char buffer[1024];
    struct sockaddr_storage from;
    socklen_t fromlen = sizeof(from);

    while (1) {
        ssize_t bytes_received = recvfrom(state->conn.sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&from, &fromlen);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "Timeout waiting for reply\n");
                return 0;
            } else {
                fprintf(stderr, "recvfrom: %s\n", strerror(errno));
                return 0;
            }
        }
        
        if (parse_icmp_reply(buffer, bytes_received, expected_sequence, state) == 0) {
            return 1;
        }
    }
}
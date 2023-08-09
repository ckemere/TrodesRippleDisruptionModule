#define _GNU_SOURCE // for recvmmsg

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#include <fcntl.h>
#include <signal.h>


#define BUFFERLEN 128

struct net_addr
{
	int ipver;
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sockaddr *sockaddr;
	int sockaddr_len;
};


int net_gethostbyname(struct net_addr *shost, const char *host, int port)
{
	memset(shost, 0, sizeof(struct net_addr));

	struct in_addr in_addr;
	struct in6_addr in6_addr;

	/* Try ipv4 address first */
	if (inet_pton(AF_INET, host, &in_addr) == 1) {
		goto got_ipv4;
	}

	// /* Then ipv6 */
	// if (inet_pton(AF_INET6, host, &in6_addr) == 1) {
	// 	goto got_ipv6;
	// }

    fprintf(stderr, "Failure in inet_pton\n");
	return -1;

got_ipv4:
	shost->ipver = 4;
	shost->sockaddr = (struct sockaddr*)&shost->sin4;
	shost->sockaddr_len = sizeof(shost->sin4);
	shost->sin4.sin_family = AF_INET;
	shost->sin4.sin_port = htons(port);
	shost->sin4.sin_addr = in_addr;
	return 0;

got_ipv6:
	shost->ipver = 6;
	shost->sockaddr = (struct sockaddr*)&shost->sin6;
	shost->sockaddr_len = sizeof(shost->sin4);
	shost->sin6.sin6_family = AF_INET6;
	shost->sin6.sin6_port = htons(port);
	shost->sin6.sin6_addr = in6_addr;
	return 0;
}



int main()
{
    const char *listen_host = "0.0.0.0";
    int port = 20782;

	char buffer[BUFFERLEN];

	struct net_addr listen_addr;
    struct sockaddr_in cliaddr;

    char *hello = "Hello from server";

    const char simple_ack[] = {'A'};

	int len = sizeof(cliaddr); //len is value/result

    int ret = net_gethostbyname(&listen_addr, listen_host, port);
    if (ret < 0)
        return(-1);

    fprintf(stderr, "[*] Starting udpreceiver on 0.0.0.0:20782\n");

    // Bind UDP
	int main_fd = -1;
	main_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (main_fd < 0) {
        fprintf(stderr, "Error in socket.\n");
        return(-1);
	}
    if (bind(main_fd, listen_addr.sockaddr, listen_addr.sockaddr_len) < 0) {
        fprintf(stderr, "Error in bind.\n");
        return(-1);
	}

	while (1) {
		/* Blocking recv. */
        int r = recvfrom(main_fd, (char *)buffer, BUFFERLEN, 0, ( struct sockaddr *) &cliaddr, &len);
        printf("Received %d, First byte from client: 0x%02x\n", r, buffer[0]);
		if (r <= 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				continue;
			}
            fprintf(stderr, "Error in recvfrom.\n");
            return(-1);
		}

        // PROCESS MESSAGES
        switch (buffer[0]) {
            case 'C':
            	fprintf(stderr, "Status check\n");
                sendto(main_fd, simple_ack, 1, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
                break;
            default:
                fprintf(stderr, "Non-handled command, 0x%02x\n", buffer[0]);
                break;
        }


	}
}

/*
 * client.c
 *
 *  Created on: Aug 11, 2017
 *      Author: mayfa
 */

#include <err.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>

#include "tftp.h"

#define BUF_LEN 256

char *host = "localhost";

void
generic_client()
{
	int e, sockfd, filefd, n;
	socklen_t client_addr_len;
	char service[PORT_LEN] = "4567";
	char* pathname = "/home/mayfa/file.bin";
	struct addrinfo *res, hints;
	struct protoent *udp_protocol;
	tftp_header_t hdr;
	uint16_t buf = htons(1);

	udp_protocol = getprotobyname("udp");

	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = udp_protocol->p_proto;
	hints.ai_flags = AI_NUMERICSERV;

	/* getaddrinfo call that is suitable for binding and then accepting. */
	if ((e = getaddrinfo(NULL, service, &hints, &res)) != 0) {
		err(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(e));
	}

	/* Reroll addr - server uses IPv4. */
	res = res->ai_next;

	/* Create socket. */
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		err(EXIT_FAILURE, "socket");

	/* Connect. */
	if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0)
		err(EXIT_FAILURE, "connect");

	if (send(sockfd, &buf, 2, 0) == -1)
		error(EXIT_FAILURE, errno, "%s", "send");

	freeaddrinfo(res);
}

int
main()
{
	generic_client();
}

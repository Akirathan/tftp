/*
 * main.c
 *
 *  Created on: Jul 22, 2017
 *      Author: mayfa
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include "tftp.h"

#define BUF_LEN	256

char *host = "127.0.0.1";
char *directory_path = "/home/mayfa/tftp_server";

char filename[FILENAME_LEN];
char modename[MODENAME_LEN];

void
usage(char *program)
{
	fprintf(stderr, "Usage: %s [-h host] [-p port] directory", program);
	exit(1);
}

int
buffer_len(tftp_header_t *hdr)
{

}

/**
 * Convert hdr to byte buffer - should be compared with packet sent by orig
 * client. Suppose buf has correct length.
 */
void
copy_to_buffer(tftp_header_t *hdr, uint8_t *buf)
{
	uint8_t *buf_idx = buf;

	uint16_t opcode = htons(hdr->opcode);
	/* Save opcode and "shift" two bytes. */
	*((uint16_t *)buf_idx) = opcode;
	buf_idx += 2;

	switch (hdr->opcode) {
	case RRQ_OPCODE:
		/* Filename. */
		strcpy(buf_idx, hdr->req_filename);
		buf_idx += strlen(hdr->req_filename);
		*buf_idx = '\0';
		buf_idx++;
		/* Modename. */
		strcpy(buf_idx, hdr->req_mode);
		buf_idx += strlen(hdr->req_mode);
		*buf_idx = '\0';
		break;
	}
}

/**
 * Fills in the tftp_header_t struct
 */
void
read_packet(const uint8_t *packet, int packet_len, tftp_header_t *hdr)
{
	int i = 2, file_idx = 0, mode_idx = 0;

	/* First two bytes are opcode. */
	if (packet[0] != 0) {
		hdr->opcode = ERR_OPCODE;
	}
	else {
		hdr->opcode = packet[1];
	}

	switch (hdr->opcode) {
	case RRQ_OPCODE:
	case WRQ_OPCODE:
		/* Extract filename. */
		while (packet[i] != '\0') {
			filename[file_idx++] = packet[i];
			++i;
		}
		hdr->req_filename = filename;

		++i;

		/* Extract modename. */
		while (packet[i] != '\0') {
			modename[mode_idx++] = packet[i];
			++i;
		}
		hdr->req_mode = modename;

		break;
	}
}

/**
 * Returns privileged port for tftp protocol if this process is privileged,
 * and non-privileged port if this process is not privileged.
 */
void
resolve_service_by_privileges(char *service)
{
	/* Investigate privileges. */
	// ...

	/* Return port number. */
	// ...
	snprintf(service, PORT_LEN, "%d", NON_PRIVILEGED_PORT);
}

/**
 * Creates generic server that binds to IPv4 and IPv6.
 */
void
generic_server()
{
	int error, sockfd, n;
	uint8_t buff[BUF_LEN];
	char service[PORT_LEN];
	struct sockaddr client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	struct addrinfo *res, hints;
	struct protoent *udp_protocol;
	tftp_header_t hdr;

	udp_protocol = getprotobyname("udp");

	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = udp_protocol->p_proto;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

	resolve_service_by_privileges(service);

	/* getaddrinfo call that is suitable for binding and then accepting. */
	if ((error = getaddrinfo(NULL, service, &hints, &res)) != 0) {
		err(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(error));
	}

	/* Create socket. */
	if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		err(EXIT_FAILURE, "socket");

	/* Bind the first address returned by getaddrinfo. */
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
		err(EXIT_FAILURE, "bind");

	/* Receive socket from client. */
	while ((n = recvfrom(sockfd, buff, BUF_LEN, 0, &client_addr, &client_addr_len)) != 0) {
		/* Check for an error. */
		if (n == -1)
			err(EXIT_FAILURE, "recvfrom");
		/* Process received socket. */
		read_packet(buff, n, &hdr);
		switch (hdr.opcode) {
		case RRQ_OPCODE:
			break;
		}
	}

	freeaddrinfo(res);
}

void
ipv4_server()
{
	int sock_fd, n;
	uint8_t buf[BUF_LEN];
	struct sockaddr_in sockaddr, client_addr;
	socklen_t client_addr_size = sizeof (client_addr);
	tftp_header_t hdr;

	if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		err(1, "socket initialization failed.");

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(NON_PRIVILEGED_PORT);
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock_fd, (struct sockaddr *) &sockaddr, sizeof (sockaddr)) == -1)
		err(1, "bind");

	n = recvfrom(sock_fd, buf, BUF_LEN, 0, (struct sockaddr *) &client_addr,
			&client_addr_size);


	read_packet(buf, n, &hdr);
}

int
main()
{
	//generic_server();
	tftp_header_t hdr;
	hdr.opcode = RRQ_OPCODE;
	hdr.req_filename = "hi";
	hdr.req_mode = NETASCII_MODE;

	//print_to_file(&hdr);
	generic_server();
}

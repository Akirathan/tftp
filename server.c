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
#include <unistd.h>
#include "tftp.h"

#define BUF_LEN	256

char *directory_path = "/home/mayfa/tftp_server";

int first_received = 1;
tftp_mode_t mode;
int client_sock;
struct sockaddr client_addr;
socklen_t client_addr_len = sizeof(client_addr);

const char *const err_msgs[] = {
		"Undefined",
		"File not found",
		"Access violation",
		"Disk full or allocation exceeded",
		"Illegal TFTP operation",
		"Unknown transfer ID",
		"File already exists",
		"No such user"
};

void
usage(char *program)
{
	fprintf(stderr, "Usage: %s [-h host] [-p port] directory", program);
	exit(1);
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
 * Generates random port.
 */
void
random_service(char *service)
{

}

/**
 * Converts header to buffer and sends it to the client.
 */
void
send_hdr(tftp_header_t *hdr)
{
	size_t buf_len = header_len(hdr);
	uint8_t buf[buf_len];

	copy_to_buffer(buf, hdr);

	if (sendto(client_sock, buf, buf_len, 0, &client_addr, client_addr_len) == -1)
		err(EXIT_FAILURE, "sendto");
}

/**
 * Client sent RRQ, server should now send first data packet.
 */
void
read_file(const char *filename)
{
	uint16_t blocknum = 1;
	tftp_header_t hdr;
	FILE *file;
	uint8_t buf[DATA_LEN], recv_buf[DATA_LEN], n = DATA_LEN;
	int received;

	if ((file = fopen(filename, "r")) == NULL)
		err(EXIT_FAILURE, "fopen");


	while (n > 0) {
		if ((n = read(fileno(file), buf, DATA_LEN)) == -1)
			err(EXIT_FAILURE, "read");
		/* Fill and send header. */
		hdr.data_blocknum = blocknum;
		hdr.data_data = buf;
		send_hdr(&hdr);
		/* Wait for ACK. */
		// TODO Start clock
		if ((received = recvfrom(client_sock, recv_buf, BUF_LEN, 0, &client_addr,
				&client_addr_len)) == -1)
			err(EXIT_FAILURE, "recvfrom");

		read_packet(&hdr, recv_buf, received);
		/* Check if received packet is of ACK type. */
		if (hdr.opcode == ACK_OPCODE) {
			if (hdr.ack_blocknum == blocknum) {
				/* ACK received. */
				blocknum++;
			}
		}
		else {

		}
	}
}

/**
 * Creates generic server that binds to IPv4 and IPv6.
 */
void
generic_server()
{
	int error, n;
	uint8_t buff[BUF_LEN];
	char service[PORT_LEN];
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
	if ((client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		err(EXIT_FAILURE, "socket");

	/* Bind the first address returned by getaddrinfo. */
	if (bind(client_sock, res->ai_addr, res->ai_addrlen) == -1)
		err(EXIT_FAILURE, "bind");

	/* Receive socket from client. */
	while ((n = recvfrom(client_sock, buff, BUF_LEN, 0, &client_addr,
			&client_addr_len)) != 0) {

		/* Rebind client_sock to random port if necessary. */
		if (first_received) {
			close(client_sock);
			random_service(service);
			if ((error = getaddrinfo(NULL, service, &hints, &res)) != 0)
				err(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(error));

			if ((client_sock = socket(res->ai_family, res->ai_socktype,
					res->ai_protocol)) == -1) {
				err(EXIT_FAILURE, "socket");
			}
			if (bind(client_sock, res->ai_addr, res->ai_addrlen) == -1)
				err(EXIT_FAILURE, "bind");

			first_received = 0;
		}

		/* Check for an error. */
		if (n == -1)
			err(EXIT_FAILURE, "recvfrom");

		read_packet(&hdr, buff, n);
		switch (hdr.opcode) {
		case RRQ_OPCODE:
			mode = hdr.req_mode;
			read_file(hdr.req_filename);
			break;
		}
	}

	freeaddrinfo(res);
}

int
main()
{
	generic_server();
}

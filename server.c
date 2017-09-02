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

char *dirpath = "/home/mayfa/tftp_server/";
char filepath[FILEPATH_LEN];

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
	service = "4568";
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

void
receive_hdr(tftp_header_t *hdr)
{
	int n = 0;
	uint8_t buf[BUF_LEN];

	/* Receive packet */
	if ((n= recvfrom(client_sock, buf, BUF_LEN, 0, &client_addr,
			&client_addr_len)) == -1)
		err(EXIT_FAILURE, "recvfrom");

	/* Convert packet to tftp_header_t */
	read_packet(&hdr, buf, n);
}

/**
 * Concatenates directory path with filename. The size of result is assigned
 * to size parameter.
 */
char *
concat_paths(const char *dirpath, const char *fname, size_t *size)
{
	size = strlen(dirpath) + strlen(fname);

	/* Build the string */
	bzero(filepath, FILEPATH_LEN);
	strcat(filepath, dirpath);
	strcat(filepath, fname);

	return filepath;
}

/**
 * Client writes to file.
 */
void
write_file(const char *fname)
{
	tftp_header_t hdr;
	FILE *file;
	char *fpath;
	size_t fpath_len;

	/* Check if files can be created */
	// ...

	fpath = concat_paths(dirpath, fname, &fpath_len);
	if ((file = fopen(fpath, "w+")) == NULL)
		err(EXIT_FAILURE, "fopen");

	/* Send ACK */
	hdr.opcode = OPCODE_ACK;
	hdr.ack_blocknum = 0;
	send_hdr(&hdr);

	while (1) {

	}
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
	uint8_t buf[DATA_LEN], recv_buf[DATA_LEN];
	size_t filepath_len = strlen(dirpath) + strlen(filename);
	ssize_t n = DATA_LEN;
	int received;
	char filepath[filepath_len];

	/* Initiate and build filepath. */
	bzero(filepath, filepath_len);
	strcat(filepath, dirpath);
	strcat(filepath, filename);
	if ((file = fopen(filepath, "r")) == NULL)
		err(EXIT_FAILURE, "fopen");


	while (n > 0) {
		if ((n = read(fileno(file), buf, DATA_LEN)) == -1)
			err(EXIT_FAILURE, "read");
		/* Fill and send header. */
		hdr.opcode = OPCODE_DATA;
		hdr.data_blocknum = blocknum;
		hdr.data_data = buf;
		hdr.data_len = n;
		send_hdr(&hdr);
		/* Wait for ACK. */
		// TODO Start clock
		if ((received = recvfrom(client_sock, recv_buf, BUF_LEN, 0, &client_addr,
				&client_addr_len)) == -1)
			err(EXIT_FAILURE, "recvfrom");

		read_packet(&hdr, recv_buf, received);
		/* Check if received packet is of ACK type. */
		if (hdr.opcode == OPCODE_ACK) {
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

		/* Read packet and save mode */
		read_packet(&hdr, buff, n);

		switch (hdr.opcode) {
		case OPCODE_RRQ:
			mode = hdr.req_mode;
			read_file(hdr.req_filename);
			break;

		case OPCODE_WRQ:
			mode = hdr.req_mode;
			write_file(hdr.req_filename);
			break;
		}
	}

	freeaddrinfo(res);
}

int
main()
{
	generic_server();
	//tests();
}

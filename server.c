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
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include "tftp.h"
#include "file_op.h"

static char *dirpath;
static char filepath[FILEPATH_LEN];
static int first_received = 1;
static tftp_mode_t mode;
static int client_sock;
static struct sockaddr client_addr;
static socklen_t client_addr_len = sizeof(client_addr);
static jmp_buf timeoutbuf;
static unsigned int timeout = 3;
static char port[PORT_LEN] = "0";

static const char *const err_msgs[] = {
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
print_usage(char *program)
{
	fprintf(stderr, "Usage: %s [-p port] [-t timeout] directory", program);
	exit(1);
}

/**
 * Return the port specified in options or standard TFTP port if this process
 * is ran with superuser privileges.
 */
void
resolve_service_by_privileges(char *service)
{
	struct servent *ent;

	/* Port specified as option. */
	if (port[0] != 0) {
		strncpy(service, port, PORT_LEN);
		return;
	}
	/* Root privileges. */
	else if (getuid() == 0) {
		if ((ent = getservbyname("tftp", "udp")) == NULL)
			err(EXIT_FAILURE, "getservbyname");
		snprintf(service, PORT_LEN, "%d", ntohs(ent->s_port));
	}
	/* Error. */
	else {
		fprintf(stderr, "No port specified and not enough privileges.");
		exit(EXIT_FAILURE);
	}
}

/**
 * Generates random non-privileged port.
 */
void
random_service(char *service)
{
	int rndnum = rand() % UINT16_MAX;

	if (rndnum <= 1024)
		rndnum += 1024;
	snprintf(service, PORT_LEN, "%d", rndnum);
}

void
send_hdr(const tftp_header_t *hdr)
{
	size_t buf_len = header_len(hdr);
	uint8_t buf[buf_len];

	copy_to_buffer(buf, hdr);

	if (sendto(client_sock, buf, buf_len, 0, &client_addr, client_addr_len) == -1)
		err(EXIT_FAILURE, "sendto");
}

/**
 * Receives packet from client and fills in the hdr structure from this packet.
 * When unknown TID error occurs, sends error packet and returns 0.
 */
int
receive_hdr(tftp_header_t *hdr)
{
	int n = 0;
	uint8_t buf[PACKET_LEN];
	char old_client_addrdata[14];
	tftp_header_t errorhdr;

	/* Copy old client's addr data value. */
	for (size_t i = 0; i < 14; ++i) {
		old_client_addrdata[i] = client_addr.sa_data[i];
	}

	/* Receive packet. */
	if ((n = recvfrom(client_sock, buf, PACKET_LEN, 0, &client_addr,
			&client_addr_len)) == -1)
		err(EXIT_FAILURE, "recvfrom");

	/* Check if client's port (TID) changed. */
	for (size_t i = 0; i < 14; ++i) {
		if (old_client_addrdata[i] != client_addr.sa_data[i]) {
			/* Error: Unknown client's TID. */
			errorhdr.opcode = OPCODE_ERR;
			errorhdr.error_code = ETID;
			send_hdr(&errorhdr);
			return 0;
		}
	}

	/* Convert packet to tftp_header_t. */
	read_packet(hdr, buf, n);
	return 1;
}

/**
 * Concatenates directory path with filename. The size of result is assigned
 * to size parameter.
 */
char *
concat_paths(const char *dirpath, const char *fname, size_t *size)
{
	*size = strlen(dirpath) + strlen(fname);

	/* Build the string */
	bzero(filepath, *size);
	strcat(filepath, dirpath);
	/* Add trailing / to directory path if necessary. */
	if (dirpath[strlen(dirpath)-1] != '/') {
		strcat(filepath, "/");
	}
	strcat(filepath, fname);

	return filepath;
}

/**
 * Client uploads a file. File is appended or created if it does not exist.
 */
void
write_file(const char *fname)
{
	tftp_header_t hdr;
	FILE *file;
	char *fpath;
	size_t fpath_len;
	uint16_t blocknum = 1;
	int last_packet = 0;

	fpath = concat_paths(dirpath, fname, &fpath_len);
	if ((file = fopen(fpath, "a")) == NULL) {
		hdr.opcode = OPCODE_ERR;

		/* Error: File not found. */
		/*if (errno == ENOENT) {
			hdr.error_code = ENFOUND;
		}*/
		/* Error: Disk full. */
		if (errno == EDQUOT) {
			hdr.error_code = EALLOC;
		}
		/* Error: Access violation. */
		else if (errno == EACCES) {
			hdr.error_code = EACCESS;
		}
		send_hdr(&hdr);
		return;
	}

	/* Send ACK. */
	hdr.opcode = OPCODE_ACK;
	hdr.ack_blocknum = 0;
	setjmp(timeoutbuf);
	send_hdr(&hdr);

	for (;;) {
		alarm(timeout);
		if (!receive_hdr(&hdr))
			break;
		alarm(0);

		if (hdr.opcode == OPCODE_DATA) {
			/* Check if last data packet was received. */
			if (hdr.data_len < DATA_LEN) {
				last_packet = 1;
			}

			/* Send ACK. */
			if (hdr.data_blocknum == blocknum) {
				hdr.opcode = OPCODE_ACK;
				hdr.ack_blocknum = blocknum;
				send_hdr(&hdr);
				blocknum++;
			}
			else {
				/* TODO Received data with unexpected block number. */
			}

			/* Write data to file. */
			write_file_convert(file, mode, (char *) hdr.data_data, hdr.data_len);

			if (last_packet)
				break;
		}
		else {
			/* Error occured - terminate connection. */
			if (hdr.opcode == OPCODE_ERR) {
				fprintf(stderr, "Error: %s, message: %s",
						err_msgs[hdr.error_code], hdr.error_msg);
			}
			else {
				fprintf(stderr, "Unexpected opcode, terminating connection.");
			}
			break;
		}
	}

	fflush(file);
}

/**
 * Client downloads a file.
 */
void
read_file(const char *filename)
{
	uint16_t blocknum = 1;
	tftp_header_t hdr;
	FILE *file;
	uint8_t buf[DATA_LEN];
	size_t fpath_len, bufsize;
	char *fpath;

	/* Initiate and build filepath. */
	fpath = concat_paths(dirpath, filename, &fpath_len);
	if ((file = fopen(fpath, "r")) == NULL) {
		/* Error: File not found. */
		if (errno == ENOENT) {
			hdr.error_code = ENFOUND;
		}
		/* Error: Disk full. */
		else if (errno == EDQUOT) {
			hdr.error_code = EALLOC;
		}
		/* Error: Access violation. */
		else if (errno == EACCES) {
			hdr.error_code = EACCESS;
		}
		send_hdr(&hdr);
		return;
	}


	do {
		read_file_convert(file, mode, (char *) buf, &bufsize, DATA_LEN);

		setjmp(timeoutbuf);
		/* Fill and send header. */
		hdr.opcode = OPCODE_DATA;
		hdr.data_blocknum = blocknum;
		hdr.data_data = buf;
		hdr.data_len = bufsize;
		send_hdr(&hdr);
		/* Wait for ACK. */
		alarm(timeout);
		if (!receive_hdr(&hdr))
			break;
		alarm(0);

		/* Check if received packet is of ACK type. */
		if (hdr.opcode == OPCODE_ACK) {
			if (hdr.ack_blocknum == blocknum) {
				/* ACK received. */
				blocknum++;
			}
			else {
				/* Error: other blocknum acknowledged - resend data. */
				longjmp(timeoutbuf, 1);
			}
		}
		else {
			/* Error occured - terminate connection. */
			if (hdr.opcode == OPCODE_ERR) {
				fprintf(stderr, "Error: %s, message: %s",
						err_msgs[hdr.error_code], hdr.error_msg);
			}
			else {
				fprintf(stderr, "Unexpected opcode, terminating connection.");
			}
			break;
		}
	} while (bufsize == DATA_LEN);
}

void
timeout_handler(int signum)
{
	longjmp(timeoutbuf, 1);
}

void
rebind(const char *service)
{
	int error;
	struct addrinfo *res, hints;
	struct protoent *udp_protocol = getprotobyname("udp");

	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = udp_protocol->p_proto;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

	/* getaddrinfo call that is suitable for binding and then accepting. */
	if ((error = getaddrinfo(NULL, service, &hints, &res)) != 0)
		err(EXIT_FAILURE, "getaddrinfo: %s", gai_strerror(error));

	/* Create socket. */
	if ((client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
		err(EXIT_FAILURE, "socket");

	/* Bind the first address returned by getaddrinfo. */
	if (bind(client_sock, res->ai_addr, res->ai_addrlen) == -1)
		err(EXIT_FAILURE, "bind");

	freeaddrinfo(res);
}

/**
 * Creates generic server that binds to IPv4 and IPv6.
 */
void
generic_server()
{
	int n;
	uint8_t buff[DATA_LEN];
	char service[PORT_LEN];
	tftp_header_t hdr;

	/* Initial binding. */
	resolve_service_by_privileges(service);
	rebind(service);

	/* Receive socket from client. */
	while ((n = recvfrom(client_sock, buff, DATA_LEN, 0, &client_addr,
			&client_addr_len)) != 0) {

		/* Rebind client_sock to random port if necessary. */
		if (first_received) {
			close(client_sock);
			random_service(service);
			rebind(service);
			first_received = 0;
		}

		/* Check for an error. */
		if (n == -1)
			err(EXIT_FAILURE, "recvfrom");

		/* Read packet */
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

		default:
			/* Client should not initiate communication with other opcodes */
			break;
		}

		/* Rebind to default service (port). */
		resolve_service_by_privileges(service);
		rebind(service);
		first_received = 1;
	}
}

void
process_opts(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "p:t:")) != -1) {
		if (opt == 'p') {
			strncpy(port, optarg, PORT_LEN);
		}
		else if (opt == 't') {
			timeout = atoi(optarg);
		}
		else if (opt == '?') {
			print_usage(argv[0]);
		}
	}

	if ((dirpath = argv[optind]) == NULL)
		print_usage(argv[0]);
}

int
main(int argc, char **argv)
{
	struct sigaction action;

	process_opts(argc, argv);

	/* Set timeout action handler. */
	action.sa_handler = timeout_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if (sigaction(SIGALRM, &action, NULL) < 0)
		err(EXIT_FAILURE, "sigaction");

	generic_server();
}

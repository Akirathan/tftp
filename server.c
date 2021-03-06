/*
 * main.c
 *
 *  Created on: Jul 22, 2017
 *      Author: mayfa
 */

#include "server.h"

static char *dirpath;
static __thread char filepath[MAXPATHLEN];
static __thread tftp_mode_t mode;
static __thread int client_sock;
static __thread struct sockaddr client_addr;
static socklen_t client_addr_len = sizeof(client_addr);
static unsigned int timeout = 3;
static char port[PORT_LEN] = "0";
/* Local connection errno. */
static __thread int conn_err = ETFTP_UNDEF;

static void usage(char *program);
static void resolve_service_by_privileges(char *service);
static void random_service(char *service);
static void send_hdr(const tftp_header_t *hdr);
static int receive_hdr(tftp_header_t *hdr);
static char* concat_paths(const char *dirpath, const char *fname);
static void unexpected_hdr(tftp_header_t *hdr);
static void write_file(const char *fname);
static void read_file(const char *fname);
static int rebind(const char *service);
static void generic_server();
static void process_opts(int argc, char **argv);

static void
usage(char *program)
{
	fprintf(stderr, "Usage: %s [-p port] [-t timeout] directory\n", program);
	exit(1);
}

/**
 * Return the port specified in options or standard TFTP port if this process
 * is ran with superuser privileges.
 */
static void
resolve_service_by_privileges(char *service)
{
	struct servent *ent;

	/* Port specified as option. */
	if (port[0] != '0') {
		strncpy(service, port, PORT_LEN);
		return;
	}
	/* Root privileges. */
	else if (geteuid() == 0) {
		if ((ent = getservbyname("tftp", "udp")) == NULL) {
			perror("getservbyname");
			exit(EXIT_FAILURE);
		}
		snprintf(service, PORT_LEN, "%d", ntohs((uint16_t) ent->s_port));
	}
	/* Error. */
	else {
		fprintf(stderr, "No port specified and not enough privileges.\n");
		exit(EXIT_FAILURE);
	}
}

/**
 * Generates random non-privileged port.
 */
static void
random_service(char *service)
{
	int rndnum = rand() % UINT16_MAX;

	if (rndnum <= 1024)
		rndnum += 1024;
	snprintf(service, PORT_LEN, "%d", rndnum);
}

static void
send_hdr(const tftp_header_t *hdr)
{
	size_t buf_len = header_len(hdr);
	uint8_t buf[buf_len];

	copy_to_buffer(buf, hdr);

	if (sendto(client_sock, buf, buf_len, 0, &client_addr, client_addr_len) == -1) {
		perror("sendto");
		exit(EXIT_FAILURE);
	}
}

/**
 * Receives packet from client and fills in the hdr structure from this packet.
 * When unknow TID error occurs, returns 0.
 * When timeout error occurs, returns 0 and sets conn_err.
 */
static int
receive_hdr(tftp_header_t *hdr)
{
	ssize_t n = 0;
	uint8_t buf[PACKET_LEN];
	char old_client_addrdata[14];
	struct pollfd poll_struct = {client_sock, POLLIN, 0};
	int ret;

	/* Copy old client's addr data value. */
	for (size_t i = 0; i < 14; ++i) {
		old_client_addrdata[i] = client_addr.sa_data[i];
	}

	if ((ret = poll(&poll_struct, 1, timeout * 1000)) == -1) {
		perror("poll");
		exit(EXIT_FAILURE);
	}

	if (ret == 0) {
		/* Timeout. */
		conn_err = ETFTP_TIMEOUT;
		return 0;
	}

	/* Receive packet. */
	if ((n = recvfrom(client_sock, buf, PACKET_LEN, 0, &client_addr,
			&client_addr_len)) == -1) {
		perror("recvfrom");
		exit(EXIT_FAILURE);
	}

	/* Check if client's port (TID) changed. */
	for (size_t i = 0; i < 14; ++i) {
		if (old_client_addrdata[i] != client_addr.sa_data[i]) {
			/* Error: Unknown client's TID. */
			conn_err = ETFTP_TID;
			return 0;
		}
	}

	/* Convert packet to tftp_header_t. */
	read_packet(hdr, buf, (size_t) n);
	return 1;
}

/**
 * Concatenates directory path with filename.
 * Returns NULL if concatenated path exceeds maximum filepath length.
 */
static char *
concat_paths(const char *dirpath, const char *fname)
{
	size_t size;

	if ((size = strlen(dirpath) + strlen(fname) + 1) > MAXPATHLEN)
		return NULL;

	/* Build the string */
	memset(filepath, 0, size);
	strncat(filepath, dirpath, MAXPATHLEN);
	/* Add trailing / to directory path if necessary. */
	if (dirpath[strlen(dirpath)-1] != '/') {
		strcat(filepath, "/");
	}
	strncat(filepath, fname, MAXPATHLEN);

	return filepath;
}

/**
 * Processes header with unexpected opcode. Sends error packet if necessary.
 */
static void
unexpected_hdr(tftp_header_t *hdr)
{
	tftp_header_t errorhdr;


	/* Check unknown opcode. */
	if (OPCODE_RRQ <= hdr->opcode && hdr->opcode <= OPCODE_ERR) {
		/* Send error header: Illegal TFTP operation. */
		fill_error_hdr(&errorhdr, ETFTP_OP, NULL);
		send_hdr(&errorhdr);
	}
	else if (hdr->opcode == OPCODE_ERR) {
		/* Print error header. */
		fprintf(stderr, "Received error packet, code: %d, message: %s\n",
				hdr->error_code, hdr->error_msg);
	}
	else {
		/* Unexpected header opcode. */
		fprintf(stderr, "Received header with unexpected opcode.\n");
	}
}

/**
 * Client uploads a file. File is appended or created if it does not exist.
 * This function manages the whole connection (downloading/uploading of one
 * file).
 */
static void
write_file(const char *fname)
{
	tftp_header_t hdr;
	tftp_header_t ack_hdr;
	FILE *file;
	char *fpath;
	uint16_t blocknum = 0;
	uint16_t errcode = 0;
	int last_packet = 0;
	int resend_ack = 0;
	prev_io_t prev_io = {0, 0};

	if ((fpath = concat_paths(dirpath, fname)) == NULL) {
		/* Error: Filepath too long. */
		fill_error_hdr(&hdr, ETFTP_UNDEF, "Filepath too long.");
	}
	/* Open file for writing. */
	if ((file = fopen(fpath, "a")) == NULL) {
		/* Error: Disk full. */
		if (errno == EDQUOT) {
			errcode = ETFTP_ALLOC;
		}
		/* Error: Access violation. */
		else if (errno == EACCES) {
			errcode = ETFTP_ACCESS;
		}
		fill_error_hdr(&hdr, errcode, NULL);
		send_hdr(&hdr);
		return;
	}

	for (;;) {
		ack_hdr.opcode = OPCODE_ACK;
		ack_hdr.ack_blocknum = blocknum;
		send_hdr(&ack_hdr);

		if (!receive_hdr(&hdr)) {
			if (conn_err == ETFTP_TID) {
				fill_error_hdr(&hdr, ETFTP_TID, NULL);
				send_hdr(&hdr);
				break;
			}
			else if (conn_err == ETFTP_TIMEOUT) {
				/* Resend ACK. */
				resend_ack = 1;
				continue;
			}
		}
		else {
			resend_ack = 0;
		}

		if (!resend_ack)
			blocknum++;

		if (hdr.opcode == OPCODE_DATA) {
			/* Check if last data packet was received. */
			if (hdr.data_len < DATA_LEN) {
				last_packet = 1;
			}

			if (hdr.data_blocknum != blocknum) {
				/* Received data with unexpected block number - resend ACK. */
				resend_ack = 1;
				continue;
			}

			/* Write data to file. */
			write_file_convert(file, &prev_io, mode, (char *) hdr.data_data, hdr.data_len);

			if (last_packet) {
				/* Send ACK for last packet. */
				ack_hdr.ack_blocknum = blocknum;
				send_hdr(&ack_hdr);
				break;
			}
		}
		else {
			/* Error occured - print error and terminate connection. */
			unexpected_hdr(&hdr);
			break;
		}
	}

	fclose(file);
}

/**
 * Client downloads a file.
 */
static void
read_file(const char *filename)
{
	uint16_t blocknum = 1;
	uint16_t errcode = 0;
	tftp_header_t hdr;
	FILE *file;
	uint8_t buf[DATA_LEN];
	size_t bufsize;
	char *fpath;
	prev_io_t prev_io = {0, 0};

	if ((fpath = concat_paths(dirpath, filename)) == NULL) {
		/* Error: File not found. */
		fill_error_hdr(&hdr, ETFTP_NFOUND, NULL);
	}
	/* Open file for reading. */
	if ((file = fopen(fpath, "r")) == NULL) {
		/* Error: File not found. */
		if (errno == ENOENT) {
			errcode = ETFTP_NFOUND;
		}
		/* Error: Disk full. */
		else if (errno == EDQUOT) {
			errcode = ETFTP_ALLOC;
		}
		/* Error: Access violation. */
		else if (errno == EACCES) {
			errcode = ETFTP_ACCESS;
		}
		fill_error_hdr(&hdr, errcode, NULL);
		send_hdr(&hdr);
		return;
	}

	do {
		read_file_convert(file, &prev_io, mode, (char *) buf, &bufsize, DATA_LEN);

	send_data:
		/* Fill and send header. */
		hdr.opcode = OPCODE_DATA;
		hdr.data_blocknum = blocknum;
		hdr.data_data = buf;
		hdr.data_len = bufsize;
		send_hdr(&hdr);
		/* Wait for ACK. */
		if (!receive_hdr(&hdr)) {
			if (conn_err == ETFTP_TID) {
				fill_error_hdr(&hdr, ETFTP_TID, NULL);
				send_hdr(&hdr);
				break;
			}
			else if (conn_err == ETFTP_TIMEOUT) {
				/* Resend data. */
				goto send_data;
			}
		}

		/* Check if received packet is of ACK type. */
		if (hdr.opcode == OPCODE_ACK) {
			if (hdr.ack_blocknum == blocknum) {
				/* ACK received. */
				blocknum++;
			}
			else {
				/* Error: other blocknum acknowledged - resend data. */
				goto send_data;
			}
		}
		else {
			/* Error occured - print error and terminate connection. */
			unexpected_hdr(&hdr);
			break;
		}
	} while (bufsize == DATA_LEN);

	fclose(file);
}

/**
 * Rebind to the given port ie. reopen client socket. Note that this function is
 * called on the beginning of every connection.
 * Returns 0 if bind results in EADDRINUSE.
 * Returns 1 on success.
 */
static int
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

	/* getaddrinfo call that is suitable for binding. */
	if ((error = getaddrinfo(NULL, service, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}

	/* Create socket. */
	if ((client_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	/* Bind the first address returned by getaddrinfo. */
	if (bind(client_sock, res->ai_addr, res->ai_addrlen) == -1) {
		if (errno == EADDRINUSE) {
			freeaddrinfo(res);
			return 0;
		}
	}

	freeaddrinfo(res);
	return 1;
}

/**
 * Parameter for process_connection function.
 */
typedef struct {
	uint8_t buff[DATA_LEN];
	size_t buff_len;
	struct sockaddr client_addr;
	socklen_t client_addr_len;
} parameter_t;

/**
 * Processes one client connection.
 * p is parameter_t from recvfrom function.
 */
static void *
process_connection(void *p)
{
	char service[PORT_LEN];
	tftp_header_t hdr;
	parameter_t par = *((parameter_t *) p);
	int ret = 0;

	/* Rebind client_sock to random port. */
	while (!ret) {
		random_service(service);
		ret = rebind(service);
	}

	/* Copy client_addr and client_addr_len from parameter. */
	client_addr = par.client_addr;
	client_addr_len = par.client_addr_len;

	/* Deserialize buffer into hdr. */
	read_packet(&hdr, par.buff, par.buff_len);

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
		/* Client should not initiate communication with other opcodes. */
		fill_error_hdr(&hdr, ETFTP_OP, NULL);
		send_hdr(&hdr);
		break;
	}

	close(client_sock);
	return NULL;
}

/**
 * Creates generic server that binds either to IPv4 or IPv6, depending on which
 * version is supported on current platform.
 */
static void
generic_server()
{
	ssize_t n;
	uint8_t buff[DATA_LEN];
	char service[PORT_LEN];
	struct sockaddr clientaddr;
	socklen_t clientaddrlen = sizeof(clientaddr);
    pool_t pool;

	/* Initialize thread pool. */
	pool_init(&pool);

	/* Initial binding. */
	resolve_service_by_privileges(service);
	rebind(service);

	/* Receive socket from client. */
	while ((n = recvfrom(client_sock, buff, DATA_LEN, 0, &clientaddr,
			&clientaddrlen)) != 0) {
		parameter_t par;

		/* Fill parameter_t struct. */
		memcpy(par.buff, buff, DATA_LEN);
		par.buff_len = (size_t) n;
		par.client_addr = clientaddr;
		par.client_addr_len = clientaddrlen;

		pool_insert(&pool, process_connection, (void *) &par, sizeof(par));
	}
}

static void
process_opts(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "p:t:")) != -1) {
		if (opt == 'p') {
			strncpy(port, optarg, PORT_LEN);
		}
		else if (opt == 't') {
			timeout = (unsigned int) atoi(optarg);
		}
		else if (opt == '?') {
			usage(argv[0]);
		}
	}

	if (optind != argc - 1)
		usage(argv[0]);

	if ((dirpath = argv[optind]) == NULL)
		usage(argv[0]);
}

int
main(int argc, char **argv)
{
	process_opts(argc, argv);
	generic_server();
}

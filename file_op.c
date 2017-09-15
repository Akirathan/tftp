/*
 * file_op.c
 *
 *  Created on: Sep 3, 2017
 *      Author: mayfa
 */

#include "file_op.h"

static __thread char prev_char;
/* There is a pending char from previous reading or last char from writing
 * buffer is \r */
static __thread int from_prev = 0;

static void write_char(const char c, FILE *file);
static int convert(const char c1, const char c2, char *c);

/**
 * Fills the buffer with data read (and converted if necessary) from file.
 * Assign total buffer size to bufsize.
 *
 * Reads one char from file at a time and if mode is MODE_NETASCII,
 * the following conversion is done:
 * 	\r --> \r \0
 * 	\n --> \r \n
 */
void
read_file_convert(FILE *file, tftp_mode_t mode, char *buf, size_t *bufsize,
		size_t maxbufsize)
{
	size_t buf_idx = 0, n;
	int currchar;

	if (mode == MODE_OCTET) {
		if ((n = read(fileno(file), buf, maxbufsize)) == -1)
			perror("read");
		*bufsize = n;
	}
	else if (mode == MODE_NETASCII) {
		/* Check if there is a pending char from previous reading. */
		if (from_prev) {
			buf[buf_idx++] = prev_char;
			from_prev = 0;
		}

		/* While buffer is not full. */
		while (buf_idx != maxbufsize) {
			/* Get char from file. */
			if ((currchar = getc(file)) == EOF) {
				/* Error or EOF. */
				break;
			}

			/* Convert \r --> \r \0 */
			if (currchar == '\r') {
				/* Check if this is last char that fits into buf. */
				if (buf_idx == maxbufsize - 1) {
					from_prev = 1;
					buf[buf_idx++] = '\r';
					prev_char = '\0';
				}
				else {
					buf[buf_idx++] = '\r';
					buf[buf_idx++] = '\0';
				}
			}
			/* Convert \n --> \r \n */
			else if (currchar == '\n') {
				/* Check if this is last char that fits into buf */
				if (buf_idx == maxbufsize - 1) {
					from_prev = 1;
					buf[buf_idx++] = '\r';
					prev_char = '\n';
				}
				else {
					buf[buf_idx++] = '\r';
					buf[buf_idx++] = '\n';
				}
			}
			/* Copy */
			else {
				buf[buf_idx++] = currchar;
			}
		}
	/* Return number of bytes filled into buffer. */
	*bufsize = buf_idx;
	} /* MODE_NETASCII */
}


static void
write_char(const char c, FILE *file)
{
	if (fputc((int) c, file) == EOF)
		perror("fputc");
}

/**
 * Performs this conversion (netascii):
 *   \r \0 --> \r
 *   \r \n --> \n
 *   \n \r --> \n
 *
 * Returns 1 if conversion is necessary.
 */
static int
convert(const char c1, const char c2, char *c)
{
	if (c1 == '\r' && c2 == '\0') {
		*c = '\r';
		return 1;
	}
	else if (c1 == '\r' && c2 == '\n') {
		*c = '\n';
		return 1;
	}
	else if (c1 == '\n' && c2 == '\r') {
		*c = '\n';
		return 1;
	}
	else {
		return 0;
	}
}

/**
 * Writes given packet into a file and converts data if mode is MODE_NETASCII.
 * Conversion:
 *   \r \0 --> \r
 * 	 \r \n --> \n
 * 	 \n \r --> \n
 */
void
write_file_convert(FILE *file, tftp_mode_t mode, const char *packet,
		size_t packetlen)
{
	if (mode == MODE_OCTET) {
		if (write(fileno(file), packet, packetlen) != packetlen)
			perror("write");
	}
	else if (mode == MODE_NETASCII) {
		/* Check if last char of previous buffer was \r or \n */
		if (from_prev) {
			if (prev_char == '\r' && packet[0] == '\0') {
				write_char('\r', file);
			}
			else if (prev_char == '\r' && packet[0] == '\n') {
				write_char('\n', file);
			}
			else if (prev_char == '\n' && packet[0] == '\r') {
				write_char('\n', file);
			}
			else {
				/* \r \r OR \n \n */
				write_char(prev_char, file);
				write_char(packet[0], file);
			}
		}

		/* Read whole packet and convert. */
		for (size_t i = 0; i < packetlen; ++i) {
			char writechar;

			/* Skip first char. */
			if (from_prev) {
				from_prev = 0;
				continue;
			}

			/* End of packet. */
			if (i == packetlen-1 && (packet[i] == '\r' || packet[i] == '\n')) {
				/* Store for next function invocation. */
				from_prev = 1;
				prev_char = packet[i];
			}
			else if (i == packetlen-1) {
				write_char(packet[i], file);
			}
			/**/
			else if (convert(packet[i], packet[i+1], &writechar)) {
				/* Convert. */
				write_char(writechar, file);
				/* Skip next char */
				i++;
			}
			else {
				/* Copy. */
				write_char(packet[i], file);
			}
		}
	}
}


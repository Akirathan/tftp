/*
 * file_op.c
 *
 *  Created on: Sep 3, 2017
 *      Author: mayfa
 */

#include "file_op.h"

static char prev_char;
/* There is a pending char from previous reading or last char from writing
 * buffer is \r */
static int from_prev = 0;

/**
 * Fills the buffer with data read (and converted if necessary) from file.
 */
size_t
read_file_convert(FILE *f, tftp_mode_t mode, char *buf, size_t bufsize)
{
	char lbuf[bufsize];
	size_t buf_idx = 0, n;

	if (mode == MODE_OCTET) {
		if ((n = read(fileno(f), buf, bufsize)) == -1)
			err(EXIT_FAILURE, "read");
	}
	else if (mode == MODE_NETASCII) {
		if ((n = read(fileno(f), lbuf, bufsize)) == -1)
			err(EXIT_FAILURE, "read");

		/* Check if there is a pending char from previous reading */
		if (from_prev) {
			buf[buf_idx++] = from_prev;
			from_prev = 0;
		}

		/* Read lbuf and fill buf */
		for (int i = 0; i < bufsize; ++i) {
			/* Convert \r --> \r \0 */
			if (lbuf[i] == '\r') {
				/* Check if this is last char that fits into buf */
				if (buf_idx == bufsize - 1) {
					from_prev = 1;
					buf[buf_idx] = '\r';
					prev_char = '\0';
				}
				else {
					buf[buf_idx++] = '\r';
					buf[buf_idx++] = '\0';
				}
			}
			/* Convert \n --> \r \n */
			else if (lbuf[i] == '\n') {
				/* Check if this is last char that fits into buf */
				if (buf_idx == bufsize - 1) {
					from_prev = 1;
					buf[buf_idx] = '\r';
					prev_char = '\n';
				}
				else {
					buf[buf_idx++] = '\r';
					buf[buf_idx++] = '\n';
				}
			}
			/* Copy */
			else {
				buf[buf_idx++] = lbuf[i];
			}
		}
	}
	return n;
}


static void
write_char(const char c, FILE *file)
{
	if (putc((int) c, file) < 0)
		err(EXIT_FAILURE, "putc");
}

/**
 * Writes given buffer into a file and converts data if necessary.
 * Conversion:
 *   \r \0 --> \r
 * 	 \r \n --> \n
 */
void
write_file_convert(FILE *file, tftp_mode_t mode, const char *packet,
		size_t packetlen)
{
	if (mode == MODE_OCTET) {
		if (write(fileno(file), packet, packetlen) != packetlen)
			err(EXIT_FAILURE, "write");
	}
	else if (mode == MODE_NETASCII) {
		/* Check if last char of previous buffer was \r */
		if (from_prev) {
			if (packet[0] == '\0') {
				write_char('\r', file);
			}
			else if (packet[0] == '\n') {
				write_char('\n', file);
			}
			from_prev = 0;
		}

		for (size_t i = 0; i < packetlen; ++i) {
			if (packet[i] == '\r') {
				if (i == packetlen - 1) {
					/* End of packet */
					from_prev = 1;
				}
				else {
					/* Check next char from packet */
					if (packet[i+1] == '\0') {
						write_char('\r', file);
					}
					else if (packet[i+1] == '\n') {
						write_char('\n', file);
					}
					/* Skip next char */
					i++;
				}
			}
			else {
				write_char(packet[i], file);
			}
		}
	}
}


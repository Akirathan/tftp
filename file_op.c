/*
 * file_op.c
 *
 *  Created on: Sep 3, 2017
 *      Author: mayfa
 */

#include "file_op.h"

static char prev_char;
/* There is a pending char from previous reading */
static int from_prev = 0;

/**
 * Fills the buffer with data read (and converted if necessary) from file.
 */
size_t
read_file_convert(FILE *f, tftp_mode_t mode, char *buf, size_t bufsize)
{
	char lbuf[bufsize];
	size_t buf_idx = 0;

	if (mode == MODE_OCTET) {
		if (read(fileno(f), buf, bufsize) == -1)
			err(EXIT_FAILURE, "read");
	}
	else if (mode == MODE_NETASCII) {
		if (read(fileno(f), lbuf, bufsize) == -1)
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
				buf[buf_idx++] = '\r';
				buf[buf_idx++] = '\n';
			}
			/* Copy */
			else {
				buf[buf_idx++] = lbuf[i];
			}
		}
	}
}

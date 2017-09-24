/*
 * file_op.h
 *
 *  Created on: Sep 3, 2017
 *      Author: mayfa
 *
 * This file contains declarations for functions that are necessary for writing
 * or reading to files with netascii conversion.
 */

#ifndef FILE_OP_H_
#define FILE_OP_H_

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include "tftp.h"

typedef struct _prev_io {
	char prev_char;
	/* There is a pending char from previous reading or last char from writing
	 * buffer is \r */
	bool from_prev;
} prev_io_t;

void read_file_convert(FILE *file, prev_io_t *prev_io, tftp_mode_t mode, char *buf,
					   size_t *bufsize, size_t maxbufsize);
void write_file_convert(FILE *file, prev_io_t *prev_io, tftp_mode_t mode,
						const char *packet, size_t packetlen);

#endif /* FILE_OP_H_ */

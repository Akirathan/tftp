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
#include <err.h>
#include "tftp.h"

void read_file_convert(FILE *file, tftp_mode_t mode, char *buf, size_t *bufsize,
		size_t maxbufsize);
void write_file_convert(FILE *file, tftp_mode_t mode, const char *packet,
		size_t packetlen);

#endif /* FILE_OP_H_ */

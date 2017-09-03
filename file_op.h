/*
 * file_op.h
 *
 *  Created on: Sep 3, 2017
 *      Author: mayfa
 */

#ifndef FILE_OP_H_
#define FILE_OP_H_

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include "tftp.h"

#define WRITEBUF_LEN	256

size_t read_file_convert(FILE *file, tftp_mode_t mode, char *buf, size_t bufsize);
void write_file_convert(FILE *file, tftp_mode_t mode, const char *packet,
		size_t packetlen);

#endif /* FILE_OP_H_ */

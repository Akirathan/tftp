/*
 * server.h
 *
 *  Created on: Sep 6, 2017
 *      Author: mayfa
 */

#ifndef SERVER_H_
#define SERVER_H_

#define FILEPATH_LEN		50 // TODO

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

#endif /* SERVER_H_ */
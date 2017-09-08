/*
 * tftp.h
 *
 *  Created on: Aug 10, 2017
 *      Author: mayfa
 *
 * This file contains opcode, mode and header struct definitions necessary
 * for TFTP communication. copy_to_buffer and read_packet are functions to
 * serialize and deserialize the byte buffers to be sent over the network.
 */

#ifndef TFTP_H_
#define TFTP_H_

#include <stdint.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#define FILENAME_LEN		20
#define MODENAME_LEN		8
#define ERRMSG_LEN			30
#define PORT_LEN			6
#define DATA_LEN			512
#define PACKET_LEN			DATA_LEN + 4
#define NETASCII_STR		"netascii"
#define OCTET_STR			"octet"

/* Error defines. */
#define EUNDEF				0 /* Undefined */
#define ENFOUND				1 /* File not found */
#define EACCESS				2 /* Access violation */
#define EALLOC				3 /* Disk full or allocation exceeded */
#define EOP					4 /* Illegal TFTP operation */
#define ETID				5 /* Unknown TID */
#define EFEXIST				6 /* File already exists */
#define EUSER				7 /* No such user */

typedef enum {
	OPCODE_RRQ = 1,
	OPCODE_WRQ = 2,
	OPCODE_DATA = 3,
	OPCODE_ACK = 4,
	OPCODE_ERR = 5
} opcode_t;

typedef enum {
	MODE_UNKNOWN,
	MODE_NETASCII,
	MODE_OCTET
} tftp_mode_t;

typedef struct {
	opcode_t opcode;
	union {
		struct {
			char *filename;
			tftp_mode_t mode;
		} req_strings;

		struct {
			uint16_t block_num;
			uint8_t *data;
			size_t len;
		} data;

		struct {
			uint16_t block_num;
		} ack;

		struct {
			uint16_t err_code;
			const char *err_msg;
		} err;
	} u;
} tftp_header_t;

#define req_filename	u.req_strings.filename
#define req_mode		u.req_strings.mode
#define data_blocknum	u.data.block_num
#define data_data		u.data.data
#define data_len		u.data.len
#define ack_blocknum	u.ack.block_num
#define error_code		u.err.err_code
#define error_msg		u.err.err_msg

size_t header_len(const tftp_header_t *hdr);
void copy_to_buffer(uint8_t *buf, const tftp_header_t *hdr);
void read_packet(tftp_header_t *hdr, uint8_t *packet, size_t packet_len);
void fill_error_hdr(tftp_header_t *hdr, uint16_t errcode);

#endif /* TFTP_H_ */

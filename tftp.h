/*
 * tftp.h
 *
 *  Created on: Aug 10, 2017
 *      Author: mayfa
 */

#ifndef TFTP_H_
#define TFTP_H_

#include <stdint.h>

#define FILENAME_LEN		20
#define MODENAME_LEN		8
#define NON_PRIVILEGED_PORT	4567
#define PORT_LEN			6
#define DATA_LEN			512

const char *const err_msgs[] = {
		"Undefined",
		"File not found",
		"Access violation",
		"Disk full or allocation exceeded",
		"Illegal TFTP operation",
		"Unknown transfer ID",
		"File already exists",
		"No such user"
};

typedef enum {
	RRQ_OPCODE = 1,
	WRQ_OPCODE = 2,
	DATA_OPCODE = 3,
	ACK_OPCODE = 4,
	ERR_OPCODE = 5
} opcode_t;

typedef enum {
	NETASCII_MODE,
	OCTET_MODE
} mode_t;

typedef struct {
	opcode_t opcode;
	union {
		struct {
			char *filename;
			mode_t mode;
		} req_strings;

		struct {
			uint16_t block_num;
			uint8_t *data;
		} data;

		struct {
			uint16_t block_num;
		} ack;

		struct {
			uint16_t err_code;
			char *err_msg;
		} err;
	} u;
} tftp_header_t;

#define req_filename	u.req_strings.filename
#define req_mode		u.req_strings.mode
#define data_blocknum	u.data.block_num
#define data_data		u.data.data
#define ack_blocknum	u.ack.block_num
#define error_code		u.err.err_code
#define error_msg		u.err.err_msg

size_t header_len(tftp_header_t *hdr);
void copy_to_buffer(tftp_header_t *hdr, uint8_t *buf);
void read_packet(const uint8_t *packet, int packet_len, tftp_header_t *hdr);

#endif /* TFTP_H_ */

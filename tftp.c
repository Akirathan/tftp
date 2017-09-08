/*
 * tftp.c
 *
 *  Created on: Aug 25, 2017
 *      Author: mayfa
 */

#include "tftp.h"

char filename[FILENAME_LEN];
char errmsg[ERRMSG_LEN];
uint8_t buf[DATA_LEN];

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

static tftp_mode_t mode_from_str(const char *str);
static void str_from_mode(char *str, const tftp_mode_t mode);
static void to_lower_str(char *dest, const char *src);

/**
 * Converts whole string to lower case.
 */
static void
to_lower_str(char *dest, const char *src)
{
	while (*src != '\0') {
		*dest = tolower((int) *src);
		src++;
		dest++;
	}

	*dest = '\0';
}

/**
 * Converts string to mode.
 */
static tftp_mode_t
mode_from_str(const char *str)
{
	char lower_str[strlen(str) + 1];

	to_lower_str(lower_str, str);

	if (strcmp(lower_str, "netascii") == 0) {
		return MODE_NETASCII;
	}
	else if (strcmp(lower_str, "octet") == 0) {
		return MODE_OCTET;
	}
	else {
		return MODE_UNKNOWN;
	}
}

/**
 * Converts given mode to string.
 */
static void
str_from_mode(char *str, const tftp_mode_t mode)
{
	switch (mode) {
	case MODE_NETASCII:
		strcpy(str, "netascii");
		break;
	case MODE_OCTET:
		strcpy(str, "octet");
		break;
	default:
		break;
	}
}

/**
 * Calculates length of the tftp_header_t.
 */
size_t
header_len(const tftp_header_t *hdr)
{
	char str[MODENAME_LEN];
	/* Initiate size with opcode size. */
	size_t size = 2;

	switch (hdr->opcode) {
	case OPCODE_RRQ:
	case OPCODE_WRQ:
		/* Count also terminating null for both strings. */
		size += strlen(hdr->req_filename) + 1;
		/* Calculate size of modename. */
		str_from_mode(str, hdr->req_mode);
		size += strlen(str) + 1;
		break;

	case OPCODE_DATA:
		size += 2; /* block number */
		size += hdr->data_len;
		break;

	case OPCODE_ACK:
		size += 2; /* block number */
		break;

	case OPCODE_ERR:
		size += 2; /* error code */
		if (hdr->error_msg != NULL)
			size += strlen(hdr->error_msg) + 1; /* error message */
		break;
	}

	return size;
}

/**
 * "Deserialize" packet buffer into header.
 */
void
read_packet(tftp_header_t *hdr, uint8_t *packet, size_t packet_len)
{
	uint8_t *packet_idx = packet;
	char modename[MODENAME_LEN];
	uint16_t opcode, blocknum, errcode;

	/* First two bytes are opcode. */
	opcode = ntohs(*((uint16_t *) packet));
	hdr->opcode = opcode;
	packet_idx += 2;

	switch (hdr->opcode) {
	case OPCODE_RRQ:
	case OPCODE_WRQ:
		/* Extract filename. */
		strcpy(filename, ((char *)packet_idx));
		hdr->req_filename = filename;

		packet_idx += strlen((char *)packet_idx);
		packet_idx++; /* skip \0 */

		/* Extract modename. */
		strcpy(modename, (char *) packet_idx);
		hdr->req_mode = mode_from_str(modename);
		break;

	case OPCODE_DATA:
		/* Block number. */
		blocknum = ntohs(*((uint16_t *) packet_idx));
		hdr->data_blocknum = blocknum;
		packet_idx += 2;

		/* Data length. */
		hdr->data_len = packet_len - 4;

		/* Copy data from packet into local buffer */
		memcpy(buf, packet_idx, hdr->data_len);
		hdr->data_data = buf;

		break;

	case OPCODE_ACK:
		/* Block number. */
		blocknum = ntohs(*((uint16_t *) packet_idx));
		hdr->ack_blocknum = blocknum;
		break;

	case OPCODE_ERR:
		/* Error code */
		errcode = ntohs(*((uint16_t *) packet_idx));
		hdr->error_code = errcode;
		packet_idx += 2;

		/* Error message */
		strncpy(errmsg, (char *) packet_idx, ERRMSG_LEN);
		hdr->error_msg = errmsg;
	}
}

/**
 * "Serialize" header into buffer ie. copy header into buffer in network byte
 * order. Suppose buf is large enough.
 */
void
copy_to_buffer(uint8_t *buf, const tftp_header_t *hdr)
{
	uint8_t *buf_idx = buf;
	uint16_t blocknum, opcode, errcode;
	char modename[MODENAME_LEN];

	/* Copy opcode and "shift" two bytes */
	opcode = htons(hdr->opcode);
	*((uint16_t *)buf_idx) = opcode;
	buf_idx += 2;

	switch (hdr->opcode) {
	case OPCODE_RRQ:
	case OPCODE_WRQ:
		/* Filename */
		strcpy((char *)buf_idx, hdr->req_filename);
		buf_idx += strlen(hdr->req_filename);
		*buf_idx = '\0';
		buf_idx++;

		/* Modename */
		str_from_mode(modename, hdr->req_mode);
		strcpy((char *)buf_idx, modename);
		buf_idx += strlen(modename);
		*buf_idx = '\0';
		break;

	case OPCODE_DATA:
		/* Copy block number and shift two bytes */
		blocknum = htons(hdr->data_blocknum);
		*((uint16_t *)buf_idx) = blocknum;
		buf_idx += 2;

		memcpy(buf_idx, hdr->data_data, hdr->data_len);

		break;

	case OPCODE_ACK:
		/* Copy only block number */
		blocknum = htons(hdr->ack_blocknum);
		*((uint16_t *)buf_idx) = blocknum;
		break;

	case OPCODE_ERR:
		/* Error code */
		errcode = htons(hdr->error_code);
		*((uint16_t *)buf_idx) = errcode;
		buf_idx += 2;

		/* Error message */
		if (hdr->error_msg == NULL) {
			*buf_idx = '\0';
		}
		else {
			strncpy((char *) buf_idx, hdr->error_msg, ERRMSG_LEN);
		}
		break;
	}
}

/**
 * Fills in hdr struct with error opcode and corresponding message.
 */
void
fill_error_hdr(tftp_header_t *hdr, uint16_t errcode)
{
	hdr->opcode = OPCODE_ERR;
	hdr->error_code = errcode;
	hdr->error_msg = err_msgs[errcode];
}

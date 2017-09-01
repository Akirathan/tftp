/*
 * tftp.c
 *
 *  Created on: Aug 25, 2017
 *      Author: mayfa
 */

#include "tftp.h"

char filename[FILENAME_LEN];

static tftp_mode_t mode_from_str(const char *str);
static void str_from_mode(char *str, const tftp_mode_t mode);

static tftp_mode_t
mode_from_str(const char *str)
{

}

/**
 * Converts given mode to string.
 */
static void
str_from_mode(char *str, const tftp_mode_t mode)
{
	switch (mode) {
	case NETASCII_MODE:
		strcpy(str, "netascii");
		break;
	case OCTET_MODE:
		strcpy(str, "octet");
		break;
	}
}

/**
 * Calculates length of the tftp_header_t.
 */
size_t
header_len(tftp_header_t *hdr)
{

}

/**
 * Fills in the tftp_header_t struct
 */
void
read_packet(const uint8_t *packet, int packet_len, tftp_header_t *hdr)
{
	int i = 2, file_idx = 0, mode_idx = 0;
	char modename[MODENAME_LEN];
	uint16_t opcode;

	/* First two bytes are opcode. */
	opcode = ntohs(*((uint16_t *) packet));
	hdr->opcode = opcode;

	switch (hdr->opcode) {
	case RRQ_OPCODE:
	case WRQ_OPCODE:
		/* Extract filename. */
		strcpy(filename, packet[i]);
		hdr->req_filename = filename;

		i += strlen(packet[i]);

		/* Extract modename. */
		strcpy(modename, packet[i]);
		hdr->req_mode = mode_from_str(modename);

		break;
	}
}

/**
 * Convert hdr to byte buffer - should be compared with packet sent by orig
 * client. Suppose buf has correct length.
 */
void
copy_to_buffer(uint8_t *buf, const tftp_header_t *hdr)
{
	uint8_t *buf_idx = buf;
	char modename[MODENAME_LEN];

	uint16_t opcode = htons(hdr->opcode);
	/* Save opcode and "shift" two bytes. */
	*((uint16_t *)buf_idx) = opcode;
	buf_idx += 2;

	switch (hdr->opcode) {
	case RRQ_OPCODE:
		/* Filename. */
		strcpy(buf_idx, hdr->req_filename);
		buf_idx += strlen(hdr->req_filename);
		*buf_idx = '\0';
		buf_idx++;

		/* Modename. */
		str_from_mode(modename, hdr->req_mode);
		strcpy(buf_idx, modename);
		buf_idx += strlen(modename);
		*buf_idx = '\0';
		break;
	}
}

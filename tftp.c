/*
 * tftp.c
 *
 *  Created on: Aug 25, 2017
 *      Author: mayfa
 */

#include "tftp.h"

/**
 * Fills in the tftp_header_t struct
 */
void
read_packet(const uint8_t *packet, int packet_len, tftp_header_t *hdr)
{
	int i = 2, file_idx = 0, mode_idx = 0;

	/* First two bytes are opcode. */
	if (packet[0] != 0) {
		hdr->opcode = ERR_OPCODE;
	}
	else {
		hdr->opcode = packet[1];
	}

	switch (hdr->opcode) {
	case RRQ_OPCODE:
	case WRQ_OPCODE:
		/* Extract filename. */
		while (packet[i] != '\0') {
			filename[file_idx++] = packet[i];
			++i;
		}
		hdr->req_filename = filename;

		++i;

		/* Extract modename. */
		while (packet[i] != '\0') {
			modename[mode_idx++] = packet[i];
			++i;
		}
		hdr->req_mode = modename;

		break;
	}
}

/**
 * Convert hdr to byte buffer - should be compared with packet sent by orig
 * client. Suppose buf has correct length.
 */
void
copy_to_buffer(tftp_header_t *hdr, uint8_t *buf)
{
	uint8_t *buf_idx = buf;

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
		strcpy(buf_idx, hdr->req_mode);
		buf_idx += strlen(hdr->req_mode);
		*buf_idx = '\0';
		break;
	}
}

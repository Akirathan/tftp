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
static void to_lower_str(char *dest, const char *src);

void
tests()
{
	tftp_mode_t mode = mode_from_str("NETascii");
}

/**
 * Converts whole string to lower case
 */
static void
to_lower_str(char *dest, const char *src)
{
	const char *src_idx = src;
	char *dest_idx = dest;

	while (*src_idx != '\0') {
		*dest_idx = (char) tolower((int) *src_idx);
		dest_idx++;
		src_idx++;
	}

	*dest_idx = '\0';
}

static tftp_mode_t
mode_from_str(const char *str)
{
	char lower_str[strlen(str)];

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
read_packet(tftp_header_t *hdr, const uint8_t *packet, int packet_len)
{
	const uint8_t *packet_idx = packet;
	char modename[MODENAME_LEN];
	uint16_t opcode;

	/* First two bytes are opcode. */
	opcode = ntohs(*((uint16_t *) packet));
	hdr->opcode = opcode;
	packet_idx += 2;

	switch (hdr->opcode) {
	case RRQ_OPCODE:
	case WRQ_OPCODE:
		/* Extract filename. */
		strcpy(filename, ((char *)packet_idx));
		hdr->req_filename = filename;

		packet_idx += strlen((char *)packet_idx);

		/* Extract modename. */
		strcpy(modename, ((char *)packet_idx));
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
	case WRQ_OPCODE:
		/* Filename. */
		strcpy((char *)buf_idx, hdr->req_filename);
		buf_idx += strlen(hdr->req_filename);
		*buf_idx = '\0';
		buf_idx++;

		/* Modename. */
		str_from_mode(modename, hdr->req_mode);
		strcpy((char *)buf_idx, modename);
		buf_idx += strlen(modename);
		*buf_idx = '\0';
		break;
	}
}

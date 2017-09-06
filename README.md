## Overview
TFTP server implementation conforming to [RFC 1350](https://tools.ietf.org/html/rfc1350).

## Build
To build the server simply run `make`, this will create `tftp_server` binary in current folder.

## Usage
`tftp_server [-p port] [-t timeout] directory`

- `p`
	- Specifies port to bind the server to. If port is omitted and server is started with superuser privileges, server is bind to port 69 (as defined in `/etc/services`). If port is omitted and server is not started with superuser privileges, an error is thrown.
- `t`
  - Specifies timeout value in seconds between every packet retransmition (this happens in case when the last packet was not acknowledged). Default value is 3.
- `directory`
  - Specifies root directory for the server.

## Implementation
First options are processed and alarm signal handler is set.
Then control is passed to `generic_server` function that listens on initial port (69 or the one specified in options).
After the first packet from client is received, random non-privileged port (TID) is chosen for the rest of the connection.
Depending whether client uploads or downloads file, control is passed to `write_file` or `read_file` function respectively.
Those functions handle rest of the connection.
After control returns from one of those functions, server is set to listen on initial port again (this is done in `rebind` function).


### Handled errors
- **Unknown TID**: This error occurs when the client address has changed since last received packet and is handled in `receive_hdr` function.
- **Illegal TFTP operation**: This error occurs when unknown opcode was received and is handled in `unexpected_hdr` function.
- **File not found**, **Disk full** and **Access violation** occurs when file is opened either for reading or writing. That means when client downloads or uploads a file. File not found error can occur only in `read_file` function ie. when client specifies non-existing file for download.
- **File already exists**: This error is ignored. When client uploads a file that already exists in the server directory, it is appended (this is the same behavior as for `tftpd-hpa`)
- **No such user**: This error is ignored. No authentication is implemented.

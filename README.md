## Overview
TFTP server implementation conforming to [RFC 1350](https://tools.ietf.org/html/rfc1350), supporting parallel client connection.

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
First options are processed and alarm signal handler is reset.
Then control is passed to `generic_server` function that listens on initial port (69 or the one specified in options).
After the first packet from client is received, new work is assigned into `thread_pool` via `pool_insert` function and manages rest of the connection.
Note that in `thread_pool.h` number of concurrently running threads can be configured with `THREAD_NUM` define.
Depending whether client uploads or downloads file, control is passed to `write_file` or `read_file` function respectively.

## Tests
There is a python script `tftp_tests.py` for testing in a root directory.
In order to run the tests you need to modify some constants in the script (client and server directory, server ip and port).
Client and server directory must be distinct.
Files are created, uploaded, downloaded and then removed in the script.
Every testing function prints the result on the end, along with some logging information.
Note that python 3 is required.


### Handled errors
- **Unknown TID**: This error occurs when the client address has changed since last received packet and is handled in `receive_hdr` function.
- **Illegal TFTP operation**: This error occurs when unknown opcode was received and is handled in `unexpected_hdr` function.
- **File not found**, **Disk full** and **Access violation** occurs when file is opened either for reading or writing. That means when client downloads or uploads a file. File not found error can occur only in `read_file` function ie. when client specifies non-existing file for download.
- **File already exists**: This error is ignored. When client uploads a file that already exists in the server directory, it is appended (this is the same behavior as for `tftpd-hpa`)
- **No such user**: This error is ignored. No authentication is implemented.

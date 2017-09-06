## Overview
TFTP server implementation conforming to [RFC 1350](https://tools.ietf.org/html/rfc1350).

## Usage
server [-p port] [-t timeout] directory

- p
  - Specifies port to bind the server to. If port is omitted and server is started with superuser privileges, server is bind to port 69 (as defined in `/etc/services`). If port is omitted and server is not started with superuser privileges, an error is ??.
- t
  - Specifies timeout value in seconds between every packet retransmition (this happens in case when the last packet was not acknowledged).
- directory
  - 

## Implementation
If user wants to put a file to the server eg. upload a file and the file does not exist yet, it is created. If it already exists, it is appended.

## Testing
Testing can be done with `tftp-hpa` client.
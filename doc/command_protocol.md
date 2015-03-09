# Introduction

This document describes a sub-protocol that dnscat2 uses, the dnscat2 command protocol. It's intended to be a simple command/response scheme that delivers machine-readable commands to the client.

# License

See LICENSE.md.

# High-level

When a client connects to a server, it can choose to use the comand protocol by setting OPT_COMMAND in the SYN packet. After that, all messages exchanged should use the command protocol. At some point, I might make that option default for dnscat2 clients, but for now it can be activated using --command. (Or will be, once I implement it).

Dnscat2 command packets are used behind-the-scenes for dnscat2 servers and clients. It's not necessary to understand this protocol to use dnscat2, this is a reference for myself (and other hackers who might get involved).

Both the dnscat2 client and server can initiate a request, although it is expected that most (if not all) messages will be initiated by the server.

Because dnscat2 already has a client and server component, it is confusing to refer to command-protocol actors as a client or a server, so refer to them as 'requester' and 'responder' in this document.

A requester sends a dnscat2 command packet in one or more dnscat2 packets. Multiple requests can be sent, one after the other, without waiting for a response, but the requests can't be interleaves for obvious reasons.

The responder performs actions requested in the message, and responds, eventually, using the same request_id the requester used. Only one response should be sent for a given request. Unexpected responses should be ignored.

Errors are indicated in the status field, by setting the status to a non-zero value. Global errors (ie, errors that apply to every message type) start with a 1-bit (0x8000 - 0xFFFF). Errors without the first bit set are defined by the command itself.

There is no time limit on how long a response should take, nor is there a requirement to order responses in any particular way.

# Structure

This is the structure of a dnscat2 command packet (note: all fields are network byte order / big endian / msb first):

- (uint32_t) length (of the rest of the message)
- (uint16_t) request_id
- (uint16_t) command_id
- (variable...) command fields

The length field is the number of bytes coming in the rest of the packet. It is presumed that this will span one or more dnscat packets, so it's important that the client can save the chunks it's received.

The request_id field is echoed back by the client. If the initiator sends multiple concurrent requests, it can be used to determine which request this responds to. If concurrent requests aren't being used by the requester, this field can be safely ignored and set to 0. If using this field, good randomness isn't required; command_ids can be incremental, either per-dnscat2 session, or globally (although globally incremental leaks a tiny amount of information to clients, that's a very minor consideration).

The command_id chooses which command to run. See the 'commands' section below. The response should be the same as the request.

The remaining fields are determined by the message type (see 'commands' below).

- Commands

The following commands are defined:

    #define COMMAND_PING     (0x0000)
    #define COMMAND_SHELL    (0x0001)
    #define COMMAND_EXEC     (0x0002)
    #define COMMAND_DOWNLOAD (0x0003)
    #define COMMAND_UPLOAD   (0x0004)
    #define COMMAND_ERROR    (0xFFFF)

## COMMAND_PING

server->client or client->server

Structure:

- (ntstring) data

Asks the other party to echo back some data. Simply used for testing.

## COMMAND_SHELL

server->client only

Structure:

- (ntstring) name (request only)
- (uint16_t) session_id (response only)

Ask a dnscat2 client to spawn a shell. The shell will be connected back to the dnscat2 server as if the client was run using "dnscat2 --exec sh" or "dnscat2 --exec cmd".

## COMMAND_EXEC

server->client only

Structure:

- (ntstring) name (request only)
- (ntstring) command (request only)
- (uint16_t) session_id (response only)

Ask a dnscat2 client to run the given command, and bind the input to a new session.

## COMMAND_DOWNLOAD

server->client only

Structure:

- (ntstring) filename (request only)
- (variable) data (response only)

Ask a dnscat2 client to over the requested file. The data is the remainder of the packet (we know that size from the command packet header).

If the file isn't found, a COMMAND_ERROR is returned.

## COMMAND_UPLOAD

server->client only

Structure:

- (ntstring) fully qualified filename (request only)
- (variable) data (request only)

Send a file to the remote host. The filename can be fully qualified, or
the file will be uploaded to a path relative to the dnscat client's
location. Like COMMAND_DOWNLOAD, the data is the remainder of the packet.

If the file can't be written, a COMMAND_ERROR is returned.

## COMMAND_ERROR

Structure:

- (uint16_t) status
- (ntstring) reason

Errors are mostly free-form, and can be sent either on their own, as a request, or in response to any other request.

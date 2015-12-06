# Introduction

This document describes a sub-protocol that dnscat2 uses, the dnscat2
command protocol. It's intended to be a simple command/response scheme
that delivers machine-readable commands to the client.

It's used for most operations, including starting a shell, starting and
transferring data via a tunnel, and basically everything else.

dnscat2 command packets are used behind-the-scenes for dnscat2 servers
and clients. It's not necessary to understand this protocol to use
dnscat2, this is a reference for myself (and other hackers who might get
involved).

# License

See LICENSE.md.

# High-level

When a client connects to a server, it can choose to use the command
protocol by setting `OPT_COMMAND` in the SYN packet, which is the
default when running the normal client.

Because dnscat2 already has a client and server component, it is
confusing to refer to command-protocol actors as a client or a server,
so refer to them as 'requester' and 'responder' in this document.

Both the dnscat2 client and server can initiate a request. Responses
have the first bit of `packed_id` set to identify them as such.

A requester sends a dnscat2 command packet in one or more dnscat2
packets. Multiple requests can be sent, one after the other, without
waiting for a response, but the requests can't be interleaved.  Requests
can be identified "on the wire" because the first bit of the `packed_id`
field is 0; the first bit of responses is 1.

The responder performs actions requested in the message, and responds,
optionally, using the same request_id the requester used. No more than
one response should be sent for a given request. Unexpected responses
should be ignored.

Errors are indicated in the status field, by setting the status to a
non-zero value. Global errors (ie, errors that apply to every message
type) start with a 1 bit (0x8000 - 0xFFFF). Errors without the first bit
set are defined by the command itself.

There is no time limit on how long a response should take, nor is there
a requirement to order responses in any particular way.

# Structure

This is the structure of a dnscat2 command packet (note: all fields are
network byte order / big endian / msb first):

- (uint32_t) length (of the rest of the message)
- (uint16_t) packed_id
- (uint16_t) command_id
- (variable...) command fields

The `length` field is the number of bytes coming in the rest of the
packet (that is, the length of the packet not including the length
itself). It's possible for a message to span one or more dnscat packets,
so it's important that the client can save the chunks it's received.

The `packed_id` field is actually two separate values: the first bit is
`is_response`, and is set if and only if the packet is a response. The
remaining 15 bits are the `request_id`.

The `request_id` field is echoed back by the responder. It should be
used to determine which request each response corresponds to. The value
should be different for each outbound packet (unless more than 32,767
packets are sent; in that case it's okay to repeat). Incremental values
are best, but random is fine too.

The `command_id` chooses which command to run. See the 'commands'
section below. A response can have a different `command_id` (most
commonly, a response can be an error packet).

The remaining command fields are determined by the message type.

# Commands

The following commands are defined:

    #define COMMAND_PING     (0x0000)
    #define COMMAND_SHELL    (0x0001)
    #define COMMAND_EXEC     (0x0002)
    #define COMMAND_DOWNLOAD (0x0003)
    #define COMMAND_UPLOAD   (0x0004)
    #define COMMAND_ERROR    (0xFFFF)

## COMMAND_PING

server->client or client->server

Structure (request):
- (ntstring) data

Structure (response):
- (ntstring) data

Asks the other party to echo back some data. Simply used for testing.

## COMMAND_SHELL

server->client only

Structure (request):
- (ntstring) name

Structure (response):
- (uint16_t) session_id

Ask a dnscat2 client to spawn a shell. The shell will be connected back
to the dnscat2 server as if the client was run using `dnscat2 --exec sh`
or `dnscat2 --exec cmd`.

## COMMAND_EXEC

server->client only

Structure (request):
- (ntstring) name
- (ntstring) command

Structure (response):
- (uint16_t) session_id

Ask a dnscat2 client to run the given command, and bind the input to a
new session.

## COMMAND_DOWNLOAD

server->client only

Structure (request):
- (ntstring) filename

Structure (response):
- (variable) data

Ask a dnscat2 client to over the requested file. The data is the
remainder of the packet.

If the file isn't found or accessible, a COMMAND_ERROR should be
returned.

## COMMAND_UPLOAD

server->client only

Structure (request):
- (ntstring) fully qualified filename
- (variable) data

Structure (response):
- n/a

Send a file to the remote host. The filename can be fully qualified, or
the file will be uploaded to a path relative to the dnscat client's
location. Like COMMAND_DOWNLOAD, the data is the remainder of the packet.

If the file can't be written, a COMMAND_ERROR is returned. Otherwise,
the response is simply blank, indicating success.

## COMMAND_ERROR

server->client or client->server

Structure (response):
- (uint16_t) status
- (ntstring) reason

Errors are mostly free-form, and must be sent as a response to something
else.

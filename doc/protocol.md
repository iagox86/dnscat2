# Introduction

This document describes the dnscat2 protocol.

I'm referring to this protocol as the dnscat2 protocol, although,
strictly speaking, it's not specific to dnscat or DNS in any way.
Basically, I needed a protocol that could track logical connections over
multiple lower-level connections/datagrams/whatever that aren't
necessarily reliable and where bandwidth is extremely limited.

Because this is designed for dnscat, it is poll-based - that is, the
client sends a packet, and the server responds to it. The server can't
know where the client is or how to initiate a connection, so that's
taken into account.

This protocol is datagram-based, has 16-bit session_id values that can
track the connection over multiple lower level connections, and handles
lower-level dropped/duplicated/out-of-order packets.

Below, I give a few details on what's required to make this work, a
description of how connections work, some constants used in the
messages, and, finally, a breakdown of the messages themselves.

# License

See LICENSE.md.

# Challenges

DNS is a pretty challenging protocol to use! Some of the problems
include:

* Every message requires a response of some sort
* Retransmissions and drops and out-of-order packets are extremely common
* DNS is restricted to alphanumeric characters, and isn't necessarily case sensitive

Both the and the DNS tunnel protocol and the dnscat protocol itself are
designed with these in mind.

# DNS tunnel protocol

The dnscat protocol itself is, in spite of its name, protocol agnostic.
It can be used over any poll/response protocol, such as DNS, HTTP,
ICMP/Ping, etc. Each of these platforms will have to wrap the data a
little differently, though, and this section discusses how to use the
DNS protocol as a transport channel.

## Encoding

All data in both directions is transported in hex-encoded strings.
"AAA", for example, becomes "414141".

Any periods in the domain name must be ignored. Therefore, "41.4141",
"414.141", and "414141" are exactly equivalent.

## Send / receive

The client can choose whether to append a domain name (the user must
have the authoritative server for the domain name) or prepend a static
tag ("dnscat.") to the message. In other words, the message looks like
this:

    <encoded data>.<domain>

or

    <tag>.<encoded data>

Any data that's not in that form, or that's in an unsupported record
type, can either be discarded by the server or forwarded to an upstream
DNS server.

The dnscat2 server must respond with a proper DNS response directly to
the system that made the request, containing no error bit set and one or
more answers. If more than one answer is present, the first byte of each
answer must be a 1-byte sequence number (intermediate DNS servers
often rearrange the order of records).

The record type of the response records should be the same as the record
type of the request, doing otherwise is more suspicious. The precise way
the answer is encoded depends on the record type.

## DNS Record type

The dnscat server supports the most common DNS message types: `TXT`, `MX`,
`CNAME`, `A`, and `AAAA`.

In all cases, the request is encoded as a DNS record, as discussed
above.

The response, however, varies slightly.

A `TXT` response is simply the hex-encoded data, with nothing else.

A `CNAME` and `MX` record is encoded with a tag prefix or domain postfix
(the same format as the request), otherwise it won't be able to traverse
the DNS network. The `MX` record type also has an additional field in
DNS, the priority field, which can be set randomly.

Finally, `A` and `AAAA` records, much like `TXT`, are simply the raw
data with no prefix/postfix. However, due to the short length of the
responses (4 bytes for `A` and 16 bytes for `AAAA`), multiple answers
are required. Unfortunately, the DNS hierarchy re-arranges answers, so
each record must have a one-byte sequence number prepended. The values
don't matter, as long as they can be sorted to obtain the original
order.

# Connections

These are all the problems I can think of that come up in a protocol like this (this applies to SYN, MSG, or FIN packets):

- A request/response is dropped
- A request/response is repeated
- An old request/response arrives late

All of those need to be considered when handling the message types below.

The concept of a connection is like a TCP connection. A connection is denoted by a 16-bit SessionID value in the header. A client is designed to deal with just a single session (usually), whereas the server is expected to handle multiple simultaneous sessions.

A valid connection starts with the client sending a SYN to the server, and the server responding to that SYN. From that point, until either side sends a FIN (or an arbitrary timeout value is reached), the connection is established.

A FIN terminates a connection, and out-of-connection packets (for example, an unexpected SYN) are generally ignored.

A good connection looks like this:

    +----------------+
    | Client  Server |  [[ Good connection ]]
    +----------------+
    |  SYN -->  |    |
    |   |       v    |
    |   |  <-- SYN   |
    |   v       |    |
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- MSG   |
    |   v       |    |
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- MSG   |
    |  ...     ...   |
    |  ...     ...   |
    |  ...     ...   |
    |   |       |    |
    |   v       |    |
    |  FIN -->  |    |
    |           v    |
    |      <-- FIN   |
    +----------------+

If there's an error in the connection, the server will return a FIN:

    +----------------+
    | Client  Server |  [[ Error during connection ]]
    +----------------+
    |  SYN -->  |    |
    |   |       v    |
    |   |  <-- SYN   |
    |   v       |    |
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- MSG   |
    |   v       |    |
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- FIN   |
    |   v            |
    | (nil)          |
    +----------------+


If an unexpected MSG is received, the server will respond with an error (FIN):

    +----------------+
    | Client  Server |  [[ Good connection ]]
    +----------------+
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- FIN   |
    |   v            |
    | (nil)          |
    +----------------+

If an unexpected FIN is received, the server will ignore it:

    +----------------+
    | Client  Server |  [[ Good connection ]]
    +----------------+
    |  FIN -->  |    |
    |           v    |
    |         (nil)  |
    +----------------+

# Constants

    /* Message types */
    #define MESSAGE_TYPE_SYN        (0x00)
    #define MESSAGE_TYPE_MSG        (0x01)
    #define MESSAGE_TYPE_FIN        (0x02)
    #define MESSAGE_TYPE_PING       (0xFF)

    /* Options */
    #define OPT_NAME             (0x01)
    #define OPT_DOWNLOAD         (0x08)
    #define OPT_CHUNKED_DOWNLOAD (0x10)
    #define OPT_COMMAND          (0x20)

# Messages

Note:

- All fields are big endian.
- It is assumed that we know the length of the datagram; if we don't, a lower-level wrapper is required (eg, for TCP I prefix a 2-byte length header)

## MESSAGE_TYPE_SYN [0x00]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x00]
- (uint16_t) session_id
- (uint16_t) initial seq number
- (uint16_t) options
- If OPT_NAME is set:
  - (ntstring) name
- If OPT_DOWNLOAD or OPT_CHUNKED_DOWNLOAD is set:
  - (ntstring) filename

(Client to server)

- Each connection is initiated by a client sending a SYN containing a random session_id and random initial sequence number to the server as well as its requested options (no options are currently defined).
- If the client doesn't get a response, it should choose a new session_id before retransmitting
  - (this resolves a potential issue where a Server->Client SYN is lost, and the server thinks a session is running while the client doesn't)

- The following options are valid:
  - OPT_NAME - 0x01
    - Packet contains an additional field called the session name, which is a free-form field containing user-readable data
    - (ntstring) session_name
  - OPT_DOWNLOAD - 0x08
    - Packet contains an additional field for the filename to download from the server
    - Server should serve that file in place of stdin
    - Servers should *not* let users read arbitrary files, but rather make it up to the user to choose which files/folders to allow
  - CHUNKED_DOWNLOAD - 0x10
    - Packet contains the filename field, as specified in OPT_DOWNLOAD
    - Each MSG also contains an offset field
    - Each data chunk is exactly XXX bytes long

(Server to client)

- The server responds with its own SYN, containing its initial sequence number and its options.

(Notes)

- Both the session_id and initial sequence number should be randomized, not incremental or static or anything, to make connection-hijacking attacks more difficult (the two sequence numbers and the session_id give us approximately 48-bits of entropy per connection).
- packet_id should be different for each packet, and is entirely designed to prevent caching. Incremental is fine. The peer should ignore it.

(Error states)
- If a client doesn't receive a response to a SYN packet, it means either the request or response was dropped. The client can choose to re-send the SYN packet for the same session, or it can generate a new SYN packet or session.
- If a server receives a second SYN for the same session before it receives a MSG packet, it should respond as if it's valid (the response may have been lost).
- If a client or server receives a SYN for a connection during said connection, it should be silently discarded.

## MESSAGE_TYPE_MSG: [0x01]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x01]
- (uint16_t) session_id
- (variable) other fields, as defined by 'options'
- (byte[]) data

Variable fields

- (if OPT_CHUNKED_DOWNLOAD is enabled)
  - (uint32_t) chunk number
- (otherwiseFIN and close the connection.
- The client and server shouldn't increment their sequence numbers or their saved acknowledgement numbers until the other side has acknowledged the value in a response.
- packet_id should be different for each packet, and is entirely designed to prevent caching. Incremental is fine. The peer should ignore it.

(Command)

- If the SYN contained OPT_COMMAND, the 'data' field uses the command protocol. See command_protocol.md.

## MESSAGE_TYPE_FIN: [0x02]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x02]
- (uint16_t) session_id
- (ntstring) reason
- (variable) other fields, as defined by 'options'

(Client to server)

- A client sends a FIN message to the server when it's completed its connection.

(Server to client)

- The server responds to a client's FIN with its own FIN.
- A server can also respond to a MSG with a FIN either when the connection has been cleanly terminated, or when there's an error in the connection.

(Out-of-state packets)

- Once a FIN has been sent, the client or server should no longer attempt to respond to anything from that connection.

(Notes)

- packet_id should be different for each packet, and is entirely designed to prevent caching. Incremental is fine. The peer should ignore it.

## MESSAGE_TYPE_PING: [0xFF]

- (uint16_t) packet_id
- (uint8_t)  message_type [0xFF]
- (uint16_t) reserved
- (ntstring) data

(Notes)

The reserved field should be ignored. It's simply there to make it easier to parse (since every other packet has a 24-bit header).

- packet_id should be different for each packet, and is entirely designed to prevent caching. Incremental is fine. The peer should ignore it.

(Client to server)

- A client can send a MESSAGE_TYPE_PING packet any time - before, during, or after a session.

(Server to client)

- The server can only respond to client ping, it can't send pings of its own out.

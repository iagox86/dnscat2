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

Both the DNS Transport Protocol and the dnscat protocol itself are
designed with these in mind. Unlike other DNS-tunneling tools, I don't
rely on having a TCP layer taking care of the difficult parts - dnscat
is capable of doing a raw connection over DNS.

# DNS Transport Protocol

The dnscat protocol itself is, in spite of its name, protocol agnostic.
It can be used over any polling protocol, such as DNS, HTTP, ICMP/Ping,
etc, which I'll refer to as a Transport Protocol. Each of these
platforms will have to wrap the data a little differently, though, and
this section discusses how to use the DNS protocol as the transport
channel.

## Encoding

All data in both directions is transported in hex-encoded strings.
"AAA", for example, becomes "414141".

Any periods in the domain name must be ignored. Therefore, "41.4141",
"414.141", and "414141" are exactly equivalent.

Additionally, the protocol is not case sensitive, so "5b" and "5B" are
also equivalent. It's important for clients and servers to handle case
insensitivity, because [Fox0x01](https://github.com/Fox0x01) on github
[reported that](https://github.com/iagox86/dnscat2/pull/62#issuecomment-133471089)
some software [actively mangles](https://developers.google.com/speed/public-dns/docs/security?hl=en#randomize_case)
the case of requests!

## Send / receive

The client can choose whether to append a domain name (the user must
have the authoritative server for the domain name) or prepend a static
tag ("dnscat.") to the message. In other words, the message looks like
this:

    <encoded data>.<domain>

or

    <tag>.<encoded data>

Any data that's not in that form, or that's in an unsupported record
type, or that has an appended domain that's unknown to the dnscat
server, can either be discarded by the server or forwarded to an
upstream DNS server. The server may choose.

The dnscat2 server must respond with a properly formatted DNS response
directly to the host that made the request, containing no error bit set
and one or more answers. If more than one answer is present, the first
byte of each answer must be a 1-byte sequence number (intermediate DNS
servers often rearrange the order of records).

The record type of the response records should be the same as the record
type of the request. The precise way the answer is encoded depends on
the record type.

## DNS Record type

The dnscat server supports the most common DNS message types: `TXT`, `MX`,
`CNAME`, `A`, and `AAAA`.

In all cases, the request is encoded as a DNS record, as discussed
above.

The response, however, varies slightly.

A `TXT` response is simply the hex-encoded data, with nothing else. In
theory, `TXT` records should be able to contain binary data, but
Windows' DNS client truncates `TXT` records at NUL bytes so the encoding
is necessary.

A `CNAME` or `MX` record is encoded the same way as the request: either
with a tag prefix or a domain postfix. This is necessary because
intermediate DNS servers won't forward the traffic if it doesn't end
with the appropriate domain name. The `MX` record type also has an
additional field in DNS, the priority field, which can be set randomly
and should be ignored by the client.

Finally, `A` and `AAAA` records, much like `TXT`, are simply the raw
data with no domain/tag added. However, there are two catches. First,
due to the short length of the answers (4 bytes for `A` and 16 bytes for
`AAAA`), multiple answers are required. Unfortunately, the DNS hierarchy
re-arranges answers, so each record must have a one-byte sequence number
prepended. The values don't matter, as long as they can be sorted to
obtain the original order.

The second problem is, there's no clear way to get the length of the
response, because the response is, effectively in blocks. Therefore, the
length of the data itself, encoded as a signle byte, is preprended to
the message. If the data doesn't wind up being a multiple of the block
size, then it can be padded however the developer likes; the padding
must be ignored by the other side.

An A response might look like:

    0.9.<byte1>.<byte2> 1.<byte3><byte4><byte5> 2.<byte6><byte7><byte8> 3.<byte9>.<pad>.<pad>

Where the leading `0` is the sequence number of the block, the `9` is
the length, and the `2` and `3` are sequence numbers.


# dnscat protocol

Above, I defined the DNS Transport Protocol, which is how to send data
through DNS, Below is the actual dnscat protocol, which is what clients
and servers must talk.

## Connections

A "connection" is a logical session established between a client and a
server. A connection starts with a `SYN`, typically contains one of more
`MSG` packets, typically ends with a `FIN` (or with one party disappearing),
and has a unique 16-bit identifier called the `session_id`. Note that
`SYN`/`FIN` shouldn't be confused with the TCP equivalents - these are
purely a construct of dnscat.

To summarize: A session is started by the client sending the server a
`SYN` packet and the server responding with a `SYN` packet. The client
sends MSG packets and the server responds with `MSG` packets. When the
client decides a connection is over, it sends a `FIN` packet to the
server and the server responds with a `FIN` packet. When the server
decides a connection is over, it responds to a `MSG` from the client
with a `FIN` and the client should no longer respond.

A `flags` field is exchanged in the `SYN` packet. These flags affect the
entire session.

Unexpected packets are ignored in most states. See below for specifics.

Both the dnscat client and the dnscat client are expected to handle
multiple sessions; the dnscat client will often have multiple
simultaneous sessions open with the same server, whereas the server can
have multiple simultaneous connections with different clients.

A good connection looks like this:

    +----------------+
    | Client  Server |
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

If the server decides a connection is over, the server will return a FIN:

    +----------------+
    | Client  Server |
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
    | Client  Server |
    +----------------+
    |  MSG -->  |    |
    |   |       v    |
    |   |  <-- FIN   |
    |   v            |
    | (nil)          |
    +----------------+

If an unexpected FIN is received, the server will ignore it:

    +----------------+
    | Client  Server |
    +----------------+
    |  FIN -->  |    |
    |           v    |
    |         (nil)  |
    +----------------+

## SEQ/ACK numbers

`SEQ` (sequence) and `ACK` (acknowledgement) numbers are used similar to
the equivalent values in TCP. At the start of a connection, both the
client and server choose a random ISN (initial sequence number) and send
it to the other.

The `SEQ` number of the client is the `ACK` number of the server, and
the `SEQ` number of the server is the `ACK` number of the client. That
means that both sides always know which byte offset to expect.

Each side will send somewhere between 0 and an infinite number of bytes
to the other side during a session. As more data gets queued to be sent,
it's helpful to imagine that you're appending the bytes to send to the list
of all bytes ever sent. When a message is going out, the system should
look at its own sequence number and the byte queue to decide what to
send. If there are bytes waiting that haven't been acknowledged by the
peer, it should send as many of those bytes as it can along with its
current sequence number.

When a message is received, the receiver must compare the sequence
number in the message with its own acknowledgement number. If it's
lower, that means that old data is being received and it must be
re-acknowledged (the `ACK` may have gotten lost, which caused the server
to re-send). If it's higher, the data can either be cached until it's
needed, or silently discarded (the peer may be sending multiple packets
at once for speed gains). If it's equal, the message can then be
processed.

When a message is processed, the receiver increments its `ACK` by the
number of bytes in the packet, then responds with the new `ACK`, its
current `SEQ`, and any data that is waiting to be sent.

When the sender sees the incremented `ACK`, it should increment its own
`SEQ` number (assuming the `ACK` value is sane; if it's not, it should
be silently discarded). Then it sends new data from the updated offset
(that is, the new `SEQ` value).

You'll note that both sides are constantly acknowledging the other
side's data (by adding the length to the other side's `SEQ` number)
while sending out its own data and updating its own `SEQ` number (by
looking at the other side's `ACK` number).

## Constants

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

## Messages

This section will explain how to encode each of the message types. All
fields are encoded big endian, and the entire packet is sent via the DNS
Transport Protocol, defined above. It is assumed that the transport
protocol handles the length, and the length of the packet is known.

All messages contain a 16-bit `packet_id` field - this should be changed
(randomized or incremented.. it doesn't matter) for each message sent
and ignored by the receiver. It's purely designed to deal with caching
problems.

### Datatypes

As mentioned above, all fields are encoded as big endian (network byte
order). The following datatypes are used:

* `uint8_t` - an 8-bit (one byte) value
* `uint16_t` - a 16-bit (two byte) value
* `uint32_t` - a 32-bit (four byte) value
* `ntstring` - a null-terminated string (that is, a series of bytes with a NUL byte ("\0") at the end
* `byte[]` - an array of bytes - if no size is specified, then it's the rest of the packet

### MESSAGE_TYPE_SYN [0x00]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x00]
- (uint16_t) session_id
- (uint16_t) initial sequence number
- (uint16_t) options
- If OPT_NAME is set:
  - (ntstring) session_name
- If OPT_DOWNLOAD or OPT_CHUNKED_DOWNLOAD is set:
  - (ntstring) filename

#### Notes

- Each connection is initiated by a client sending a SYN containing a
  random session_id and random initial sequence number to the server as
  well as its requested options
- If the client doesn't get a response, it should choose a new
  session_id before retransmitting
  - (this resolves a potential issue where a Server->Client SYN is lost,
    and the server thinks a session is running while the client doesn't)
- The following options are defined:
  - OPT_NAME - 0x01
    - Packet contains an additional field called the session name, which
      is a free-form field containing user-readable data
  - OPT_DOWNLOAD - 0x08
    - Requests a download from the server
    - Server should serve that file in place of stdin
    - For obvious reasons, servers should *not* let users read arbitrary
      files, but rather make it up to the user to choose which
      files/folders to allow
  - CHUNKED_DOWNLOAD - 0x10
    - Requests a "chunked" download from the server - in the MSG
      packets, the client will let the server know which offset to
      download
    - Each MSG also contains an offset field
    - Each data chunk is exactly XXX-TODO bytes long
- The server responds with its own SYN, containing its initial sequence
  number and its options.
- Both the `session_id` and initial sequence number should be
  randomized, not incremental or static or anything, to make
  connection-hijacking attacks more difficult (the two sequence numbers
  and the session_id give us approximately 48-bits of entropy per
  connection).
- `packet_id` should be different for each packet, and is entirely
  designed to prevent caching. Incremental is fine. The peer should
  ignore it.

#### Error states

- If a client doesn't receive a response to a SYN packet, it means
  either the request or response was dropped. The client can choose to
  re-send the SYN packet for the same session, or it can generate a new
  SYN packet or session.
- If a server receives a second SYN for the same session before it
  receives a MSG packet, it should respond as if it's valid (the
  response may have been lost).
- If a client or server receives a SYN for a connection during said
  connection, it should be silently discarded.

### MESSAGE_TYPE_MSG: [0x01]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x01]
- (uint16_t) session_id
- If OPT_CHUNKED_DOWNLOAD is set:
  - (uint32_t) chunk number
- If OPT_CHUCNKED_DOWNLOAD is not set:
  - (uint16_t) seq
  - (uint16_t) ack
- (byte[]) data

#### Notes

- If the SYN contained OPT_COMMAND, the 'data' field uses the command protocol. See command_protocol.md.

### MESSAGE_TYPE_FIN: [0x02]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x02]
- (uint16_t) session_id
- (ntstring) reason

#### Notes

- Once a FIN has been sent, the client or server should no longer
  attempt to respond to anything from that connection.

### MESSAGE_TYPE_PING: [0xFF]

- (uint16_t) packet_id
- (uint8_t)  message_type [0xFF]
- (uint16_t) ping_id
- (ntstring) data

#### Notes

- The 'ping_id' field should be simply echoed back from the server as if
  it was data

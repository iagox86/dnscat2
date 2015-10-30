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

## Errors

If the server encounters and error (for example, an exception occurs or
a bad message is received), it can take a few actions depending on the
severity:

* For errors that should be ignored (for example, a duplicate SYN packet is
  received), it should return no dnscat2 data (a blank message; on TXT/A/AAAA,
  it's literally no data returned; on CNAME/MX/NS, where the domain name is
  mandatory, the domain name alone should be returned ("skullseclabs.org" for
  example).
* For fatal errors (like an unhandled exception on the server), a FIN packet
  with a descriptive message should be returned.

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

## Encryption / signing

It's important to start by noting: this isn't designed to be strong
encryption, with assurances like SSL. It's designed to be fast, easy to
implement, and to prevent passive eavesdropping. Active (man in the
middle) attacks are only prevented using a pre-shared secret, which is
optional by default.

To summarize, a ECDH and SHA3 are used to generate a shared symmetric
key, which is used with SHA3 and Salsa20 to sign and encrypt all
messages between the client and server.

Only packets' bodies are encrypted; the headers are cleartext. This is
necessary, because otherwise it's impossible to know who encrypted the
message and which key should be used to decrypt it.

The data that crosses the network in cleartext is:

* `packet_id` - a meaningless random value
* `packet_type` - information on the type of packet (syn/msg/fin/etc.)
* `session_id` - uniquely identifies the session

These give no more information than the unencrypted TCP headers of a
typical TLS session, so I decided that it's safe enough.

The next few sections will detail the various parts of the encryption
and signing. At the end, there will be a section with specific
implementation and library information for C and Ruby.

### Key exchange

Key exchange is performed using the "P-256" elliptic curve and SHA3-256.
The client and server each generates a random 256-bit secret key at the
start of a connection, then derive a shared (symmetric) key.  This is
all performed in the `MESSAGE_TYPE_ENC` (aka, `ENC`) packet, subtype
`INIT` (aka, `ENC|AUTH`).

Once both sides have the other's public key, the client and server use
those keys to generate the `shared_secret`, using standard ECDH.  The
`shared_secret` is SHA3'd with a few different static strings to
generate the actual keys:

    `shared_secret = ECDH("P-256", their_public_key, my_private_key)`
    `client_write = SHA3-256(shared_secret || "client_write_key")`
    `client_mac   = SHA3-256(shared_secret || "client_mac_key")`
    `server_write = SHA3-256(shared_secret || "server_write_key")`
    `server_mac   = SHA3-256(shared_secret || "server_mac_key")`

Note that, without peer authentication (see below), this is vulnerable to
man-in-the-middle attacks. But it still prevents passive inspection, so
it isn't completely without merit, and should be used as the default
mode. Servers may choose whether or not to require encryption and
authentication.

### Peer authentication

The client and server can optionally use a pre-shared secret (ie, a
password or a key) to prevent man-in-the-middle attacks.

`preshared_secret` is something the client and server have to
pre-decide.  Typically, it'll be passed in as a commandline argument.
The server may even auto-generate a key for each listener and give the
user the specific command to enter the key.

If a client attempts to authenticate, the server *MUST* authenticate
back. If the client authenticates and the server doesn't, the client
*MUST* terminate the connection.

Authentication, like encryption, isn't mandatory in the protocol; the
client can choose whether or not to negotiate it, and the server can
choose whether or not to allow or require it.

The actual authentication is done in an `ENC|AUTH` packet. It should be
sent immediately after the initial key exchange.

The authentication strings are computed using SHA3-256:

    client_authenticator = SHA3-256("client" || shared_secret || pubkey_client || pubkey_server || preshared_secret)
    server_authenticator = SHA3-256("server" || shared_secret || pubkey_client || pubkey_server || preshared_secret)

### Short-authentication strings

Instead of using peer authentication, a short-authentication string can
be used. This is a way for the user to visually validate that the client
and server are connected to each other, and not to somebody else. This
*SHOULD* be displayed to the user if a pre-shared secret isn't being
used.

This is done by first taking the first 6 bytes (48 bits) of this hash:

    sas = SHA3-256("authstring" || shared_secret || public_key_client || public_key_server)[0,6]

Where the public keys are the full x and y coordinates, represented as
64-byte strings (or pairs of 32-byte strings).

After calculating that value, for each byte, look up the corresponding
line in [this word list](/data/wordlist_256.txt) and display it to the
user.  It's up to the user to verify that the 6 words on the client
match the 6 words on the server - they can choose whether or not to
actually do that.

### Stream encapsulation

A standard dnscat2 packet contains a 5-byte header and an arbitrarily
long body.  The header must be transmitted unencrypted, but must be
signed.

After each dnscat2 packet is serialized to a byte stream, but before
it's converted to DNS by the `tunnel_driver`, the packet's body is
wrapped in encryption and the full packet is signed.

To encrypt each packet, a distinct nonce value is required, so an
incremental 16-bit value is used. When the client or server's nonce
value approaches the maximum value (0xFFFF (65535)), the client *must*
initiate a re-negotiation (see below). The client and server *MUST NOT*
allow the other to re-use a nonce or to decrement the nonce, unless a
re-negotiation has happened since it was last used. Any packets
containing the same or a lower nonce should be ignored (but that should
not terminate the connection; otherwise, it's an easy DoS).

A server has to deal with receiving multiple packets with the *same*
nonce carefully. DNS tends to retransmit itself, so receiving multiple
packets with the same nonce isn't surprising. In particular, this will
happen if the server's response was lost on the way back to the client.
If a response is lost, the client will eventually re-transmit with a new
nonce, and the server will be able to handle it appropriately; as such,
a server *MUST NOT* respond to multiple messages with the same nonce.

Encrypting the body is simply done with salsa20:

    encrypted_data = salsa20(packet_body, nonce, write_key)

The packet_body is the sixth byte of the packet and onwards (everything
after the header). The nonce is the nonce value, encoded in big endian,
padded with zeroes to 8 bytes. The write_key is the write_key for the
client or server, as appropriate.

Each packet also requires a signature. This is to prevent
man-in-the-middle attacks, so as long as it can hold off an attacker for
more than a couple seconds, it's suitable. As such, we use SHA3-256
truncated to 48 bits.

The signature covers the 5-byte packet header, the nonce, and the
encrypted body.

The calculation for the signature is:

    signature = SHA3(mac_key || packet_header || nonce || encrypted_body)[0,6]

Where the `write_key` and `mac_key` are either the client's or the
server's respective keys, depending on who's performing the operation.
The `nonce` is the two-byte big-endian-encoded nonce value, and the
`encrypted_body` is, of course, the body after it's been encrypted.

The final encapsulated packet looks like this:

* (byte[5]) header
* (byte[6]) signature
* (byte[2]) nonce
* (byte[])  encrypted_body

### Re-negotiation

Note: This isn't implemented anywhere yet, and is likely to change!

To re-negotiate encryption, the client simply sends another `ENC|INIT`
message to the server with a new public key, encrypted and signed as a
normal message. The server will respond with a new pubkey of its own in
its own `ENC` packet, exactly like the original exchange.

Authentication is not re-performed. The connection is assumed to still
be authenticated.

After successful re-negotiation, the client and server *SHOULD* both
reset their nonce values back to 0. The next packet after the `ENC`
packet should be encrypted with the new key, and the old one should be
discarded (preferably zeroed out so it can't be recovered, if possible).

### Re-transmits and keys

One really annoying thing about dnscat2 is that it has to operate over a
really, really bad protocol: DNS.

A bug I ran into a lot when testing code through actual DNS servers is
that actual DNS servers will gratuitously re-transmit like crazy,
especially if you aren't fast enough (and ruby key generation is a tad
slow). That means the implementation has to deal with re-transmissions
cleanly.

Imagine this: the client starts the session with an `ENC|INIT` packet,
and the server responds with `ENC|INIT`. At that point, the server is
ready to receive encrypted packets! However, the next packet is almost
always another unencrypted `ENC|INIT` packet. That screws up everything.

Likewise when re-keying. You're almost always going to receive at least
one message with the old key when re-keying is performed. That's why we
love DNS so much!

You might think it's safe to just ignore packets you receive with the
wrong key. Unfortunately, that's not enough. Imagine a client sent you
an `ENC|INIT`, either at the start or during a stream, but the response
was dropped. That happens, frequently. At that point, you're expecting
one set of keys, but the client is using another. From then on, it's
impossible to communicate!

Fortunately for clients, they don't have to worry about this. As soon as
a client receives a new key, it can safely cut over to it immediately.
Servers, however, do require some special treatment. Here's how I deal
with it...

Always keep the previous encryption keys handy immediately after
changing them. When a message is received, attempt to decrypt it with
the *new* key *first*. If the signature turns out to be wrong, fall back
to the previous key, and use that to decrypt it. If the previous key has
to be used to decrypt the message, always respond using that key: it's
often a sign that the client doesn't know about the new key yet.

And then, *as soon as you receive data encrypted with the new key,
delete the old one from memory*. Once you've received a packet encrypted
with the key, you know the client has the proper key and you can safely
discard the old ones. But up to that point, you're stuck supporting both
for a brief period.

Note that even with this feature, the server *MUST NOT* allow the same
nonce to be used with the same key! That undermines all security!


### Algorithms

#### ECDH

The Prime256v1 (aka "P-256" or "Nisp256") curve is used for the ECDH key
exchange.

The following are the defined constants:

    p: 11579208921035624876269744694940757353008614_3415290314195533631308867097853951,
    a: -3,
    b: 0x5ac635d8_aa3a93e7_b3ebbd55_769886bc_651d06b0_cc53b0f6_3bce3c3e_27d2604b,
    g: [0x6b17d1f2_e12c4247_f8bce6e5_63a440f2_77037d81_2deb33a0_f4a13945_d898c296,
        0x4fe342e2_fe1a7f9b_8ee7eb4a_7c0f9e16_2bce3357_6b315ece_cbb64068_37bf51f5],
    n: 11579208921035624876269744694940757352999695_5224135760342422259061068512044369,
    h: nil,  # cofactor not given in NIST document

This is implemented in the Ruby gem
[ecdsa](https://github.com/DavidEGrayson/ruby_ecdsa) and in the C library
[micro-ecc](https://github.com/kmackay/micro-ecc).

To generate a keypair in Ruby:

    require 'ecdsa'
    require 'securerandom'
    
    my_private_key = 1 + SecureRandom.random_number(ECDSA::Group::Nistp256.order - 1)
    my_public_key  = ECDSA::Group::Nistp256.generator.multiply_by_scalar(my_private_key)

To import another public key (the values must be Bignum values, see
[encryptor.rb](/server/controller/encryptor.rb) for how to do that):

    their_public_key = ECDSA::Point.new(ECDSA::Group::Nistp256, their_public_key_x, their_public_key_y)

And to generate the shared secret:

    shared_secret = their_public_key.multiply_by_scalar(my_private_key)

In C, the keypair can be generated like this:

    #include "libs/crypto/micro-ecc/uECC.h"
    uECC_make_key(their_public_key, my_private_key, uECC_secp256r1())

To calculate the shared secret, the peer's public key must be formatted
as a 64-byte string, where the first 32 bytes represent the `x`
coordinate and the second 32 bytes represent the `y` coordinate:

    uECC_shared_secret(their_public_key, my_private_key, shared_secret, uECC_secp256r1())

To test your code, here are all the variables in a successful key
exchange:

    alice_private_key:  7997cdc9af9690c78e58468c6a5f273b4c22a8a6e6a0e4be32e81d17c78a3f8b
    alice_public_key_x: a651dedcb8833d574628bbb7b2fa2e63f3ac528aca48d38901955b6c76515c80
    alice_public_key_y: a5d16a0bcfcc76868e9179f44c28eae55b48bacb3168f8977156e1edc7b6334d
    
    bob_private_key:    612b7bb5b84cdb200e4108d6ca52bc4fad94cd04fa8711227e17a268d16a7b85
    bob_public_key_x:   8a748d60b9293e3e5f5d8b50793e476190f869b1006a23aa462ac5cd32572f1a
    bob_public_key_y:   04e11e6440c579a3e13e67661004337ce63fd05bbeaa8c211f8fef844c075b34
    
    shared_secret:      6db2c22f7b0fd8921a15cf22bcbecfe84da0a852075f2707b2a24e19d9f4a6cf

#### SHA3

SHA3 is used in the protocol for simplicity; HMAC and similar constructs
aren't required, we can simply concatenate data inside the hash (as it
was designed to allow).

The downside of SHA3 is that finding a proper implementation can be
tricky!

We use the 256-bit output (SHA3-256) in every case. When we need a
48-bit string, rather than using SHA3-48 (which isn't always
implemented), we simply truncate a SHA3-256 output to the proper length,
which should be safe to do per the SHA3 standard.

The [sha3 gem](https://github.com/johanns/sha3), as of 1.0.1, implements
SHA3 properly. You can verify that whatever your library is using
generates the right string by hashing the empty string:

    1.9.3-p392 :004 > require 'sha3'
     => false
    1.9.3-p392 :003 > SHA3::Digest.new(256).hexdigest('')
     => "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"

If you get that value, it's working! If you get something else, then
look for another implementation. As of late 2015, I found lots of
problematic libraries.

To test your implementation, the `shared_secret` defined above should generate
the following session keys:

    shared_secret:      6db2c22f7b0fd8921a15cf22bcbecfe84da0a852075f2707b2a24e19d9f4a6cf
    client_write_key:   95f786ebf2f4bd460a4031f6b097f54635c27fb8df4c53cfd225c6d9d7ef3abc
    client_mac_key:     726505b9481b72f123fa40aef9f6e777c0070b3cc016f097a8e9569ef4200810
    server_write_key:   a795d45bd0baee7bdef64c7053f1f63b9a2edc0c3c876abe45282dd2dc777d53
    server_mac_key:     40cb251330c07f2cfd084c841a707aa66e81e1d70775d45bcbc6a6ec72f97e91

#### Salsa20

I chose Salsa20 because it has a nice implementation in both C and Ruby,
and is generally considered to be a secure stream cipher. It also uses a
256-bit key, which is rather nice (all my cryptographic values are
256 bits!).

You can verify it's working by encrypting 'password' with a blank
(all-NUL) 256-bit key and a blank (all-NUL) nonce and checking the
output against mine:

    1.9.3-p392 :001 > require 'salsa20'
     => true
    1.9.3-p392 :002 > Salsa20.new("\0"*32, "\0"*8).encrypt("password").unpack("H*")
     => ["eaf68528ec23007f"]

## Constants

    /* Message types */
    #define MESSAGE_TYPE_SYN    (0x00)
    #define MESSAGE_TYPE_MSG    (0x01)
    #define MESSAGE_TYPE_FIN    (0x02)
    #define MESSAGE_TYPE_ENC    (0x03)
    #define MESSAGE_TYPE_PING   (0xFF)

    /* Encryption subtypes */
    #define ENC_SUBTYPE_INIT    (0x00)
    #define ENC_SUBTYPE_AUTH    (0x01)

    /* Options */
    #define OPT_NAME            (0x01)
    #define OPT_COMMAND         (0x20)

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

* `uint8_t` - an 8-bit (one-byte) value
* `uint16_t` - a 16-bit (two-byte) value
* `uint32_t` - a 32-bit (four-byte) value
* `uint64_t` - a 64-bit (eight-byte) value
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

#### Notes

- Each connection is initiated by a client sending a SYN containing a
  random session_id and random initial sequence number to the server as
  well as its requested options
- The following options are defined:
  - OPT_NAME - 0x01 [C->S]
    - Packet contains an additional field called the session name, which
      is a free-form field containing user-readable data
  - OPT_COMMAND - 0x20 [C->S]
    - This is a command session, and will be tunneling command messages
  - OPT_ENCRYPTED - 0x40 [C->S and S->C]
    - We're negotiating encryption
    - `crypto_flags` are currently undefined, and 0
    - The public key x and y values are the BigInteger values converted
      directly to hex values, then padded on the left with zeroes (if
      necessary) to make 32 bytes.
- The server responds with its own SYN, containing its initial sequence
  number and its options.
  - If the client's request contained `OPT_ENCRYPTED`, the server's
    response *MUST* also contain it.
- Both the `session_id` and initial sequence number should be
  randomized, not incremental or static or anything, to make
  connection-hijacking attacks more difficult (the two sequence numbers
  and the session_id give us approximately 48-bits of entropy per
  connection).
- `packet_id` should be different for each packet, and is entirely
  designed to prevent caching. Incremental is fine. The peer should
  ignore it.
- If the server received multiple identical SYN packets, it should reply
  to each of them the same way (this is required in case the response to
  the SYN gets dropped).
- If the server receives a different SYN packet with the same
  session_id, it should be ignored (this prevents session stealing).

#### Error states

- If a client doesn't receive a response to a SYN packet, it means
  either the request or response was dropped. The client can choose to
  re-send the SYN packet for the same session, or it can generate a new
  SYN packet or session.
- If a server receives a second SYN for the same session before it
  receives a MSG packet, it should respond as if it's valid (the
  response may have been lost).
  - "if it's valid" means if it contains the same options, the same
    sequence number, the same name (if applicable), and the same
    encryption key (if applicable).
- If a client or server receives a SYN for a connection during said
  connection, it should be silently discarded.

### MESSAGE_TYPE_MSG: [0x01]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x01]
- (uint16_t) session_id
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

### MESSAGE_TYPE_ENC: [0x03]

- (uint16_t) packet_id
- (uint8_t)  message_type [0x03]
- (uint16_t) session_id
- (uint16_t) subtype
- (uint16_t) flags
- If subtype is ENC_SUBTYPE_INIT:
  - (byte[32]) public_key_x
  - (byte[32]) public_key_y
- If subtype is ENC_SUBTYPE_AUTH:
  - (byte[32]) authenticator

#### Notes
- An `ENC|INIT` packet (`ENC` with subtype `INIT` should be sent
  immediately if the client wants to use encryption
- The server *MUST* respond to an `ENC` packet with its own `ENC` packet
  of the same subtype
  - If the client opts for encryption, the server must opt for
    encryption; if the client authenticates, the server must
    authenticate
- The server *MUST* respond to an `ENC|INIT` packet with the same crypto
  keys - if any - that the client used to send the `ENC|INIT` message.
  Until a client receives the `ENC|INIT` response, it has no way of
  knowing what the key shared key is going to be!
- The public keys and authenticators are encoded as 32-byte hex strings,
  padded with zeroes on the left

Here's the Ruby code for converting an integer `bn` to a binary string:

    [bn.to_s(16).rjust(32*2, "\0")].pack("H*")

And for going from a `binary` blob to an integer:

    binary.unpack("H*").pop().to_i(16)

### MESSAGE_TYPE_PING: [0xFF]

- (uint16_t) packet_id
- (uint8_t)  message_type [0xFF]
- (uint16_t) ping_id
- (ntstring) data

#### Notes

- The 'ping_id' field should be simply echoed back from the server as if
  it was data

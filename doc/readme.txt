+--------------+
| Introduction |
+--------------+

Welcome to dnscat2, a DNS tunnel that WON'T make you sick and kill you!

dnscat2 - the successor to dnscat, also written by me - is an attempt to
right some of the wrongs that I committed while writing the original
version of dnscat. The biggest problem being a total lack of testing or
of forethought into the protocol.

The original dnscat was heavily linked to the dns protocol. It tried to
encode the various control fields - things like sequence number - into
the DNS protocol and use that.

dnscat2, on the other hand, treats everything as a stream of bytes, and
converts that stream of bytes into dns requests. Thus, it's a layered
protocol, with DNS being a lower layer (for testing, I actually used a
TCP layer).

I invented a protocol that I'm calling the dnscat protocol. You can find
documentation about it in docs/protocol.txt. It's a simple polling
network protocol, where the client occasionally polls the server, and
the server responds with a message (or an error code). The protocol is
designed to be resilient to the various issues I had with dnscat1 - that
is, it can handle out-of-order packets, dropped packets, and duplicated
packets equally well.

I also used unit testing for the server, where a fake client
communicates with the dnscat2 server using a fake socket. You can run
those tests yourself by running server/dnscat2_test.rb.

Finally, one last change from the original dnscat is that I decided not
to use the same program for both clients and servers. It turns out that
dnscat servers are much more complex than clients, so it made sense to
write the server in a higher level language (I chose ruby), while still
keeping the client (written in C) as functional/simple/portable as I
possibly could.

+-------------+
| Compilation |
+-------------+

TODO

+--------------+
| Installation |
+--------------+

TODO

+-------+
| Usage |
+-------+

TODO


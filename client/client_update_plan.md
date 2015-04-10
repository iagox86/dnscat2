This branch is meant to be a cleanup of the client code.

Specifically, I want to re-structure it so there's a clear separation of
the "DNS Transport Protocol" and the "dnscat protocol" in the code.

I'm going to rename driver_dns to tdriver_dns - the 't' meaning
transport.

The transport drivers will report to dnscat_manager, which is a static
class (singleton) that will handle the dnscat protocol. dnscat_manager
will create or find the appropriate session to send the data to.

The session will create other drivers - like driver_exec and driver_ping
- and feed them data.

Responses will bubble back up the same path.

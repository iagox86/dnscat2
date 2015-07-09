# Introduction

This document describes the layout and logic behind the dnscat2
codebase. It's intended for developers or perhaps security auditors.

This document complements [protocol.md](protocol.md).

# Client

The client is written in C, and can be found in the client/ directory.
Where possible, the code conforms to the C89 standard, and uses a
variety of libraries I've written over the years. I avoid external
dependencies because as much as possible because I want this to be able
to compile and run in as many environments and with as few
pre-requisites as possible.

Any changes should conform closely to the C89 standard and should not
introduce dependencies unless it's absolutely necessary.

Additionally, unless there's a good reason not to, the code should be
written very modularly - each implementation, xxx.c, should have a
corresponding xxx.h file that defines its types and functions. When it
makes sense, each module should have its own type, xxx_t, which is
created by xxx_create() and destroyed/freed by xxx_destroy().
xxx_destroy(xxx_create()) shouldn't leak memory.

This keeps the code clean and well structured.

Finally, all changes should be compatible with Windows, Linux, OS X, and
FreeBSD, at a minimum.

## Structure

The main file is client/dnscat.c. It contains the main() function, which
handles the commandline parsing, starts the initial drivers, and enters
the main network loop.

To understand the structure, it'd be helpful to understand how the
dnscat protocol works - see [protocol.md](protocol.md) for that. I'll
give a quick overview, though.

### tunnel_driver

The actual code that touches the network is called a tunnel_driver, and
is located in tunnel_drivers/. An example of a tunnel driver - and the
only tunnel_driver that exists as of this writing - is dns. The protocol
I invented for doing packets over DNS (sort of akin to layer 2) is
called the "DNS Tunneling Protocol", and is discussed in detail in
[protocol.md](protocol.md).

The tunnel_driver has no real understanding of the dnscat protocol or of
sessions or 'connections' or anything like that.  The "dnscat protocol"
happens at a higher layer (in sessions), and is tunnel_driver agnostic.
All it does is take data that's embedded in a packet and convert it into
a stream of bytes.

When a tunnel_driver is created, it must create the socket(s) it needs
to communicate with the outside world, and starts polling for data - it
is important for the tunnel driver to be in constant communication with
a dnscat2 server, because the server can only send data to the client
when it's polled.

All communication with the rest of dnscat2 is done via the
[controller](#controller).  When the tunnel_driver gets data, it's
decoded and sent to the controller. When the tunnel_driver is capable of
sending data, it asks the controller for any data that's available.

The tunnel_driver is aware of the controller, but the controller isn't
aware of the tunnel_driver.

### controller

The controller is the go-between from the tunnel_drivers to the
sessions. It's essentially a session manager - it creates, destroys, and
can enumerate sessions as needed.

When data comes in to a tunnel_driver, it's sent to the controller. The
controller parses it just enough to get the session_id value, then it
finds the session that corresponds to that id and sends the data to it.

When data needs to go out, the controller polls the sessions (in a
circular way - it grabs one message from each session that has data
ready on each tick) and sends it along to the tunnel_driver (the
tunnel_driver polls the controller which polls one of the sessions).

The controller knows how to find and talk to sessions, but it doesn't know
anything about the tunnel_driver.

           +---------------+
           | tunnel_driver | (only 1)
           +---------------+
                   |
                   v
            +------------+
            | controller | (only 1)
            +------------+

### session

There are multiple session objects created, each with its own id value.
Each session has a corresponding driver (not to be confused with a
tunnel_driver), which is how it interacts with the real world (the
console, an executable, etc).

sessions can be created and destroyed over the lifetime of the client. For
example, a "command" session might create an "exec" session.

The session module is where the actual dnscat protocol (see
[protocol.md](protocol.md)) is parsed. It's agnostic to both its back
end (a tunnel_driver) and its front end (just a driver).

When data is received by the tunnel_driver, it's sent to the session via the
controller. The packet is parsed as a dnscat protocol packet, and it's fed into
the session's state machine. If everything is okay (a good seq/ack, the right
type in the right state, etc), the data is passed up to the driver. If it's
bad, the driver is simply polled.

When the driver gets polled, it has the opportunity to return its own data. Its
data is wrapped in a dnscat protocol packet (with the proper session_id/seq/ack
values).  That packet goes through the session, then the controller, then the
tunnel_driver, where it's wrapped in a tunnel_driver packet and sent.

The session knows what driver it's using, but has no knowledge of other
sessions or the controller.

           +---------------+
           | tunnel_driver | (only 1)
           +---------------+
                   |
               (has one)
                   |
                   v
            +------------+
            | controller | (only 1)
            +------------+
                   |
           (has one or more)
                   |
                   v
              +---------+
              | session | (at least one)
              +---------+

### drivers

The final part of the structure is drivers, which are stored in drivers/. Each
session has exactly one driver. The driver defines how it interacts with the
outside world (or with the program itself). There are a few types of driver
already created:

* driver_console - simply feeds the program's stdin/stdout into the session.
  Not terribly useful outside of testing. Only one / program is allowed,
  because stdin is a prude (will only talk to one driver at a time)

* driver_exec - execute a process (like cmd.exe) and attach the process's stdin
  / stdout to the session

* driver_ping - this is a 'special' driver that just handles dnscat2 pings (it
  simply echoes back what is sent)

* driver_command - this driver has its own command packets, which can be used
  to upload files, download files, execute commands, etc - basically, it creates
  other drivers.

It's pretty trivial to add more, from a programming perspective. But it's
important for the server to recognize how to handle the special connections
(that is, the if it's a command session, the server needs to know it's a
command session). A driver like driver_exec that simply send/receive data
doesn't require anything special, though.

The driver runs in a vacuum - it knows it gets data and gets polled for data,
but it doesn't know about sessions or controllers or tunnel_drivers (though
there are cases, such as driver_command, where a driver has to access other
sessions (for example, to kill one).

           +---------------+
           | tunnel_driver | (only 1)
           +---------------+
                   |
               (has one)
                   |
                   v
            +------------+
            | controller | (only 1)
            +------------+
                   |
           (has one or more)
                   |
                   v
              +---------+
              | session | (at least one)
              +---------+
                   |
           (has exactly one)
                   |
                   v
              +--------+
              | driver | (exactly one / session)
              +--------+


### Directory structure

There are a few collections of functions that matter:

* libs/
* controller/
* tunnel_drivers/
* drivers/

libs/ is pretty boring - it contains a bunch of standalone libraries,
mostly written by me.

controller/ contains session and controller code. The tunnel_drivers/
and drivers/ directories contain exactly what you'd expect.

## Networking

All networking is done using libs/tcp.c, libs/udp.c, and
libs/select_group.c. All three work and are tested on Windows, Linux, OS
X, and FreeBSD.

The tcp and udp modules provide a simple interface to creating or
destroying sockets without having to understand everything about them.

The select_group module provides a way to asynchronously handle socket
data. Essentially, you can create as many sockets as you want and add
them to the select_group module with a callback for receiving data or
being closed. Then the main() function (or equivalent) calls
select_group_do_select() with a timeout value in an infinite loop.

Each iteration of the loop, the select() syscall is used, and any
sockets that are active have their appropriate callbacks called. From
those callbacks, the socket can be removed from the select_group (for
example, if they decided to close it).

stdin can also be read by select_group, meaning that special code isn't
required to handle stdin. On Windows, a secondary thread is
automatically created to poll for activity; see the asynchronous code
section for info.

## Asynchronous code

There is as little threading as possible, and it should stay
that way. Every socket is put into select(), as well as stdin (when
needed), and when the socket is active an appropriate callback is called
by the select_group module.

As a result, no packet-handling function should take any longer than it
has to. A slow function will slow down the handling of all other
traffic, which isn't great.

There is one place where threads are used, though: on Windows, it isn't
possible to put the stdin handle into select(). So, transparently, the
select_group module will create a thread that basically just reads stdin
and shoves it into a pipe that can be selected on. You can find all the
code for this in libs/select_group.c, and this only affects Windows.

## Memory management

I use sorta weird memory-management techniques that came from a million
years ago when I wrote nbtool: all memory management
(malloc/free/realloc/strdup/etc) takes place in a module called memory,
which can be found in libs/memory.c and libs/memory.h.

Basically, as a developer, all you need to know is to use safe_malloc(),
safe_free(), safe_strdup(), and other safe_* functions defined in
libs/memory.h.

The advantage of this is that in debug mode, these are added to a linked
list of all memory ever allocated. When it's freed, it's marked as such.
When the program terminates cleanly, a list of all unfreed memory
allocations (including the file and line number where it was allocated)
are displayed, giving a platform-independent way to find memory leaks.

An additional advantage is that, for a small performance hit, the memory
being freed is zeroed out, which makes bugs somewhat easier to find. It
will also detect double-frees and terminate automatically.

When debug mode is disabled, safe_malloc() and similar are simple
wrappers for malloc() and friends.

## Parsing

All parsing (and marshaling and most kinds of string-building) should be
done by the buffer module, found in libs/buffer.c and libs/buffer.h.

The buffer module harkens back to Battle.net bot development, and is a
safe way to build arbitrary binary strings. I've used the same module
for 10+ years, with only minor changes (the only major change has been
moving to 64-bit lengths all around).

Essentially, you create a buffer with your chosen byte order (little,
big, network (aka, big) or host:

    buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

Then you can add bytes, integers, null-terminated strings (ntstrings),
byte arrays, and other buffers to it:

    buffer_add_int8(buffer, 1);
    buffer_add_ntstring(buffer, "hello");
    buffer_add_bytes(buffer, (uint8_t*)"hello", 5);

You can convert it to an array and destroy it at the same time (which is
the normal situation):

    size_t length;
    uint8_t *out = buffer_create_string_and_destroy(buffer, &length);
    [...]
    safe_free(out);

You can also create a buffer containing data:

    buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

This makes a copy of data, so you can free it if necessary. Once it's
created you can read in values:

    uint8_t byte = buffer_read_next_int8(buffer);
    uint16_t word = buffer_read_next_int16(buffer);

You can also read an ntstring into a string you allocate:

    char str[128];
    buffer_read_next_ntstring(buffer, str, 128);

Or you can allocate a string and read the next ntstring into it:

    char *str = buffer_alloc_next_ntstring(buffer);
    [...]
    safe_free(str);

You can also read at certain indices (buffer_read_int8_at(...), for
example). See libs/buffer.h for all the various functions.

Currently the error handling consists basically of buffer.[c/h] crashing
when attempting to read too much (it crashes in a safe way, but it still
shouldn't). See below for more info on error handling.

## Error handling

The error handling on the client sucks.

Right now, the main line for error handling is done by the buffer module
(libs/buffer.c and libs/buffer.h). If there's anything wrong with the
protocol (dnscat, dns, etc.), it simply exits.

That's clearly not a great situation for stable code. I plan to go
through and add better error handling in the future, but it's a
non-trivial operation.

# Security

As mentioned in the client error handling section, any bad data causes the
client to terminate.  That's obviously not ideal, but at least it's safe. On
the server, bad data is ignored - an exception is triggered and the connection
is closed.

Some other security concerns:

* Man-in-the-middle: A man-in-the-middle attack is possible, and can cause code
  execution on the client. This has no more defense against tampering than TCP
  has. May add some signing in the future.

* Server: The server should be completely safe (ie, able to be run on trusted
  infrastructure).

* Confidentiality: There is no confidentiality requirement (all data is sent in
  plaintext). May add some crypto in the future.

* Cloaking: From a network traffic perspective, it's exceedingly obvious that
  it's dnscat. It's also possible to trick a dnscat2 server into revealing
  itself (with a ping). There is no hiding.

# Server

TODO: Currently the server, as written, needs to be re-structured and
changed to conform to better styles. dnscat2 0.03 beta will focus on
converting the server to a similar style to the client.

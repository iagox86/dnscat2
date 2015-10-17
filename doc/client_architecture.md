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
created by `xxx_create` and destroyed/freed by `xxx_destroy`. This:

    xxx_destroy(xxx_create())

shouldn't leak memory.

This keeps the code clean and well structured.

Finally, all changes should be compatible with Windows, Linux, OS X, and
FreeBSD, at a minimum.

## Structure

The main file is client/dnscat.c. It contains the `main` function, which
handles the commandline parsing, starts the initial drivers, and enters
the main network loop.

To understand the structure, it'd be helpful to understand how the
dnscat protocol works - see [protocol.md](protocol.md) for that. I'll
give a quick overview, though.

### tunnel_driver

The actual code that touches the network is called a tunnel_driver, and
is located in tunnel_drivers/. An example of a tunnel driver - and the
only tunnel_driver that exists as of this writing - is the dns driver,
which is implemented in
[driver_dns.c](/client/tunnel_drivers/driver_dns.c). The protocol I
invented for doing packets over DNS (sort of akin to layer 2) is called
the "DNS Tunneling Protocol", and is discussed in detail in
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

There is only ever a single tunnel_driver instance running. The
tunnel_driver is aware of the controller, but the controller isn't aware
of the tunnel_driver.

### controller

The controller is the go-between from the
[tunnel_driver](#tunnel_driver) to the [sessions](#session). It's
essentially a session manager - it creates, destroys, and can enumerate
sessions as needed. It's implemented in
[controller.c](/client/controller/controller.c)

When data comes in to a tunnel_driver, it's decoded sent to the
controller. The controller parses it just enough to get the session_id
value from the header, then it finds the session that corresponds to
that id and sends the data to it for processing. If there's no such
session, an error is printed and it's ignored.

For data being sent out, the controller polls each session and sends it
along to the tunnel_driver.

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

Initially, a single session is created. That session, however, might
spawn other sessions. Sessions are identified with a 16-bit id value
that's also sent across the network. The session management code is
implemented in [session.c](/client/controller/session.c).

Each session has a corresponding [driver](#drivers) (different from a
[tunnel_driver](#tunnel_driver)), which is how it interacts with the
real world (the console, an executable, etc).

The session module is where the actual dnscat protocol (see
[protocol.md](protocol.md)) is parsed - the actual parsing is done in
[packet.c](/client/controller/packet.c). It's agnostic to both its back
end (a tunnel_driver) and its front end (just a driver).

When data is received by the tunnel_driver, it's sent to the session via
the [controller](#controller). The packet received by the session is
parsed as a dnscat protocol packet, and it's fed into the session's
state machine. If everything is okay (a good seq/ack, the right type in
the right state, etc), the data portion of the packet, if any, is passed
up to the driver. If there's no data in the received packet, the driver
is still polled to see if any outgoing data is waiting.

Whether or not the driver had data waiting, the session generates a new
packet in the dnscat protocol (with the proper session_id/seq/ack
values). That packet is returned to the session, then the controller,
then the tunnel_driver, where it's wrapped in a DNS Tunneling Protocol
packet and sent as a response.

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

* [driver_console](/client/drivers/driver_console.c) - simply feeds the
  program's stdin/stdout into the session.  Not terribly useful outside
  of testing.  Only one / program is allowed, because stdin is a prude
  (will only talk to one driver at a time).

* [driver_exec](/client/drivers/driver_exec.c) - execute a process (like
  cmd.exe) and attach the process's stdin / stdout to the session.

* [driver_ping](/client/drivers/driver_ping.c) - this is a 'special' driver
  that just handles dnscat2 pings (it simply echoes back what is sent)

* [driver_command](/client/drivers/command/driver_command.c) - this driver has
  its own command packets, which can be used to upload files, download
  files, execute commands, etc - basically, it creates other drivers.

It's pretty trivial to add more, from a programming perspective. But if
the connection is "special" (ie, the data isn't meant to be parsed by
humans, like driver_command, which has its own protocol), it's important
for the server to understand how to handle the connections (that is, the
if it's a command session, the server needs to show the user a menu and
wait for commands). That's done by using flags in the SYN packet.

A driver meant for direct human consumption - like driver_exec, which
just runs a program and sends the output over the session, doesn't
require anything special.

The driver runs in a vacuum - it doesn't know anything else about what
dnscat2 is up to. All it knows is that it's receiving data and getting
polled for its own data. Other than that, it's on its own. It doesn't
know about [sessions](#session) or [controllers](#controller) or
[tunnel_drivers](#tunnel_driver) or anything.

That's the design, anyway, but it isn't a hard and fast rule. There are
some cases where a driver will want to create another session, for
example (which is done by driver_command). As a result, those have to
call functions in [controller](#controller), which breaks the
architecture a bit, but there isn't really an alternative. I used to use
some message passing strategy, but it was horrible.

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

## Networking

All networking is done using [libs/tcp.h](/client/libs/tcp.h),
[libs/udp.h](/client/libs/udp.h), and
[libs/select_group.h](/client/libs/select_group.h). Like all libraries that
dnscat2 uses, all three work and are tested on Windows, Linux, OS X, and
FreeBSD.

The tcp and udp modules provide a simple interface to creating or
destroying IPv4 sockets without having to understand everything about
them. It also uses BSD sockets or Winsock, as appropriate.

The select_group module provides a way to asynchronously handle socket
communication. Essentially, you can create as many sockets as you want
and add them to the select_group module with a callback for receiving
data or being closed (basically, a platform-independent wrapper around
`select`). Then the `main` function (or whatever) calls
`select_group_do_select` with a timeout value in an infinite loop.

Each iteration of the loop, `select` is called, and any sockets that are
active have their appropriate callbacks called. From those callbacks,
the socket can be removed from the select_group (for example, if the
socket needs to be closed).

stdin can also be read by select_group, meaning that special code isn't
required to handle stdin input. On Windows, a secondary thread is
automatically created to poll for activity; see the asynchronous code
section for info.

## Asynchronous code

There is as little threading as possible, and it should stay that way.
Every socket is put into `select` via the select_group library and when
the socket is active an appropriate callback is called by the
select_group module.

As a result, packet-handling functions must be fast. A slow function
will slow down the handling of all other traffic, which isn't great.

There is one place where threads are used, though: on Windows, it isn't
possible to put the stdin handle into `select`. So, transparently, the
select_group module will create a thread that basically just reads stdin
and shoves it into a pipe that can be selected on. You can find all the
code for this in [libs/select_group.h](/client/libs/select_group.h), and
this only affects Windows.

## Memory management

I use sorta weird memory-management techniques that came from a million
years ago when I wrote nbtool: all memory management
(malloc/free/realloc/strdup/etc) takes place in a module called memory,
which can be found in [libs/memory.h](/client/libs/memory.h).

Basically, as a developer, all you need to know is to use `safe_malloc`,
`safe_free`, `safe_strdup`, and other `safe_*` functions defined in
libs/memory.h.

The advantage of this is that in debug mode, these are added to a linked
list of all memory ever allocated. When it's freed, it's marked as such.
When the program terminates cleanly, a list of all currently allocated
memory (including the file and line number where it was allocated) are
displayed, giving a platform-independent way to find memory leaks.

An additional advantage is that, for a small performance hit, the memory
being freed is zeroed out, which makes bugs somewhat easier to find and
stops user-after-free bugs from being exploitable. It will also detect
double-frees and abort automatically.

## Parsing

All parsing (and marshalling and most kinds of string-building) should
be done by the buffer module, found in [libs/buffer.h](/client/libs/buffer.h).

The buffer module harkens back to Battle.net bot development, and is a
safe way to build and parse arbitrary binary strings. I've used more or
less the same code for 10+ years, with only minor changes (the only
major change has been moving to 64-bit lengths all around, and
occasionally adding a datatype).

Essentially, you create a buffer with your chosen byte order (little
endian, big endian, network (aka, big endian) or host:

    buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

Then you can add bytes, integers, null-terminated strings (ntstrings),
byte arrays, and other buffers to it:

    buffer_add_int8(buffer, 1);
    buffer_add_ntstring(buffer, "hello");
    buffer_add_bytes(buffer, (uint8_t*)"hello", 5);

When a buffer is ready to be sent, you can convert it to a byte string
and free it at the same time (you can also do these separately, but I
don't think I ever have):

    size_t length;
    uint8_t *out = buffer_create_string_and_destroy(buffer, &length);
    [...]
    safe_free(out);

You can also create a buffer containing data, such as data that's been
received from a socket:

    buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

This makes a copy of data, so you can free it if necessary. Once it's
created you can read in values:

    uint8_t byte = buffer_read_next_int8(buffer);
    uint16_t word = buffer_read_next_int16(buffer);

You can also read from specific offsets:

    uint32_t dword = buffer_read_int32_at(buffer, 0);
    uint64_t qword = buffer_read_int64_at(buffer, 10);

You can also read an ntstring into a string you allocate:

    char str[128];
    buffer_read_next_ntstring(buffer, str, 128);

Or you can allocate a string automatically:

    char *str = buffer_alloc_next_ntstring(buffer);
    [...]
    safe_free(str);

You can also read at certain indices (buffer_read_int8_at(...), for
example). See [libs/buffer.h](/client/libs/buffer.h) for everything, with
documentation.

Currently the error handling consists basically of aborting and printing
an error when attempting to read out of bounds. See below for more info
on error handling.

(I'm planning on shoring up the error handling in a future release)

## Error handling

I'll just say it: the error handling on the client sucks right now. In
normal use everything's fine, but if you try messing with anything, any
abnormality causes an `abort`.

Right now, the majority of error handling is done by the
[buffer](/client/libs/buffer.h) module. If there's anything wrong with
the protocol (dnscat, dns, etc.), it simply exits.

That's clearly not a great situation for stable code. I plan to go
through and add better error handling in the future, but it's a
non-trivial operation.



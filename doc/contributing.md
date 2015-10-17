# Introduction

So, you want to contribute to dnscat2 but don't know where to start?
Well, this document will help give you some context on how to contribute
and how development happens.

It also describes the layout and logic behind the dnscat2 codebase.
This is primarily intended for developers or perhaps security auditors,
and shouldn't be necessary for end users.

This document complements other documents, such as
[protocol.md](protocol.md).

# How do I contribute?

Contributing is easy! I love getting code submitted from others! Send me
a pull request!

To go into more details...

If you're wondering how to help, check out the [issue
tracker](https://github.com/iagox86/dnscat2/issues). I try to keep that
up to date with the current bugs / requested features. Some of the
things on there are poorly described, because I write them for myself,
but if something catches your eye, feel free to request details!

If you plan to develop something, be sure to take a look at the
[branches](https://github.com/iagox86/dnscat2/branches) to see if
there's currently a branch that's going to conflict with your changes
(especially in the early stages, I'm doing a lot of global refactoring,
so I can easily and unintentionally break your changes).

Being that I (Ron / iagox86) am currently the only developer, there
isn't a mailing list or anything like that set up. However, you're
absolutely welcome to email me any questions you have - I'm very
responsive with email, and I respond to any and all emails that aren't,
for example, asking me to hack your wife. If a few days go by with no
response, feel free to email again with "friendly ping" or something :)

Because it's just me, politics and coding style aren't a huge issue.
Please try to use the same coding style as the surrounding code to make
it easier to read. Also, comment and document generously!

I think that's all you really need to know about contributing. The rest
of this document will be about design decisions and where to find
different pieces!

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

Each session has a corresponding [driver](#driver) (different from a
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

# Server

The server is written in Ruby, and can be found in the server/
directory.

I'm a fan of being fairly verbose when programming, so you'll find my
style of writing Ruby very C-like. I strongly believe in always using
parenthesis, for example.

Other than nitpicky stuff, there aren't a lot of style conventions that
I insist on. I'm still learning the best way to write Ruby, and it's
reflected in the code.

In general, if you're making changes, try to copy the style of the rest
of the file.

I've only really tested the server on Linux. I don't know if it runs on
Linux, OS X, etc, but it *should* run anywhere that supports Ruby.

## Dependencies

At the moment, the only dependency is on Trollop. Trollop is a
command-line parser, and I use it to parse the arguments that the user
entered, as well as commands the users type into the various windows.

## Structure

The main file is server/dnscat2.rb. It processes commandline arguments
(using Trollop), sets up the global settings in the `Settings` class,
and starts up a DNS Tunnel Driver (see below).

Like the client, to understand the structure of the rest of the progra,
it's helpful to understand how the dnscat protocol works - see
[protocol.md](protocol.md) for that.

If you read the section on the client, you'll find that this is very
similar - in fact, I re-use a lot of text. The reason is pretty obvious:
I intentionally structured the client and server similarly.

### tunnel_driver

The actual code that touches the network is called a tunnel_driver, and,
like on the client, is located in tunnel_drivers/. An example of a
tunnel driver - and the only tunnel_driver that exists as of this
writing - is the DNS driver, which is implemented in
[driver_dns.rb](/server/tunnel_drivers/driver_dns.rb). The protocol I
invented for doing packets over DNS (sort of akin to layer 2) is called
the "DNS Tunneling Protocol", and is discussed in detail in
[protocol.md](protocol.md).

The tunnel_driver has no real understanding of the dnscat protocol or of
sessions or 'connections' or anything like that.  The "dnscat protocol"
happens at a higher layer (in sessions), and is tunnel_driver agnostic.
All it does is take data that's embedded in a packet and convert it into
a stream of bytes.

When a tunnel_driver is created, it must create the socket(s) it needs
to listen to incoming traffic. All incoming traffic - regardless of
whether it's part of a session, regardless of which tunnel_driver is
receiving the traffic, etc, all data is sent to the
[controller](#controller).

When data comes in, in any form, it's handed to the controller. When it
does so, the controller can return outbound data.

There can be multiple tunnel drivers running, but only ever a single
controller.

           +---------------+
           | tunnel_driver | (one or more)
           +---------------+

### controller

The controller is the go-between from the
[tunnel_driver](#tunnel_driver) to the [sessions](#session). It's
essentially a session manager - it creates, destroys, and can enumerate
sessions as needed. It's implemented in
[controller.rb](/server/controller/controller.rb)

When data comes in to a `tunnel_driver`, it's decoded and then sent to
the controller. The controller parses it just enough to get the
`session_id` value from the header, then it finds the session that
corresponds to that id and sends the data to it for processing. If
there's no such session, and it's a valid SYN packet, the session is
created.

There's actually one type of packet that doesn't make it up to the
session - `MESSAGE_TYPE_PING`. When the Controller sees a
`MESSAGE_TYPE_PING` request, it immediately returns a
`MESSAGE_TYPE_PING` response. Since a PING isn't part of a session, it
can't be handled as one.

In the future, there will be another message type -
`MESSAGE_TYPE_DOWNLOAD` - that is also handled by the Controller.

Outgoing data is queued up in the sessions. When a message for a
particular session is received, the controller calls a method on the
session and it responds with data it has ready.

The controller knows how to find and talk to sessions, but it doesn't
know anything about the `tunnel_driver` - it just gets messages from it.
In fact, it's possible for the server to receive multiple messages from
multiple different tunnel drivers as part of the same session, and the
controller would never even know.

           +---------------+
           | tunnel_driver | (one or more)
           +---------------+
                   |
                   v
            +------------+
            | controller | (only 1, ever)
            +------------+

### session

A session is akin to a TCP connection. It handles all the state - the
state of the session, the sequence/acknowledgement number, and so on.

Each session comes with a [driver](#driver). The driver is what knows
how to handle incoming/outgoing data - for example, what to display, how
to handle user input, and so on. We'll look at the driver more below.

When a message arrives, the session will parse it and determine if
there's any actual data in the message. The data (if any) is
passed to the driver, and any data the driver is waiting to send out is
returned to the session. The session takes that data, stuffs it into a
dnscat2 packet (when it can), and returns it.

The [session module](/server/controller/session.rb) is where the actual
dnscat protocol (see [protocol.md](protocol.md)) is implemented. The
individual dnscat2 packets is done in
[packet.rb](/server/controller/packet.rb). It's agnostic to both its
back end (a tunnel_driver) and its front end (just a driver with a well
known interface).

The session knows which driver it's using, but has no knowledge of which
other sessions exist or of the controller.

           +---------------+
           | tunnel_driver | (one or more)
           +---------------+
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
              | session | (one per connection)
              +---------+

### drivers

The final part of the structure is drivers, which are stored in drivers/. Each
session has exactly one driver. The driver defines how it interacts with the
outside world (or with the program itself). A driver has the opportunity
to define a sort of "sub-protocol" (think application-level protocol) on
top of dnscat. The console driver (`driver_console`) is simply
text-based - everything is displayed as text. The command driver
(`driver_command`), however, defines its own protocol.

Here is more details about the currently extant drivers:

* [driver_console](/server/drivers/driver_console.rb) - the incoming
  messages are displayed as text, and anything the user types is sent
  back to the server, also as text (encoded in the dnscat protocol, of
  course). This can be used for 'console' programs (where the users can
  type back and forth), but can also be used for, for example, shells.
  The shell runs on the client, and sends its stdin/stdout to the
  server, which simply displays it.

* [driver_command](/server/drivers/driver_command.rb) - this is a
  sub-protocol of the dnscat protocol. It defines a way to send commands
  to the client - such as 'download file' - and to handle the responses
  appropriately. What the user types in are commands, similar to
  meterpreter
  (see [driver_command_commands](/server/drivers/driver_command_commands.rb)).
  What's displayed on the screen is the results of parsing the incoming
  command packets.

* [driver_process](/server/drivers/driver_process.rb) - this is a little
  bit like `driver_console`, with one important distinction: instead of
  simply displaying the incoming traffic on a console, it starts a
  process and sends the process the incoming traffic. The program's
  output is sent back across the wire to the client. This can be used
  for some interesting tunnels, but is ultimately rather dangerous,
  because it potentially compromises the security of the server by
  sending untrusted input to other processes.

The driver runs in a vacuum - it doesn't know anything else about what
dnscat2 is up to. All it knows is that it's receiving data and getting
polled for its own data. Other than that, it's on its own. It doesn't
know about [sessions](#session) or [controllers](#controller) or
[tunnel_drivers](#tunnel_driver) or anything.

           +---------------+
           | tunnel_driver | (one or more)
           +---------------+
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
              | session | (one per connection)
              +---------+
                   |
           (has exactly one)
                   |
                   v
              +--------+
              | driver | (one per session)
              +--------+

## DNS

The original version of dnscat2 (before beta 0.03) used rubydns for all
things DNS. rubydns's backend was abstracted into celluloid-dns, but
when I tried to use celluloid-dns, none of their examples worked and you
had to import a bunch of things in the correct order. It was really a
mess (I think it was fairly new at the time).

So, knowing that I only really need a small subset of DNS functionality
(the same subset as I implemented in the client :) ), I wrote a DNS
library called [DNSer](server/libs/dnser.rb).

DNSer is an asynchronous resolver or server. It runs in its own thread,
and performs all of its actions via process blocks.

Sending a query through DNSer is as simple as:

    DNSer.query("google.com") do |response|
      puts(response)
    end

`response` is an instance of DNSer::Packet. The actual query is done in
a thread, so that block returns immediately. If you're writing a program
just to do a lookup, you can wait on the thread:

    t = DNSer.query("google.com") do |response|
      puts(response)
    end
    t.join()

There are also a bunch of optional parameters you can pass:
* server (default: "8.8.8.8")
* port (default: 53)
* type (default: DNSer::Packet::TYPE_A)
* cls (default: DNSer::Packet::CLS_IN)
* timeout (default: 3)

Creating a DNS server is likewise easy! Create a new instance of the
class to bind the socket (this can throw an exception):

    dnser = DNSer.new("0.0.0.0", 53)

Then set up the block:

    dnser.on_request() do |transaction|
      puts(transaction.questions[0] || "There was no question!")
      transaction.error!(DNSer::Packet::RCODE_NAME_ERROR)
    end

Like queries, it's asynchronous and that function returns immediately.
You can use the `DNSer#wait` method to wait until the listener ends.

When the request comes, it's sent as a transaction. The transaction
contains the request (in `transaction.request`) and a skeleton of the
response (in `transaction.response`).

There are a number of functions (you'll have to look at [the
implementation](server/libs/dnser.rb) for full details), but the
important ones are the functions that have bangs ('!') in their names -
`DNSer::Transaction#reply!`, `DNSer::Transaction#error!, etc.

Functions that end with a bang will send a response to the requester.
Once one of them has been called, any additional attempts to modify or
send the message will result in an exception (although it's still
possible to read values from it).

One mildly interesting function is `DNSer::Transaction#passthrough`,
which sends the request to an upstream server. When the response comes
back, it's automatically sent back to the client. Thus, it behaves like
a recursive DNS server! An optional `Proc` can be passed to
`passthrough` to intercept the response, too.

## SWindow

Before beta0.03, the UI for the server was a bit of a mess, coupled with
the functionality really badly.

After getting sick of the coupling, I decided to take care of it and
wrote SWindow. SWindow tries to simulate a multi-window environment
using only `Readline`, which works okay, but not great. By keeping it
fairly abstract, it'll be trivial to add a NCurses or Web-based
interface down the road.

When SWindow() is included, it immediately starts an input thread,
waiting for user input. When the user presses <enter>, the input is sent
to the active window.

Windows are created by calling SWindow.new(), and passing in a bunch of
parameters (see [the implementation](server/libs/swindow.rb) for full
details).

Windows can be switched to by calling `SWindow#activate`. That prints
the window's history to the screen and starts accepting commands for
that window. Windows can be temporarily 'closed' by calling
`SWindow#deactivate` or permanently closed with `SWindow#close`. Those
are essentially UI things, nothing about the window itself changes other
than being marked as closed and not showing up in lists.

There is also a hierarchy amongst windows - each window can have a
parent and one or more children. In addition to displaying messages to
the window, messages can also be displayed on
child/descendent/parent/ancestor windows. For example:

    window.with({:to_ancestors=>true}) do
      window.puts("Hi")
    end

will display on the window itself, as well as on all of its parents!

When an active window is closed or deactivated, the parent window is
activated.

## Commander

[Commander](server/libs/commander.rb) is a fairly simple command-parsing
engine used to parse commands typed by users into a window. It uses
`Trollop` and `shellwords` behind the scenes.

Classes that need to parse user commands
([controller.rb](server/controller/controller.rb) and
[driver_command.rb](server/drivers/driver_command.rb)) can set up a
bunch of commands. Later, `Commander#feed` can be called with a line
that the user typed, and the appropriate callback with the appropriate
arguments will be called.

## Settings

The [Settings class](server/libs/settings) was written to store settings
for either the program (global settings, stored in `Settings::GLOBAL`)
or for sessions.

The settings class is instantiated by creating a new instance:

    settings = Settings.new()

Then, settings have to be created with default values and parsers and
such:

    settings.create('mysetting',   Settings::TYPE_STRING,  '',   "This is some documentation")
    settings.create('intsetting',  Settings::TYPE_INTEGER, 123,  "This is some documentation")
    settings.create('boolsetting', Settings::TYPE_BOOLEAN, true, "This is some documentation")

Once a setting is created, it can be set and retrieved:

    settings.set('mysetting', '123')

Based on the type, some massaging and error checking are done. For
example, integers are converted to actual integers, and booleans can
understand the strings 'true', 'yes', 'y', etc.

Optionally, a `proc` can be passed in to handle changes:

    settings.create('mysetting',   Settings::TYPE_STRING,  '',   "This is some documentation") do |oldval, newval|
      puts("Changing mysetting from '#{oldval}' to '#{newval}'")
    end

If an exception is thrown in the block, the change isn't cancelled:

    settings.create('intsetting',  Settings::TYPE_INTEGER, 123,  "This is some documentation") do |oldval, newval|
      if(newval < 0 || newval > 1000)
        raise(Settings::ValidationError, "Value has to be between 0 and 1000!")
      end
    end

It's up to the function calling `Settings#set` to handle that exception.

## Parsing and error handling

Parsing is almost entirely done with `String#unpack` and building packets
is almost entirely done with `Array#pack`.

The error handling on the server is designed to be fairly robust (unlike
the client). Parsing is always done in error handling blocks, and thrown
exceptions are handled appropriately (usually by killing the session
cleanly).

Typically, if something goes wrong, raising a DnscatException() is the
safest way to bail out safely.

# Security

As mentioned in the client error handling section, any bad data causes
the client to `abort`. That's obviously not ideal, but at least it's
safe. On the server, bad data is ignored - an exception is triggered and
the connection is closed, if it can't recover.

Some other security concerns:

* Man-in-the-middle: A man-in-the-middle attack is possible, and can
  cause code execution on the client. This has no more defense against
  tampering than TCP has. I may add some signing in the future.

* Server: The server should be completely safe (ie, able to be run on
  trusted infrastructure). The client can't execute code, download
  files, or anything else that would negatively affect the server.

* Server 'process': The server has a --process argument (and 'process'
  setting) that hands any incoming data from clients (who, by
  definition, aren't trusted) to the process. If an insecure process is
  chosen (or a command shell, like '/bin/sh'), it can compromise the
  server's security. Use --process with extreme caution!

* Confidentiality: There is no confidentiality (all data is sent in
  plaintext). I may add some crypto in the future.

* Cloaking: From a network traffic perspective, it's exceedingly obvious that
  it's dnscat. It's also possible to trick a dnscat2 server into revealing
  itself (with a ping). There is no hiding.

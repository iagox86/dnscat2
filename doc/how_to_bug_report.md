Lately, dnscat2 has gotten enough use that I'm getting hard-to-reproduce
bug reports! That's great news for the project, but bad news for me as
the only developer. :)

# Getting started

If you're an expert and you don't mind taking the time out to
troubleshoot it, that's great! The info below will explain how to get
the best debug logs currently available.  There are also a bunch of
documents in this folder about the client and server architecture. Try
[contributing.md](contributing.md),
[client_architecture.md](client_architecture.md), and
[server_architecture.md](server_architecture.md).
[protocol.md](protocol.md) might also come in handy!

If you don't have the time or desire to troubleshoot yourself, that's
cool! Please file a bug on
[Github](https://github.com/iagox86/dnscat2/issues). If you email me,
I'll probably ask you to file it on github.

If you aren't able to file the bug publicly, perhaps because of private
IP addresses, you can email me directly - ron-at-skullsecurity-dot-net.

For most bugs, full client and server logs are super helpful. There's
information below on how to get them. If it's a network issue, then I'd
love to have a pcap, and it's a segmentation fault, I could use a
backtrace. All information on how to do that is below!

The examples below disable encryption for easier troubleshooting. If the
problem goes away, or you suspect it's a problem with encryption, then
don't set `--security=open` on the server or `--no-encryption` on the
client!

## Getting server logs...

On the server, there are a few helpful options:

* Log *everything* to stdout: `--firehose`
* Display detailed packet information: `--packet-trace`
* Temporarily allow unencrypted connections: `--security=open`
* Enable debug mode for ruby: `ruby -d [...]`

Here's the command:

    # ruby -d dnscat2.rb --packet-trace --firehose --security=open [other options]

You can also redirect it into a file, though running commands might be
tricky:

    # ruby -d dnscat2.rb --packet-trace --firehose --security=open [other options] 2>&1 | tee server-logfile.txt

## Getting client logs...

Basically, you want to create a debug build, then run it with the
following additional options:

* Extra debug output: `-d`
* Display packet information: `--packet-trace`
* Disable encryption: `--no-encryption`

Here are the commands:

    $ make clean debug
    $ ./dnscat -d --packet-trace --no-encryption [other options]

You can also redirect it into a file:

    $ ./dnscat -d --packet-trace --no-encryption [other options] 2>&1 | tee client-logfile.txt

## Getting a pcap

If your problem is with the real transport (like UDP) - typically, that
means if the server sees nothing or just errors - then a pcap is
helpful!

You can run the command the same way as earlier, ensuring that the
server uses `--security=open` and the client uses `--no-encryption`,
then run a packet capture.

I find it helpful to manually set a less-common but legit DNS server
manually, such as 4.2.2.5 or 8.8.4.4, then to filter on those:

    # tcpdump -s0 -w dnscat2-traffic.pcap host 4.2.2.5

That should generate dnscat2-traffic.pcap (depending on your OS, it
might be put somewhere weird; Gentoo runs tcpdump in a chroot
environment by default).

## Segmentation faults

If you have a reproduceable segmentation fault on the client, please try
to send me a corefile:

    $ ulimit -c unlimited
    $ make clean debug
    $ ./dnscat2 [options]

That should generate some sort of corefile you can send me.

If you're unable to get a corefile, or uncomfortable sharing one, then a
backtrace and registers at a minimum would be helpful:

    $ make clean debug
    $ gdb --args ./dnscat2 [options]

When the process crashes:

    (gdb) bt
    [...]
    (gdb) info reg
    [...]

And send along the information!

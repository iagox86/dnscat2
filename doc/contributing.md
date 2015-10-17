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

# Architecture

This section became too long to fit into a single doc, so I decided to
split out the documentation for the client and the server"

* [Client architecture](client_architecture.md)
* [Server architecture](server_architecture.md)

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

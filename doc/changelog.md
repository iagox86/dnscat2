# 0.04

* Added encryption, and made all connections encrypted by default
* Some other minor UI or code cleanup changes

# 0.03

* Re-wrote large parts of the server into way cleaner code
* Significantly updated the documentation for the server
* Removed reliance from rubydns, a built-in DNS server is now used for
  everything
* Added a standalone tool, dnslogger.rb
* There is now a "passthrough" option, which will forward any requests
  that dnscat2 doesn't know how to handle to an upstream server
  (somewhat stealthier, maybe?)

# 0.02

* Re-wrote large parts of the client into cleaner code (for example,
removed the entire message.\* code, which was an awful, awful idea)
* When multiple sessions are in progress, it's now "fair" (a message is
sent every 'tick'; each session now takes turns sending out a message,
rather than the oldest sessions blocking out younger ones
* Removed some parameters that nobody will ever use from the
commandline, like --name and --download (though --download may come back
in another form!)
* Changed the way a "tunnel driver" (ie, dns driver) is created on the
commandline - it's now modeled after socat
* The client will no longer transmit forever against a bad server - it
will attempt to retransmit 10 times by default

# 0.01

* Initial release, everything is new!



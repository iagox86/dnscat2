# 0.01

* Initial release, everything is new!

# 0.02

* Re-wrote large parts of the client into cleaner code (for example,
removed the entire message.* code, which was an awful, awful idea)
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


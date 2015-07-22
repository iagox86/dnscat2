##
# dnscat2_server.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# Implements basically the full Dnscat2 protocol. Doesn't care about
# lower-level protocols.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'driver_dns'
require 'driver_tcp'

require 'log'
require 'packet'
require 'session_manager'
require 'settings'
require 'ui'

# Option parsing
require 'trollop'

# version info
NAME = "dnscat2"
VERSION = "0.03"

# Capture log messages during start up - after creating a command session, all
# messages go to it, instead
Log.logging(nil) do |msg|
  $stdout.puts(msg)
end

# Options
opts = Trollop::options do
  version(NAME + " v" + VERSION + " (server)")

  opt :version,   "Get the dnscat version",
    :type => :boolean, :default => false

  opt :dns,       "Start a DNS server",
    :type => :boolean, :default => true
  opt :dnshost,   "The DNS ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on",
    :type => :integer, :default => 53
  opt :passthrough, "If set (not by default), unhandled requests are sent to a real (upstream) DNS server",
    :type => :boolean, :default => false

  opt :tcp,       "Start a TCP server",
    :type => :boolean, :default => true
  opt :tcphost,   "The TCP ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :tcpport,    "The port to listen on",
    :type => :integer, :default => 4444

  opt :debug,     "Min debug level [info, warning, error, fatal]",
    :type => :string,  :default => "warning"

  opt :auto_command,   "Send this to each client that connects",
    :type => :string,  :default => ""
  opt :auto_attach,    "Automatically attach to new sessions",
    :type => :boolean, :default => false
  opt :packet_trace,   "Display incoming/outgoing dnscat packets",
    :type => :boolean,  :default => false
  opt :isn,            "Set the initial sequence number",
    :type => :integer,  :default => nil
end

# Note: This is no longer strictly required, but it gives the user better feedback if
# they use a bad debug level, so I'm keeping it
Log.PRINT(nil, "Setting debug level to: #{opts[:debug].upcase()}")
if(!Log.set_min_level(opts[:debug].upcase()))
   Trollop::die :debug, "level values are: #{Log::LEVELS}"
end

if(opts[:dnsport] < 0 || opts[:dnsport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

if(opts[:tcpport] < 0 || opts[:tcpport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

DriverDNS.passthrough = opts[:passthrough]
# Make a copy of ARGV
domains = [].replace(ARGV)

if(domains.length > 0)
  puts("Handling requests for the following domain(s):")
  puts(domains.join(", "))

  puts()
  puts("Assuming you are the authority, you can run clients like this:")
  puts()
  domains.each do |domain|
    puts("./dnscat2 #{domain}")
  end

  Log.PRINT(nil)
  Log.PRINT(nil, "You can also run a directly-connected client:")
else
  Log.PRINT(nil, "It looks like you didn't give me any domains to recognize!")
  Log.PRINT(nil, "That's cool, though, you can still use a direct connection.")
  Log.PRINT(nil, "Try running this on your client:")
end

Log.PRINT(nil)
Log.PRINT(nil, "./dnscat2 --dns server=<server>")
Log.PRINT(nil)
Log.PRINT(nil, "Of course, you have to figure out <server> yourself! Clients will connect")
Log.PRINT(nil, "directly on UDP port 53 (by default).")
Log.PRINT(nil)

settings = Settings.new()

settings.verify("packet_trace") do |value|
  if(!(value == true || value == false))
    "'packet_trace' has to be either 'true' or 'false'!"
  else
    nil
  end
end

settings.watch("debug") do |old_val, new_val|
  if(Log::LEVELS.index(new_val.upcase).nil?)
    # return
    "Possible values for 'debug': " + Log::LEVELS.join(", ")
  else
    if(old_val.nil?)
      Log.PRINT(nil, "Set debug level to #{new_val}")
    else
      puts("Changed debug from #{old_val} to #{new_val}")
    end
    Log.set_min_level(new_val)

    # return
    nil
  end
end

settings.watch("passthrough") do |old_val, new_val|
  if(!(new_val == true || new_val == false))
    "'passthrough' has to be either 'true' or 'false'!"
  else
    # return
    if(!old_val.nil?)
      puts("Changed 'passthrough' from " + old_val.to_s() + " to " + new_val.to_s() + "!")
    end
    DriverDNS.passthrough = new_val

    # return
    nil
  end
end

settings.watch("isn") do |old_val, new_val|
  Session.debug_set_isn(new_val.to_i)

  puts("Changed the initial sequence number to 0x%04x (note: you probably shouldn't do this unless you're debugging something!)" % new_val)

  nil
end

settings.verify("auto_command") do |value|
  if(value.is_a?(String) || value.is?(nil))
    nil
  else
    "'auto_command' has to be a string!"
  end
end

settings.verify("auto_attach") do |value|
  if(value == true || value == false)
    nil
  else
    "'auto_attach' has to be true or false!"
  end
end

settings.set("auto_command", opts[:auto_command])
settings.set("auto_attach",  opts[:auto_attach])
settings.set("passthrough",  opts[:passthrough])
settings.set("debug",        opts[:debug])
settings.set("packet_trace", opts[:packet_trace])

if(opts[:isn])
  settings.set("isn",          opts[:isn])
end

threads = []
if(opts[:dns])
  threads << Thread.new do
    begin
      Log.PRINT(nil, "Starting DNS server...")
      driver = DriverDNS.new(opts[:dnshost], opts[:dnsport], domains)
      SessionManager.go(driver, settings)
    rescue DnscatException => e
      Log.FATAL(nil, "Protocol exception caught in DNS module:")
      Log.FATAL(nil, e)
    rescue Exception => e
      Log.FATAL(nil, "Exception starting the driver:")
      Log.FATAL(nil, e)

      if(e.to_s =~ /no datagram socket/)
        Log.PRINT(nil, "")
        Log.PRINT(nil, "Translation: Couldn't listen on #{opts[:dnshost]}:#{opts[:dnsport]}")
        Log.PRINT(nil, "(if you're on Linux, you might need to use sudo or rvmsudo)")
      end

      exit
    end
  end
end

# This is simply to give up the thread's timeslice, allowing the driver threads
# a small amount of time to initialize themselves
sleep(0.01)

ui = Ui.new(settings)

# Subscribe the Ui to the important notifications
SessionManager.subscribe(ui)

# Turn off the 'main' logger
Log.reset()

# Get the UI going
ui.go()


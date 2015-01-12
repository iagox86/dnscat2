##
# dnscat2_server.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
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
require 'ui'

# Option parsing
require 'trollop'

Thread::abort_on_exception = true

Log.logging(nil) do |msg|
  puts(msg)
end

# Options
opts = Trollop::options do
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
    :type => :string,  :default => nil
  opt :packet_trace,   "Display incoming/outgoing dnscat packets",
    :type => :boolean,  :default => false
end

Log.WARNING(nil, "Setting debug level to: #{opts[:debug].upcase()}")
if(!Log.set_min_level(opts[:debug].upcase()))
   Trollop::die :debug, "level values are: #{Log::LEVELS}"
end

if(opts[:dnsport] < 0 || opts[:dnsport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

if(opts[:tcpport] < 0 || opts[:tcpport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

passthrough = opts[:passthrough]
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

  Log.WARNING(nil)
  Log.WARNING(nil, "You can also run a directly-connected client:")
else
  Log.WARNING(nil, "It looks like you didn't give me any domains to recognize!")
  Log.WARNING(nil, "That's cool, though, you can still use a direct connection!")
  Log.WARNING(nil, "Try running this on your client:")
end

Log.WARNING(nil)
Log.WARNING(nil, "./dnscat2 --host <server>")
Log.WARNING(nil)
Log.WARNING(nil, "Of course, you have to figure out <server> yourself! Clients will connect")
Log.WARNING(nil, "directly on UDP port 53.")
Log.WARNING(nil)

threads = []
if(opts[:dns])
  threads << Thread.new do
    begin
      Log.WARNING(nil, "Starting DNS server...")
      driver = DriverDNS.new(opts[:dnshost], opts[:dnsport], domains, passthrough)
      SessionManager.go(driver, opts)
    rescue DnscatException => e
      Log.FATAL(nil, "Protocol exception caught in DNS module:")
      Log.FATAL(nil, e)
    rescue Exception => e
      Log.FATAL(nil, "Exception starting the driver:")
      Log.FATAL(nil, e)

      if(e.to_s =~ /no datagram socket/)
        Log.FATAL(nil, "")
        Log.FATAL(nil, "Translation: Couldn't listen on #{opts[:dnshost]}:#{opts[:dnsport]}")
        Log.FATAL(nil, "(if you're on Linux, you might need to use sudo or rvmsudo)")
      end

      exit
    end
  end
end

# This is simply to give up the thread's timeslice, allowing the driver threads
# a small amount of time to initialize themselves
sleep(0.01)

ui = Ui.new(opts)

# Subscribe the Ui to the important notifications
SessionManager.subscribe(ui)

# TODO: Verify that this works, and probably put it somewhere better
ui.set_option("auto_command", opts[:auto_command])

# Turn off the 'main' logger
Log.reset()

# Get the UI going
ui.go()


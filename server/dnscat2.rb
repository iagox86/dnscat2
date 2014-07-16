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

# Options
opts = Trollop::options do
  opt :dns,       "Start a DNS server",
    :type => :boolean, :default => true
  opt :dnshost,   "The DNS ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on",
    :type => :integer, :default => 53
  opt :autodomain,"If set (which is the default), in addition to any domain(s) specified, requests to no particular domain (but prefixed with a dnscat2 marker) will be processed",
    :type => :boolean, :default => true

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

opts[:debug] = opts[:debug].upcase()
if(Log.get_by_name(opts[:debug]).nil?)
  Trollop::die :debug, "level values are: #{Log::LEVELS}"
  return
end

if(opts[:dnsport] < 0 || opts[:dnsport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

if(opts[:tcpport] < 0 || opts[:tcpport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

autodomain = opts[:autodomain]
# Make a copy of ARGV
domains = [].replace(ARGV)

if(autodomain == false && domains.length == 0)
  puts("autodomain is turned off and you didn't provide any DNS names on the")
  puts("commandline!")
  puts()
  puts("You've gotta give me something to detect... try --autodomain or")
  puts("putting domain names at the end of the command")

  exit(0)
end

if(domains.length > 0)
  puts("Handling requests for the following domain(s):")
  puts(domains.join(", "))

  puts()
  puts("Assuming you are the authority, you can run clients like this:")
  puts()
  domains.each do |domain|
    puts("./dnscat2 #{domain}")
  end

  if(autodomain)
    puts()
    puts("You can also run a directly-connected client:")
    puts()
    puts("./dnscat2 --host <server>")
    puts()
    puts("Of course, you have to figure out <server> yourself! Clients will connect")
    puts("directly on UDP port 53.")
    puts()
  end
else
  puts("It looks like you didn't give me any domains to recognize!")
  puts("That's cool, though, you can still use a direct connection!")
  puts("Try running this on your client:")
  puts()
  puts("./dnscat2 --host <server>")
  puts()
  puts("Of course, you have to figure out <server> yourself! Clients will connect")
  puts("directly on UDP port 53.")
end

threads = []
if(opts[:dns])
  threads << Thread.new do
    begin
      Log.WARNING("Starting DNS server...")
      driver = DriverDNS.new(opts[:dnshost], opts[:dnsport], domains, autodomain)
      SessionManager.go(driver)
    rescue DnscatException => e
      Log.ERROR("Protocol exception caught in DNS module:")
      Log.ERROR(e.inspect)
    rescue Exception => e
      puts(e)
      puts(e.backtrace)

      if(e.to_s =~ /no datagram socket/)
        puts()
        puts("Translation: Couldn't listen on #{opts[:dnshost]}:#{opts[:dnsport]}")
        puts("(if you're on Linux, you might need to use sudo or rvmsudo)")
      end

      exit
    end
  end
end

# This is simply to give up the thread's timeslice, allowing the driver threads
# a small amount of time to initialize themselves
sleep(0.01)

ui = Ui.new()

# Subscribe the Ui to the important notifications
SessionManager.subscribe(ui)
Log.subscribe(ui)

ui.set_option("auto_command", opts[:auto_command])
ui.set_option("packet_trace", opts[:packet_trace])
ui.set_option("log_level",    opts[:debug])

ui.go()


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
require 'ui_new'

# Option parsing
require 'trollop'

Thread::abort_on_exception = true

# Subscribe the Ui to the important notifications
SessionManager.subscribe(UiNew)
Log.subscribe(UiNew)

# Options
opts = Trollop::options do
  opt :dns,       "Start a DNS server",
    :type => :boolean, :default => true
  opt :dnshost,   "The DNS ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on",
    :type => :integer, :default => 53
  opt :domain,    "The DNS domain to respond to [regex, but must only match the non-dnscat portion of the string]",
    :type => :string,  :default => "skullseclabs.org"

  opt :tcp,       "Start a TCP server",
    :type => :boolean, :default => true
  opt :tcphost,   "The TCP ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :tcpport,    "The port to listen on",
    :type => :integer, :default => 4444

  opt :debug,     "Min debug level [info, warning, error, fatal]",
    :type => :string,  :default => "warning"

  opt :auto_attach, "If set to 'false', don't auto-attach to clients when no client is specified",
    :type => :boolean, :default => true
  opt :auto_command,   "Send this to each client that connects",
    :type => :string,  :default => nil
  opt :packet_trace,   "Display incoming/outgoing dnscat packets",
    :type => :boolean,  :default => false
  opt :prompt,         "Display a prompt during sessions",
    :type => :boolean,  :default => false
  opt :signals,        "Use to disable signals, which break rvmsudo",
    :type => :boolean,  :default => true
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

threads = []
if(opts[:dns])
  threads << Thread.new do
    begin
      Log.WARNING("Starting DNS server...")
      driver = DriverDNS.new(opts[:dnshost], opts[:dnsport], opts[:domain])
      SessionManager.go(driver)
    rescue DnscatException => e
      Log.ERROR("Protocol exception caught in DNS module:")
      Log.ERROR(e.inspect)
    rescue Exception => e
      puts(e)
      puts(e.backtrace)
    end
  end
end

# TODO: Disabling TCP for now
#if(opts[:tcp])
#  threads << Thread.new do
#    Log.WARNING("Starting DNS server...")
#    driver = DriverDNS.new(opts[:dnshost], opts[:dnsport], opts[:domain])
#    SessionManager.get_instance().go(driver)
#  end
#end


if(threads.length == 0)
  Log.FATAL("No UI was started! Use --dns or --tcp!")
  exit
end

# This is simply to give up the thread's timeslice, allowing the driver threads
# a small amount of time to initialize themselves
sleep(0.01)

UiNew.set_option("auto_attach",  opts[:auto_attach])
UiNew.set_option("auto_command", opts[:auto_command])
UiNew.set_option("packet_trace", opts[:packet_trace])
UiNew.set_option("prompt",       opts[:prompt])
UiNew.set_option("log_level",    opts[:debug])
UiNew.set_option("signals",      opts[:signals])

UiNew.go()


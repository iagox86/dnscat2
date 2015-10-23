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

require 'controller/controller'
require 'libs/command_helpers'
require 'libs/settings'
require 'tunnel_drivers/driver_dns'
require 'tunnel_drivers/driver_tcp'
require 'tunnel_drivers/tunnel_drivers'

# Option parsing
require 'trollop'

# version info
NAME = "dnscat2"
VERSION = "0.03"

# Don't ignore unhandled errors in threads
Thread.abort_on_exception = true

# Options
opts = Trollop::options do
  version(NAME + " v" + VERSION + " (server)")
  banner("You'll almost certainly want to run this in one of a few ways...")
  banner("")
  banner("Default host (0.0.0.0) and port (53), with no specific domain:")
  banner("# ruby dnscat2.rb")
  banner("")
  banner("Default host/port, with a particular domain to listen on:")
  banner("# ruby dnscat2.rb domain.com")
  banner("")
  banner("Or multiple domains:")
  banner("# ruby dnscat2.rb a.com b.com c.com")
  banner("")
  banner("If you need to change the address or port it's listening on, that")
  banner("can be done by passing the --dns argument:")
  banner("# ruby dnscat2.rb --dns 'host=127.0.0.1,port=53531,domain=a.com,domain=b.com'")
  banner("")
  banner("For other options, see below!")
  banner("")

  opt :h,         "Placeholder for help",   :type => :boolean, :default => false
  opt :version,   "Get the dnscat version", :type => :boolean, :default => false

  opt :dns,       "Start a DNS server. Can optionally pass a number of comma-separated name=value pairs (host, port, domain). Eg, '--dns host=0.0.0.0,port=53531,domain=skullseclabs.org' - 'domain' can be passed multiple times",
    :type => :string, :default => nil
  opt :dnshost,   "The DNS ip address to listen on [deprecated]",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on [deprecated]",
    :type => :integer, :default => 53
  opt :passthrough, "Unhandled requests are sent upstream DNS server, host:port",
    :type => :string, :default => ""

  opt :require_enc, "Require all clients to negotiate encryption",
    :type => :boolean, :default => false
  opt :require_auth, "Require all clients using encryption to authorize with a pre-shared secret",
    :type => :boolean, :default => false

  opt :auto_command,   "Send this to each client that connects",
    :type => :string,  :default => ""
  opt :auto_attach,    "Automatically attach to new sessions",
    :type => :boolean, :default => false
  opt :packet_trace,   "Display incoming/outgoing dnscat packets",
    :type => :boolean, :default => false
  opt :process,        "If set, the given process is run for every incoming console/exec session and given stdin/stdout. This has security implications.",
    :type => :string,  :default => nil
  opt :history_size,   "The number of lines of history that windows will maintain",
    :type => :integer, :default => 1000

  opt :firehose,       "If set, all output goes to stdout instead of being put in windows.",
    :type => :boolean, :default => false
end

SWindow.set_firehose(opts[:firehose])

window = SWindow.new(nil, true, { :prompt => "dnscat2> ", :name => "main" })
window.puts("Welcome to dnscat2! Some documentation may be out of date.")
window.puts()

if(opts[:h])
  window.puts("To get help, you have to use --help; I can't find any way to make")
  window.puts("'-h' work on my command parser... :(")
  exit
end

controller = Controller.new(window)

begin
  Settings::GLOBAL.create("packet_trace", Settings::TYPE_BOOLEAN, opts[:packet_trace], "If set to 'true', will open some extra windows that will display incoming/outgoing dnscat2 packets, and also parsed command packets for command sessions.") do |old_val, new_val|
    # We don't have any callbacks
  end

  Settings::GLOBAL.create("passthrough", Settings::TYPE_BLANK_IS_NIL, opts[:passthrough], "Send queries to the given upstream host (note: this can cause weird recursion problems). Expected: 'set passthrough host:port'. Set to blank to disable.") do |old_val, new_val|
    if(new_val.nil?)
      window.puts("passthrough => disabled")

      DriverDNS.set_passthrough(nil, nil)
      next
    end

    host, port = new_val.split(/:/, 2)
    port = port || 53

    DriverDNS.set_passthrough(host, port)
    window.puts("passthrough => #{host}:#{port}")
  end

  Settings::GLOBAL.create("auto_attach", Settings::TYPE_BOOLEAN, opts[:auto_attach], "If true, the UI will automatically open new sessions") do |old_val, new_val|
    window.puts("auto_attach => #{new_val}")
  end

  Settings::GLOBAL.create("auto_command", Settings::TYPE_BLANK_IS_NIL, opts[:auto_command], "The command (or semicolon-separated list of commands) will automatically be executed for each new session as if they were typed at the keyboard.") do |old_val, new_val|
    window.puts("auto_command => #{new_val}")
  end

  Settings::GLOBAL.create("process", Settings::TYPE_BLANK_IS_NIL, opts[:process] || "", "If set, this process is spawned for each new console session ('--console' on the client), and it handles the session instead of getting the i/o from the keyboard.") do |old_val, new_val|
    window.puts("process => #{new_val}")
  end

  Settings::GLOBAL.create("history_size", Settings::TYPE_INTEGER, opts[:history_size], "Change the number of lines to store in the new windows' histories") do |old_val, new_val|
    SWindow.history_size = new_val
    window.puts("history_size (for new windows) => #{new_val}")
  end

  Settings::GLOBAL.create("require_enc", Settings::TYPE_BOOLEAN, opts[:require_enc], "If true, all new clients *must* use encrypted connections") do |old_val, new_val|
  end

  Settings::GLOBAL.create("require_auth", Settings::TYPE_BOOLEAN, opts[:require_auth], "If true, all new clients using encryption *must* authenticate") do |old_val, new_val|
  end
rescue Settings::ValidationError => e
  window.puts("There was an error with one of your commandline arguments:")
  window.puts(e)
  window.puts()

  Trollop::die("Check your command-line arguments")
end

domains = []
if(opts[:dns])
  begin
    dns_settings = CommandHelpers.parse_setting_string(opts[:dns], { :host => "0.0.0.0", :port => "53", :domains => [], :domain => [] })
    dns_settings[:domains] = dns_settings[:domain] + dns_settings[:domains]
  rescue ArgumentError => e
    window.puts("Sorry, we had trouble parsing your --dns string:")
    window.puts(e)
    exit(1)
  end
elsif(opts[:dnsport] || opts[:dnshost])
  # This way of starting a server is deprecated, technically
  dns_settings = {
    :host => opts[:dnshost],
    :port => opts[:dnsport],
    :domains => [],
  }
end

# Add any domains passed on the commandline
dns_settings[:domains] = dns_settings[:domains] || []
dns_settings[:domains] += ARGV

# Start the DNS driver
TunnelDrivers.start({
  :controller => controller,
  :window     => window,
  :driver     => DriverDNS,
  :args       => [dns_settings[:host], dns_settings[:port], dns_settings[:domains]],
})

# Wait for the input window to finish its thing
SWindow.wait()

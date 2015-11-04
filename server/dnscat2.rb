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

# Create the window right away so other includes can create their own windows if they want
require 'libs/swindow'
WINDOW = SWindow.new(nil, true, { :prompt => "dnscat2> ", :name => "main" })

require 'controller/controller'
require 'libs/command_helpers'
require 'libs/settings'
require 'tunnel_drivers/driver_dns'
require 'tunnel_drivers/driver_tcp'
require 'tunnel_drivers/tunnel_drivers'

# Option parsing
require 'trollop'

require 'securerandom'

# version info
NAME = "dnscat2"
VERSION = "0.04"

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

  opt :security, "Set the security level; 'open' lets the client choose; 'encrypted' requires encryption (default if --secret isn't set); 'authenticated' requires encryption and authentication (default if --secret is set)",
    :type => :string, :default => nil
  opt :secret, "A pre-shared secret, passed to both the client and server to prevent man-in-the-middle attacks",
    :type => :string, :default => nil

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

if(opts[:security].nil?)
  if(opts[:secret].nil?)
    opts[:security] = 'encrypted'
  else
    opts[:security] = 'authenticated'
  end
end

if(opts[:secret].nil?)
  opts[:secret] = SecureRandom::hex(16)
end

WINDOW.puts("Welcome to dnscat2! Some documentation may be out of date.")
WINDOW.puts()

if(opts[:h])
  WINDOW.puts("To get help, you have to use --help; I can't find any way to make")
  WINDOW.puts("'-h' work on my command parser... :(")
  exit
end

controller = Controller.new()

begin
  Settings::GLOBAL.create("packet_trace", Settings::TYPE_BOOLEAN, opts[:packet_trace], "If set to 'true', will open some extra windows that will display incoming/outgoing dnscat2 packets, and also parsed command packets for command sessions.") do |old_val, new_val|
    # We don't have any callbacks
  end

  Settings::GLOBAL.create("passthrough", Settings::TYPE_BLANK_IS_NIL, opts[:passthrough], "Send queries to the given upstream host (note: this can cause weird recursion problems). Expected: 'set passthrough host:port'. Set to blank to disable.") do |old_val, new_val|
    if(new_val.nil?)
      WINDOW.puts("passthrough => disabled")

      DriverDNS.set_passthrough(nil, nil)
      next
    end

    host, port = new_val.split(/:/, 2)
    port = port || 53

    DriverDNS.set_passthrough(host, port)
    WINDOW.puts("passthrough => #{host}:#{port}")
  end

  Settings::GLOBAL.create("auto_attach", Settings::TYPE_BOOLEAN, opts[:auto_attach], "If true, the UI will automatically open new sessions") do |old_val, new_val|
    WINDOW.puts("auto_attach => #{new_val}")
  end

  Settings::GLOBAL.create("auto_command", Settings::TYPE_BLANK_IS_NIL, opts[:auto_command], "The command (or semicolon-separated list of commands) will automatically be executed for each new session as if they were typed at the keyboard.") do |old_val, new_val|
    WINDOW.puts("auto_command => #{new_val}")
  end

  Settings::GLOBAL.create("process", Settings::TYPE_BLANK_IS_NIL, opts[:process] || "", "If set, this process is spawned for each new console session ('--console' on the client), and it handles the session instead of getting the i/o from the keyboard.") do |old_val, new_val|
    WINDOW.puts("process => #{new_val}")
  end

  Settings::GLOBAL.create("history_size", Settings::TYPE_INTEGER, opts[:history_size], "Change the number of lines to store in the new windows' histories") do |old_val, new_val|
    SWindow.history_size = new_val
    WINDOW.puts("history_size (for new windows) => #{new_val}")
  end

  Settings::GLOBAL.create("security", Settings::TYPE_STRING, opts[:security], "Options: 'open' (let the client decide), 'encrypted' (require clients to encrypt), 'authenticated' (require clients to authenticate)") do |old_val, new_val|
    options = {
      'open'          => "Client can decide on security level",
      'encrypted'     => "All connections must be encrypted",
      'authenticated' => "All connections must be encrypted and authenticated",
    }

    new_val_str = options[new_val]

    if(!new_val_str)
      raise(Settings::ValidationError, "Valid options for security: #{options.keys.map() { |value| "'#{value}'" }.join(', ')}")
    end

    WINDOW.puts("Security policy changed: #{new_val_str}")
  end

  Settings::GLOBAL.create("secret", Settings::TYPE_STRING, opts[:secret], "Pass the same --secret value to the client and the server for extra security") do |old_val, new_val|
  end
rescue Settings::ValidationError => e
  WINDOW.puts("There was an error with one of your commandline arguments:")
  WINDOW.puts(e)
  WINDOW.puts()

  Trollop::die("Check your command-line arguments")
end

domains = []
if(opts[:dns])
  begin
    dns_settings = CommandHelpers.parse_setting_string(opts[:dns], { :host => "0.0.0.0", :port => "53", :domains => [], :domain => [] })
    dns_settings[:domains] = dns_settings[:domain] + dns_settings[:domains]
  rescue ArgumentError => e
    WINDOW.puts("Sorry, we had trouble parsing your --dns string:")
    WINDOW.puts(e)
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
  :driver     => DriverDNS,
  :args       => [dns_settings[:host], dns_settings[:port], dns_settings[:domains]],
})

# Wait for the input window to finish its thing
SWindow.wait()

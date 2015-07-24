##
# dnslogger.rb
# Created July 22, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Implements a stupidly simple DNS server.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'trollop'
require 'rubydns'

# version info
NAME = "dnslogger"
VERSION = "v1.0.0"

# Options
opts = Trollop::options do
  version(NAME + " " + VERSION)

  opt :version, "Get the #{NAME} version",      :type => :boolean, :default => false
  opt :host,    "The ip address to listen on",  :type => :string,  :default => "0.0.0.0"
  opt :port,    "The port to listen on",        :type => :integer, :default => 53

  opt :passthrough,   "If enabled, forward DNS requests upstream",        :type => :boolean, :default => false
  opt :upstream,      "The upstream DNS server to use for --passthrough", :type => :string,  :default => "8.8.8.8"
  opt :upstream_port, "The port to use for upstream requests",            :type => :integer, :default => 53

  opt :A,       "Response to send back for 'A' requests",     :type => :string,  :default => nil
  opt :AAAA,    "Response to send back for 'AAAA' requests",  :type => :string,  :default => nil
  opt :CNAME,   "Response to send back for 'CNAME' requests", :type => :string,  :default => nil
  opt :TXT,     "Response to send back for 'TXT' requests",   :type => :string,  :default => nil
  opt :MX,      "Response to send back for 'MX' requests",    :type => :string,  :default => nil
  opt :MX_PREF, "The preference order for the MX record",     :type => :integer, :default => 10

  opt :ttl, "The TTL value to return", :type => :integer, :default => 60
end

if(opts[:port] < 0 || opts[:port] > 65535)
  Trollop::die :port, "must be a valid port (between 0 and 65535)"
end

puts("#{NAME} #{VERSION} is starting!")

# Use upstream DNS for name resolution.
UPSTREAM = RubyDNS::Resolver.new([[:udp, opts[:upstream], opts[:upstream_port]]])

# Get a handle to these things
Name = Resolv::DNS::Name
IN = Resolv::DNS::Resource::IN

puts(nil, "Starting #{NAME} DNS server on #{opts[:host]}:#{opts[:port]}")

interfaces = [
  [:udp, opts[:host], opts[:port]],
]

RubyDNS::run_server(:listen => interfaces) do |s|
  # Turn off DNS logging
  s.logger.level = Logger::WARN

  # Capture all requests
  otherwise do |transaction|
    name = transaction.name
    type = transaction.resource_class.name.gsub(/.*::/, '')

    # If they provided a way to handle it, to that
    if(opts[type.to_sym])
      if(transaction.resource_class == IN::MX)
        puts("Got a request for #{name.to_s} [type = #{type}], responding with #{opts[:MX_PREF]} #{opts[:MX]}")
        transaction.respond!(opts[:MX_PREF], Name.create(opts[:MX]))
      elsif(transaction.resource_class == IN::A || transaction.resource_class == IN::AAAA || transaction.resource_class == IN::TXT)
        puts("Got a request for #{name.to_s} [type = #{type}], responding with #{opts[type.to_sym]}")
        transaction.respond!(opts[type.to_sym])
      elsif(transaction.resource_class == IN::CNAME)
        puts("Got a request for #{name.to_s} [type = #{type}], responding with #{opts[type.to_sym]}")
        transaction.respond!(Name.create(opts[type.to_sym]))
      else
        puts("Got a request for #{name.to_s} [type = #{type}], and thought I could handle it, but can't. Please report this on github.com/iagox86/dnscat2/issues")
        transaction.fail!(:NXDomain)
      end

    # If they didn't provide a response, but they requested upstream, do that
    elsif(opts[:passthrough])
      puts("Got a request for #{name.to_s} [type = #{type}], sending to #{opts[:upstream]}:#{opts[:upstream_port]}")
      transaction.passthrough!(UPSTREAM)

    # Otherwise, just fail the request
    else
      puts("Got a request for #{name.to_s} [type = #{type}], responding with NXDomain")
      transaction.fail!(:NXDomain)
    end

    transaction
  end
end

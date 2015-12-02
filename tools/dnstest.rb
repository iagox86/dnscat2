##
# dnslogger.rb
# Created July 22, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Simply checks if you're the authoritative server.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'trollop'
require '../server/libs/dnser'

# version info
NAME = "dnstest"
VERSION = "v1.0.0"

Thread.abort_on_exception = true

# Options
opts = Trollop::options do
  version(NAME + " " + VERSION)

  opt :version, "Get the #{NAME} version",                             :type => :boolean, :default => false
  opt :host,    "The ip address to listen on",                         :type => :string,  :default => "0.0.0.0"
  opt :port,    "The port to listen on",                               :type => :integer, :default => 53
  opt :domain,  "The domain to check",                                 :type => :string,  :default => nil,      :required => true
  opt :timeout, "The amount of time (seconds) to wait for a response", :type => :integer, :default => 10
end

if(opts[:port] < 0 || opts[:port] > 65535)
  Trollop::die :port, "must be a valid port (between 0 and 65535)"
end

if(opts[:domain].nil?)
  Trollop::die :domain, "Domain is required!"
end

puts("Starting #{NAME} #{VERSION} DNS server on #{opts[:host]}:#{opts[:port]}")

domain = (0...16).map { ('a'..'z').to_a[rand(26)] }.join() + "." + opts[:domain]

dnser = DNSer.new(opts[:host], opts[:port])

dnser.on_request() do |transaction|
  request = transaction.request

  if(request.questions.length < 1)
    puts("The request didn't ask any questions!")
    next
  end

  if(request.questions.length > 1)
    puts("The request asked multiple questions! This is super unusual, if you can reproduce, please report!")
    next
  end

  question = request.questions[0]
  puts("Received: #{question}")
  if(question.type == DNSer::Packet::TYPE_A && question.name == domain)
    puts("You have the authoritative server!")
    transaction.error!(DNSer::Packet::RCODE_NAME_ERROR)
    exit()
  else
    puts("Received a different request: #{question}")
  end

  # Always respond with an error
  transaction.error!(DNSer::Packet::RCODE_NAME_ERROR)
end

puts("Sending: #{domain}!")
DNSer.query(domain, { :type => DNSer::Packet::TYPE_A }) do |response|
  # Do nothing
end

sleep(opts[:timeout])

puts("Request timed out... you probably don't have the authoritative server. :(")
exit(0)

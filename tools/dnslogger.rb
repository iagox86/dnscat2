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
require '../server/libs/dnser'

# version info
NAME = "dnslogger"
VERSION = "v1.0.0"

Thread.abort_on_exception = true

# Options
opts = Trollop::options do
  version(NAME + " " + VERSION)

  opt :version, "Get the #{NAME} version",      :type => :boolean, :default => false
  opt :host,    "The ip address to listen on",  :type => :string,  :default => "0.0.0.0"
  opt :port,    "The port to listen on",        :type => :integer, :default => 53

  opt :passthrough,   "Set to a host:port, and unanswered queries will be sent there", :type => :string, :default => nil
  opt :packet_trace,  "If enabled, print details about the packets",                   :type => :boolean, :default => false

  opt :A,       "Response to send back for 'A' requests",     :type => :string,  :default => nil
  opt :AAAA,    "Response to send back for 'AAAA' requests",  :type => :string,  :default => nil
  opt :CNAME,   "Response to send back for 'CNAME' requests", :type => :string,  :default => nil
  opt :TXT,     "Response to send back for 'TXT' requests",   :type => :string,  :default => nil
  opt :MX,      "Response to send back for 'MX' requests",    :type => :string,  :default => nil
  opt :MX_PREF, "The preference order for the MX record",     :type => :integer, :default => 10
  opt :NS,      "Response to send back for 'NS' requests",    :type => :string,  :default => nil

  opt :ttl, "The TTL value to return", :type => :integer, :default => 60
end

if(opts[:port] < 0 || opts[:port] > 65535)
  Trollop::die :port, "must be a valid port (between 0 and 65535)"
end

puts("Starting #{NAME} #{VERSION} DNS server on #{opts[:host]}:#{opts[:port]}")

pt_host = pt_port = nil
if(opts[:passthrough])
  pt_host, pt_port = opts[:passthrough].split(/:/, 2)
  pt_port = pt_port || 53
  puts("Any queries without a specific answer will be sent to #{pt_host}:#{pt_port}")
end

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

  puts(request.to_s(!opts[:packet_trace]))

  # If they provided a way to handle it, to that
  response = question.type_s ? opts[question.type_s.to_sym] : nil
  if(response)
    if(question.type == DNSer::Packet::TYPE_MX)
      answer = question.answer(opts[:ttl], response, opts[:MX_PREF])
    else
      answer = question.answer(opts[:ttl], response)
    end

    transaction.add_answer(answer)
    puts(transaction.response.to_s(!opts[:packet_trace]))
    transaction.reply!()
  else
    if(pt_host)
      transaction.passthrough!(pt_host, pt_port, Proc.new() do |packet|
        puts(packet.to_s(!opts[:packet_trace]))
      end)
      puts("OUT: (...forwarding upstream...)")
    else
      transaction.error!(DNSer::Packet::RCODE_NAME_ERROR)
      puts(transaction.response.to_s(!opts[:packet_trace]))
    end
  end

  if(!transaction.sent)
    raise(StandardError, "Oops! We didn't send the response! Please file a bug")
  end

end

# Wait for it to finish (never-ending, essentially)
dnser.wait()

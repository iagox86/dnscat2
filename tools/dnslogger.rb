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

#  opt :passthrough,   "If enabled, forward DNS requests upstream",        :type => :boolean, :default => false
#  opt :upstream,      "The upstream DNS server to use for --passthrough", :type => :string,  :default => "8.8.8.8"
#  opt :upstream_port, "The port to use for upstream requests",            :type => :integer, :default => 53
  opt :packet_trace,  "If enabled, print details about the packets",      :type => :boolean, :default => false

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
  puts("IN:  #{question}")
  if(opts[:packet_trace])
    puts("Received: #{request}")
  end

  # If they provided a way to handle it, to that
  response = opts[question.type_s.to_sym]
  if(response)
    if(question.type == DNSer::Packet::TYPE_MX)
      answer = question.answer(opts[:ttl], response, opts[:MX_PREF])
    else
      answer = question.answer(opts[:ttl], response)
    end
    transaction.add_answer(answer)
    puts("OUT: #{answer}")
  elsif(question.type == DNSer::Packet::TYPE_ANY)
    if(opts[:A])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_A, question.cls, opts[:ttl], DNSer::Packet::A.new(opts[:A])))
    end
    if(opts[:AAAA])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_AAAA, question.cls, opts[:ttl], DNSer::Packet::AAAA.new(opts[:AAAA])))
    end
    if(opts[:CNAME])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_CNAME, question.cls, opts[:ttl], DNSer::Packet::CNAME.new(opts[:CNAME])))
    end
    if(opts[:TXT])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_TXT, question.cls, opts[:ttl], DNSer::Packet::TXT.new(opts[:TXT])))
    end
    if(opts[:MX])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_MX, question.cls, opts[:ttl], DNSer::Packet::MX.new(opts[:MX], opts[:MX_PREF])))
    end
    if(opts[:NS])
      transaction.add_answer(DNSer::Packet::Answer.new(question.name, DNSer::Packet::TYPE_NS, question.cls, opts[:ttl], DNSer::Packet::NS.new(opts[:NS])))
    end
  else
    transaction.error(DNSer::Packet::RCODE_NAME_ERROR)
    puts("OUT: NXDomain (domain not found)")
  end

  if(opts[:packet_trace])
    puts("Sent: #{transaction.response}")
  end

  transaction.reply!()
end

# Wait for it to finish (never-ending, essentially)
dnser.wait()

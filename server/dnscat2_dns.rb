##
# dnscat2_dns.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# The DNS dnscat server.
##
$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9
$LOAD_PATH << File.dirname(__FILE__) + '/rubydns/lib'

#require 'rubydns/lib/rubydns'
require 'rubydns'

require 'dnscat2_server'
require 'log'



class DnscatDNS
  IN = Resolv::DNS::Resource::IN

  def max_packet_size()
    return 32 # TODO: do this better
  end

  def initialize()
  end

  # I have to re-implement RubyDNS::start_server() in order to disable the
  # annoying logging (I'd love to have a better way!)
  def start_dns_server(options = {}, &block)
    server = RubyDNS::Server.new(&block)
    server.logger.level = Logger::FATAL
    server.logger.info "Starting RubyDNS server (v#{RubyDNS::VERSION})..."

    options[:listen] ||= [[:udp, "0.0.0.0", 53], [:tcp, "0.0.0.0", 53]]

    EventMachine.run do
      server.fire(:setup)

      # Setup server sockets
      options[:listen].each do |spec|
        server.logger.info "Listening on #{spec.join(':')}"
        if spec[0] == :udp
          EventMachine.open_datagram_socket(spec[1], spec[2], RubyDNS::UDPHandler, server)
        elsif spec[0] == :tcp
          EventMachine.start_server(spec[1], spec[2], RubyDNS::TCPHandler, server)
        end
      end

      server.fire(:start)
    end

    server.fire(:stop)
  end

  def recv()
    start_dns_server() do
      match(/.*/, IN::TXT) do |transaction|
        puts("Transaction:")
        puts(transaction.inspect)
        transaction.respond!("hello")
      end
    end
  end

  def close()
    @s.close
  end
end

begin
  driver = DnscatDNS.new()
  Dnscat2.go(driver)
rescue IOError => e
  puts("IOError caught: #{e.inspect}")
  puts(e.inspect)
  puts(e.backtrace)
rescue Exception => e
  puts("Exception caught: #{e.inspect}")
  puts(e.inspect)
  puts(e.backtrace)
end


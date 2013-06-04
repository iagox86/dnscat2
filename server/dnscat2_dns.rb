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

# TODO: I think I can get rid of this
$LOAD_PATH << File.dirname(__FILE__) + '/rubydns/lib'

#require 'rubydns/lib/rubydns'
require 'rubydns'

require 'log'

class DnscatDNS
  IN = Resolv::DNS::Resource::IN

  def max_packet_size()
    return 20 # TODO: do this better
  end

  def initialize(host, port, domain)
    Log.WARNING "Starting Dnscat2 DNS server on #{host}:#{port} [domain = #{domain}]..."

    @host   = host
    @port   = port
    @domain = domain
  end

  # I have to re-implement RubyDNS::start_server() in order to disable the
  # annoying logging (I'd love to have a better way!)
  def start_dns_server(options = {}, &block)
    server = RubyDNS::Server.new(&block)
    server.logger.level = Logger::FATAL

    options[:listen] ||= [[:udp, @host, @port], [:tcp, @host, @port]]
    #options[:listen] ||= [[:tcp, "0.0.0.0", 5353], [:udp, "0.0.0.0", 5353]]

    EventMachine.run do
      server.fire(:setup)

      # Setup server sockets
      options[:listen].each do |spec|
        Log::INFO("Listening on #{spec.join(':')}")
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
      match(/\.skullseclabs.org$/, IN::TXT) do |transaction| # TODO: Make this a variable
        Log.INFO("Received: #{transaction.name}")
        name = transaction.name.gsub(/\.skullseclabs.org$/, '')
        name = name.gsub(/\./, '')
        name = [name].pack("H*")
        response = yield(name)
        if(response.nil?)
          Log.INFO("Sending nil response...")
          response = domain # TODO: Use the original domain
        else
          response = "#{response.unpack("H*").pop}.skullseclabs.org"
        end
        Log.INFO("Sending:  #{response}")
        transaction.respond!(response)
      end
    end
  end

  def close()
    @s.close
  end

  def DnscatDNS.go(host, port, domain)
    driver = DnscatDNS.new(host, port, domain)
    Dnscat2.go(driver)
  end
end



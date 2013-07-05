##
# driver_dns.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# The DNS dnscat server.
##

require 'rubydns'
require 'log'

IN = Resolv::DNS::Resource::IN

class DriverDNS
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

  MAX_TXT_LENGTH = 255 # The max value that can be expressed by a single byte
  MAX_A_RECORDS = 20   # A nice number that shouldn't cause a TCP switch
  MAX_A_LENGTH = (MAX_A_RECORDS * 4) - 1 # Minus one because it's a length prefixed value

  def recv()
    # Save the domain locally so the block can see it
    domain = @domain

    start_dns_server() do
      match(/#{domain}$/, IN::TXT) do |transaction|
        begin
          Log.INFO("Received: #{transaction.name}")

          name = transaction.name.gsub(/\.(#{domain})$/, '')
          domain = $1 # Save the actual domain (in case they used a regex)
          name = name.gsub(/\./, '')
          name = [name].pack("H*")

          response = yield(name, MAX_TXT_LENGTH / 2)

          if(response.nil?)
            Log.INFO("Sending nil response...")
            response = ''
          else
            response = "#{response.unpack("H*").pop}"
          end
          Log.INFO("Sending:  #{response}")
          transaction.respond!(response)
        rescue SystemExit
          exit
        rescue DnscatException => e
          Log.ERROR("Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.ERROR(e.inspect)
        rescue Exception => e
          Log.FATAL("Fatal exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.FATAL(e.inspect)
          Log.FATAL(e.backtrace)
          exit
        end

        transaction # Return this, effectively
      end

      match(/(#{domain})$/, IN::A) do |transaction|
        begin
          Log.INFO("Received: #{transaction.name}")

          # Parse the name
          name = transaction.name.gsub(/\.(#{domain})$/, '')
          domain = $1 # Save the actual domain (in case they used a regex)
          name = name.gsub(/\./, '')
          name = [name].pack("H*")

          # Get the response
          response = yield(name, MAX_A_LENGTH)

          # Prepend the length
          response = [response.length].pack("C") + response

          # Loop through each 4-byte chunk
          response.bytes.each_slice(4) do |slice|
            # Make sure it's exactly 4 bytes long (to make life easy)
            while(slice.length < 4)
              slice << rand(255)
            end

            # Create the IP address
            name = "%d.%d.%d.%d" % slice

            # Add it to the response
            transaction.respond!(name)
          end
        rescue SystemExit
          exit
        rescue DnscatException => e
          Log.ERROR("Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.ERROR(e.inspect)
        rescue Exception => e
          Log.FATAL("Fatal exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.FATAL(e.inspect)
          Log.FATAL(e.backtrace)
          exit
        end

        transaction # Return this, effectively
      end

      otherwise do |transaction|
        Log.ERROR("Unable to handle request: #{transaction}")
      end
    end
  end

  def close()
    @s.close
  end

  def DriverDNS.go(host, port, domain)
    Log.WARNING("Starting DNS server...")
    driver = DriverDNS.new(host, port, domain)
    Dnscat2.go(driver)
  end
end



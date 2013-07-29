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

IN   = Resolv::DNS::Resource::IN
Name = Resolv::DNS::Name

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
  MAX_MX_LENGTH = 250

  def DriverDNS.parse_name(name, domain)
    Log.INFO("Parsing: #{name}")

    # Break out the name and domain
    if(name.match(/^(.*)\.(#{domain})$/))
      name = $1
      domain = $2

      # Remove the periods from the name
      name = name.gsub(/\./, '')

      # Convert the name from hex to binary
      # TODO: Perhaps we can use other encodings?
      name = [name].pack("H*")

      return name, domain
    end

    raise(DnscatException, "Name didn't contain the expected dnscat values: #{name}")
  end

  def recv()
    # Save the domain locally so the block can see it
    domain = @domain

    start_dns_server() do
      match(/#{domain}$/, IN::TXT) do |transaction|
        begin
          name, domain = DriverDNS.parse_name(transaction.name, domain)

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
          name, domain = DriverDNS.parse_name(transaction.name, domain)

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

      match(/(#{domain})$/, IN::MX) do |transaction|
        begin
          name, domain = DriverDNS.parse_name(transaction.name, domain)

          # Get the response, be sure to leave room for the domain in the response
          # Divided by 2 because we're encoding in hex
          response = yield(name, (MAX_MX_LENGTH / 2) - domain.length)

          response_name = nil
          if(response.nil?)
            Log.INFO("Sending nil response...")
            response_name = domain
          else
            response = "#{response.unpack("H*").pop}"

            # Add the name in chunks no bigger than 63 characters
            response_name = ""
            response.bytes.each_slice(63) do |slice|
              response_name += slice.pack("C*")
              response_name += "."
            end
            response_name += domain
          end

          transaction.respond!(10, Name.create(response_name))
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



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

class DriverDNS
  # Use upstream DNS for name resolution.
  UPSTREAM = RubyDNS::Resolver.new([[:udp, "8.8.8.8", 53]])

  MAX_TXT_LENGTH = 250 # The max value that can be expressed by a single byte
  MAX_A_RECORDS = 20   # A nice number that shouldn't cause a TCP switch
  MAX_A_LENGTH = (MAX_A_RECORDS * 4) - 1 # Minus one because it's a length prefixed value
  MAX_MX_LENGTH = 250

  def initialize(host, port, domain)
    Log.WARNING "Starting Dnscat2 DNS server on #{host}:#{port} [domain = #{domain}]..."

    @host   = host
    @port   = port
    @domain = domain
  end

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

  Name = Resolv::DNS::Name
  IN = Resolv::DNS::Resource::IN


  def recv()
    # Save the domain locally so the block can see it
    domain = @domain

    interfaces = [
      [:udp, @host, @port],
    ]

    RubyDNS::run_server(:listen => interfaces) do |s|
      s.logger.level = Logger::FATAL

      match(/\.#{domain}$/, IN::TXT) do |transaction|
        begin
          name, domain = DriverDNS.parse_name(transaction.name, domain)

          response = yield(name, MAX_TXT_LENGTH / 2) # TODO: Should be '2'

          if(response.nil?)
            Log.INFO("Sending nil response...")
            response = ''
          else
            response = "#{response.unpack("H*").pop}"
          end
          Log.INFO("Sending:  #{response}")
          transaction.respond!(response)
        rescue DnscatException => e
          Log.ERROR("Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.ERROR(e.inspect)
        end

        transaction # Return this, effectively
      end

      # Default DNS handler
      otherwise do |transaction|
        Log.ERROR("Unable to handle request, passing upstream: #{transaction}")
        transaction.passthrough!(UPSTREAM)
      end
    end
  end

  def close()
    @s.close
  end
end



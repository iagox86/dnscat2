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

  Name = Resolv::DNS::Name
  IN = Resolv::DNS::Resource::IN

  def initialize(host, port, domains, autodomain, passthrough)
    Log::WARNING(nil, "Starting Dnscat2 DNS server on #{host}:#{port} [domains = #{domains.nil? ? "n/a" : domains.join(", ")}]...")
    if(autodomain)
      Log::WARNING(nil, "Will also accept direct queries, if they're tagged properly!")
    end
    if(domains.nil? || domains.length == 0)
      Log::WARNING(nil, "No domains were selected, which means this server will only respond to direct queries")
    end

    @host        = host
    @port        = port
    @domains     = domains
    @autodomain  = autodomain
    @passthrough = passthrough
    @shown_pt    = false
  end


  # If domain is non-nil, match /(.*)\.domain/
  # If domain is nil, match /identifier\.(.*)/
  # If required_prefix is set, it only matches domains that contain that prefix
  #
  # The required prefix has to come first, if it's present
  def DriverDNS.get_domain_regex(domain, identifier, required_prefix = nil)
    if(domain.nil?)
      if(required_prefix.nil?)
        return /^#{identifier}(.*)$/
      else
        return /^#{required_prefix}\.#{identifier}(.*)$/
      end
    else
      if(required_prefix.nil?)
        return /^(.*)\.#{domain}$/
      else
        return /^#{required_prefix}\.(.*)\.#{domain}$/
      end
    end
  end

  def recv()
    # Save the domains locally so the block can see it
    domains     = @domains
    autodomain  = @autodomain
    passthrough = @passthrough

    interfaces = [
      [:udp, @host, @port],
    ]

    RubyDNS::run_server(:listen => interfaces) do |s|
      # Turn off DNS logging
      s.logger.level = Logger::FATAL

      # Loop through the domains and see if any match
      domains.each do |domain|
        domain_regex = /^(.*)\.#{domain}$/

        match(domain_regex, IN::TXT) do |transaction|
          begin
            # Parse the name with a regex
            transaction.name.match(domain_regex)

            # Get the name out and validate it
            name = $1
            if(name.nil?)
              raise(DnscatException, "Regex parsing failed for name: #{transaction.name}")
            end

            # Sanity check the rest
            if(name !~ /^[a-fA-F0-9.]*$/)
              raise(DnscatException, "Name contains illegal characters: #{transaction.name}")
            end

            # Get rid of periods
            name = name.gsub(/\./, '')
            name = [name].pack("H*")

            response = yield(name, MAX_TXT_LENGTH / 2)

            if(response.nil?)
              Log.INFO(nil, "Sending nil response...")
              response = ''
            else
              response = "#{response.unpack("H*").pop}"
            end

            Log.INFO(nil, "Sending:  #{response}")
            transaction.respond!(response)
          rescue DnscatException => e
            Log.ERROR(nil, "Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
            Log.ERROR(nil, e.inspect)
          rescue Exception => e
            Log.ERROR(nil, "Error caught:")
            Log.ERROR(nil, e)
          end

          transaction # Return this, effectively
        end

        match(domain_regex) do |transaction|
          raise(DnscatException, "Received a request for an unhandled DNS type: #{transaction.name}")
        end
      end

      if(autodomain)
        domain_regex = /^dnscat\.(.*)$/

        match(domain_regex, IN::TXT) do |transaction|
          begin
            # Parse the name with a regex
            transaction.name.match(domain_regex)

            # Get the name out and validate it
            name = $1
            if(name.nil?)
              raise(DnscatException, "Regex parsing failed for name: #{transaction.name}")
            end

            # Sanity check the rest
            if(name !~ /^[a-fA-F0-9.]*$/)
              raise(DnscatException, "Name contains illegal characters: #{transaction.name}")
            end

            # Get rid of periods
            name = name.gsub(/\./, '')
            name = [name].pack("H*")

            response = yield(name, MAX_TXT_LENGTH / 2)

            if(response.nil?)
              Log.INFO("Sending nil response...")
              response = ''
            else
              response = "#{response.unpack("H*").pop}"
            end

            Log.INFO(nil, "Sending:  #{response}")
            transaction.respond!(response)
          rescue DnscatException => e
            Log.ERROR(nil, "Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
            Log.ERROR(nil, e)
          rescue Exception => e
            Log.ERROR(nil, "Error caught:")
            Log.ERROR(nil, e)
          end

          transaction # Return this, effectively
        end

        match(domain_regex) do |transaction|
          raise(DnscatException, "Received a request for an unhandled DNS type: #{transaction.name}")
        end
      end

      # Default DNS handler
      otherwise do |transaction|
        if(passthrough)
          if(!@shown_pt)
            Log.WARNING(nil, "Unable to handle request, passing upstream: #{transaction.name}")
            Log.WARNING(nil, "(This will only be shown once)")
          end
          transaction.passthrough!(UPSTREAM)
        elsif(!@shown_pt)
          Log.WARNING(nil, "Unable to handle request, returning an error: #{transaction.name}")
          Log.WARNING(nil, "(If you want to pass to upstream DNS servers, use --passthrough)")
          Log.WARNING(nil, "(This will only be shown once)")
          @shown_pt = true
        end
      end
    end
  end

  def close()
    @s.close
  end
end

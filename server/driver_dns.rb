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
require 'pp' # TODO: Debug

class DriverDNS
  # Use upstream DNS for name resolution.
  UPSTREAM = RubyDNS::Resolver.new([[:udp, "8.8.8.8", 53]])

  Name = Resolv::DNS::Name
  IN = Resolv::DNS::Resource::IN

  MAX_A_RECORDS = 20   # A nice number that shouldn't cause a TCP switch
  MAX_A_LENGTH = (MAX_A_RECORDS * 4) - 1 # Minus one because it's a length prefixed value

  RECORD_TYPES = [IN::TXT, IN::MX, IN::CNAME]

  MAX_LENGTH = {
    IN::TXT   => 250,
    IN::MX    => 250,
    IN::CNAME => 250,
  }

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

  def DriverDNS.figure_out_name(name, domains)
    # Check if it's one of our domains
    domains.each do |domain|
      if(name =~ /^(.*)\.(#{domain})/)
        return $1, $2
      end
    end

    # Check if it starts with dnscat, which is used when
    # the server is unknown
    if(name =~ /^dnscat\.(.*)$/)
      return $1, nil
    end

    # Can't process. :(
    return nil
  end

  def recv()
    # Save the domains locally so the block can see it
    domains     = @domains
    passthrough = @passthrough

    interfaces = [
      [:udp, @host, @port],
    ]

    RubyDNS::run_server(:listen => interfaces) do |s|
      # Turn off DNS logging
      s.logger.level = Logger::FATAL

      # This ugly line basically joins the domains together in a string that looks like:
      # (^dnscat\.|\.skullseclabs.org$)
      domain_regex = "(^dnscat\\.|" + (domains.map { |x| "\\.#{x}$" }).join("|") + ")"

      match(/#{domain_regex}/, RECORD_TYPES) do |transaction|
        begin
          # Log what's going on
          Log.INFO(nil, "Received:  #{transaction.name}")

          # Determine the actual name, without the extra cruft
          name, domain = DriverDNS.figure_out_name(transaction.name, domains)
          if(name.nil?)
            raise(DnscatException, "Failed to parse a matching name (please report this, it shouldn't happen): #{transaction.name}")
          end


          # Determine the type
          type = transaction.resource_class

          # Sanity check the name
          if(name !~ /^[a-fA-F0-9.]*$/)
            raise(DnscatException, "Name contains illegal characters: #{transaction.name}")
          end

          # Figure out the length of the domain based on the record type
          if(type == IN::TXT)
            domain_length = 0
          elsif(type.nil?)
            domain_length = "dnscat.".length
          else
            domain_length = domain.length
          end

          # Figure out the max length of data we can handle
          max_length = (MAX_LENGTH[type] / 2) - domain_length

          # Get rid of periods
          name = name.gsub(/\./, '')
          name = [name].pack("H*")

          response = yield(name, max_length)

          if(response.nil?)
            Log.INFO(nil, "Sending nil response...")
            response = ''
          elsif(response.length > max_length)
            raise(DnscatException, "The handler returned too much data! This shouldn't happen, please report")
          else
            response = "#{response.unpack("H*").pop}"
          end

          # Append domain, if needed
          if(type == IN::TXT)
            # Do nothing
          elsif(domain.nil?)
            response = "dnscat." + response
          else
            response = response + "." + domain
          end

          Log.INFO(nil, "Sending:  #{response}")
          transaction.respond!(response)
        rescue DnscatException => e
          Log.ERROR(nil, "Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
          Log.ERROR(nil, e.inspect)
          transaction.fail!(:NXDomain)
        rescue Exception => e
          Log.ERROR(nil, "Error caught:")
          Log.ERROR(nil, e)
          transaction.fail!(:NXDomain)
        end

        transaction # Return this, effectively
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

          transaction.fail!(:NXDomain)
        end

        transaction
      end
    end
  end

  def close()
    @s.close
  end
end

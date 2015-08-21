##
# driver_dns.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# The DNS dnscat server.
##

require 'rubydns'

require 'log'
require 'pp' # TODO: Debug

class DriverDNS
  # Use upstream DNS for name resolution.
  UPSTREAM = RubyDNS::Resolver.new([[:udp, "8.8.8.8", 53]])

  # Get a handle to these things
  Name = Resolv::DNS::Name
  IN = Resolv::DNS::Resource::IN

  MAX_A_RECORDS = 20   # A nice number that shouldn't cause a TCP switch
  MAX_AAAA_RECORDS = 5

  @@passthrough = false

  RECORD_TYPES = {
    IN::TXT => {
      :requires_domain => false,
      :max_length      => 241, # Carefully chosen
      :requires_hex    => true,
      :requires_name   => false,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop
      end,
    },
    IN::MX => {
      :requires_domain => true,
      :max_length      => 241,
      :requires_hex    => true,
      :requires_name   => true,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop.chars.each_slice(63).map(&:join).join(".")
      end,
    },
    IN::CNAME => {
      :requires_domain => true,
      :max_length      => 241,
      :requires_hex    => true,
      :requires_name   => true,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop.chars.each_slice(63).map(&:join).join(".")
      end,
    },
    IN::A => {
      :requires_domain => false,
      :max_length      => (MAX_A_RECORDS * 4) - 1, # Length-prefixed, since we only have DWORD granularity
      :requires_hex    => false,
      :requires_name   => false,

      # Encode in length-prefixed dotted-decimal notation
      :encoder         => Proc.new() do |name|
        i = 0
        (name.length.chr + name).chars.each_slice(3).map(&:join).map do |ip|
          ip = ip.force_encoding('ASCII-8BIT').ljust(3, "\xFF".force_encoding('ASCII-8BIT'))
          i += 1
          "%d.%d.%d.%d" % ([i] + ip.bytes.to_a) # Return
        end
      end,
    },
    IN::AAAA => {
      :requires_domain => false,
      :max_length      => (MAX_AAAA_RECORDS * 16) - 1, # Length-prefixed, because low granularity
      :requires_hex    => false,
      :requires_name   => false,

      # Encode in length-prefixed IPv6 notation
      :encoder         => Proc.new() do |name|
        i = 0
        (name.length.chr + name).chars.each_slice(15).map(&:join).map do |ip|
          ip = ip.force_encoding('ASCII-8BIT').ljust(15, "\xFF".force_encoding('ASCII-8BIT'))
          i += 1
          ([i] + ip.bytes.to_a).each_slice(2).map do |octet|
            "%04x" % [octet[0] << 8 | octet[1]]
          end.join(":") # return
         end
      end,
    },

  }

  def initialize(host, port, domains)
    Log::PRINT(nil, "Starting Dnscat2 DNS server on #{host}:#{port} [domains = #{(domains.nil? || domains.length == 0) ? "n/a" : domains.join(", ")}]...")
    if(domains.nil? || domains.length == 0)
      Log::PRINT(nil, "No domains were selected, which means this server will only respond to direct queries (using \"--dns server=x.x.x.x,port=yyy\" on the client)")
    else
      Log::PRINT(nil, "Will also accept direct queries (using \"--dns server=x.x.x.x,port=yyy\" on the client)")
    end

    @host        = host
    @port        = port
    @domains     = domains
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
      if(name =~ /^(.*)\.(#{domain})/i)
        return $1, $2
      end
    end

    # Check if it starts with dnscat, which is used when
    # the server is unknown
    if(name =~ /^dnscat\.(.*)$/i)
      return $1, nil
    end

    # Can't process. :(
    return nil
  end

  def DriverDNS.passthrough=(value)
    @@passthrough = value
  end
  def DriverDNS.passthrough()
    return @@passthrough
  end

  def recv()
    # Save the domains locally so the block can see it
    domains     = @domains

    interfaces = [
      [:udp, @host, @port],
    ]

    RubyDNS::run_server(:listen => interfaces) do |s|
      # Turn off DNS logging
      s.logger.level = Logger::WARN

      # This ugly line basically joins the domains together in a string that looks like:
      # (^dnscat\.|\.skullseclabs.org$)
      domain_regex = "(^dnscat\\.|" + (domains.map { |x| "\\.#{x}$" }).join("|") + ")"

      # Only match proper domains with proper record types
      match(/#{domain_regex}/i, RECORD_TYPES.keys) do |transaction|
        begin
          # Determine the type
          type = transaction.resource_class
          type_info = RECORD_TYPES[type]

          # Log what's going on
          Log.INFO(nil, "Received:  #{transaction.name} (#{type})")

          # Determine the actual name, without the extra cruft
          name, domain = DriverDNS.figure_out_name(transaction.name, domains)
          if(name.nil? || name !~ /^[a-fA-F0-9.]*$/)
            if(DriverDNS.passthrough)
              if(!@shown_pt)
                Log.INFO(nil, "Unable to handle request, passing upstream: #{transaction.name}")
                Log.INFO(nil, "(This will only be shown once)")
              end
              transaction.passthrough!(UPSTREAM)
            elsif(!@shown_pt)
              Log.INFO(nil, "Unable to handle request, returning an error: #{transaction.name}")
              Log.INFO(nil, "(If you want to pass to upstream DNS servers, use --passthrough")
              Log.INFO(nil, "or run \"set passthrough=true\")")
              Log.INFO(nil, "(This will only be shown once)")
              @shown_pt = true

              transaction.fail!(:NXDomain)
            end
          else
            if(type.nil? || type_info.nil?)
              raise(DnscatException, "Couldn't figure out how to handle the record type! (please report this, it shouldn't happen): " + type)
            end

            # Get rid of periods in the incoming name
            name = name.gsub(/\./, '')
            name = [name].pack("H*")

            # Figure out the length of the domain based on the record type
            if(type_info[:requires_domain])
              if(domain.nil?)
                domain_length = ("dnscat.").length
              else
                domain_length = domain.length + 1 # +1 for the dot
              end
            else
              domain_length = 0
            end

            # Figure out the max length of data we can handle
            if(type_info[:requires_hex])
              max_length = (type_info[:max_length] / 2) - domain_length
            else
              max_length = (type_info[:max_length]) - domain_length
            end

            # Get the response
            response = yield(name, max_length)

            # Sanity check the response
            if(response.nil?)
              response = ''
            elsif(response.length > max_length)
              raise(DnscatException, "The handler returned too much data! This shouldn't happen, please report")
            end

            # Encode the response as needed
            response = type_info[:encoder].call(response)

            # Append domain, if needed
            if(type_info[:requires_domain])
              if(domain.nil?)
                response = "dnscat." + response
              else
                response = response + "." + domain
              end
            end

            # Do another length sanity check (with the *actual* max length, since everything is encoded now)
            if(response.length > type_info[:max_length])
              raise(DnscatException, "The handler returned too much data (after encoding)! This shouldn't happen, please report")
            end

            # Translate it into a name, if needed
            if(type_info[:requires_name])
              response = Name.create(response)
            end

            # Log the response
            Log.INFO(nil, "Sending:  #{response}")

            # Make sure response is an array (certain types require an array, and it's easier to assume everything is one)
            if(!response.is_a?(Array))
              response = [response]
            end

            # Allow multiple response records
            response.each do |r|
              # MX requires a special response
              if(type == IN::MX)
                transaction.respond!(rand(5) * 10, r)
              else
                transaction.respond!(r)
              end
            end
          end
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
        if(DriverDNS.passthrough)
          if(!@shown_pt)
            Log.INFO(nil, "Unable to handle request, passing upstream: #{transaction.name}")
            Log.INFO(nil, "(This will only be shown once)")
          end
          transaction.passthrough!(UPSTREAM)
        elsif(!@shown_pt)
          Log.INFO(nil, "Unable to handle request, returning an error: #{transaction.name}")
          Log.INFO(nil, "(If you want to pass to upstream DNS servers, use --passthrough)")
          Log.INFO(nil, "(This will only be shown once)")
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

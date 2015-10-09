##
# driver_dns.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# This is a driver that will listen on a DNS port (using lib/dnser.rb) and
# will decode the "DNS tunnel protocol" and pass the resulting data to the
# controller.
##

require 'libs/dnser.rb'

class DriverDNS
  attr_reader :window

  # This is upstream dns
  @@passthrough = nil

  MAX_A_RECORDS = 20   # A nice number that shouldn't cause a TCP switch
  MAX_AAAA_RECORDS = 5

  # This is only used to ensure each packet is unique to avoid caching
  @@id = 0

  RECORD_TYPES = {
    DNSer::Packet::TYPE_TXT => {
      :requires_domain => false,
      :max_length      => 241, # Carefully chosen
      :requires_hex    => true,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop
      end,
    },
    DNSer::Packet::TYPE_MX => {
      :requires_domain => true,
      :max_length      => 241,
      :requires_hex    => true,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop.chars.each_slice(63).map(&:join).join(".")
      end,
    },
    DNSer::Packet::TYPE_CNAME => {
      :requires_domain => true,
      :max_length      => 241,
      :requires_hex    => true,
      :encoder         => Proc.new() do |name|
         name.unpack("H*").pop.chars.each_slice(63).map(&:join).join(".")
      end,
    },
    DNSer::Packet::TYPE_A => {
      :requires_domain => false,
      :max_length      => (MAX_A_RECORDS * 4) - 1, # Length-prefixed, since we only have DWORD granularity
      :requires_hex    => false,

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
    DNSer::Packet::TYPE_AAAA => {
      :requires_domain => false,
      :max_length      => (MAX_AAAA_RECORDS * 16) - 1, # Length-prefixed, because low granularity
      :requires_hex    => false,

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

  def initialize(host, port, domains, parent_window)
    if(domains.nil?)
      domains = []
    end

    @window = SWindow.new(parent_window, false, {
      :id => ('td' + (@@id+=1).to_s()),
      :name => "DNS Tunnel Driver status window for #{host}:#{port} [domains = #{(domains == []) ? "n/a" : domains.join(", ")}]",
      :noinput => true,
    })

    @window.puts_ex("Starting Dnscat2 DNS server on #{host}:#{port}", {:to_parent => true})
    @window.puts_ex("[domains = #{(domains == []) ? "n/a" : domains.join(", ")}]...", {:to_parent=>true})
    @window.puts_ex("", {:to_parent => true})

    if(domains.nil? || domains.length == 0)
      @window.puts_ex("It looks like you didn't give me any domains to recognize!", {:to_parent => true})
      @window.puts_ex("That's cool, though, you can still use direct queries,", {:to_parent => true})
      @window.puts_ex("although those are less stealthy.", {:to_parent => true})
      @window.puts_ex("", {:to_parent => true})
    else
      @window.puts_ex("Assuming you have an authoritative DNS server, you can run", {:to_parent => true})
      @window.puts_ex("the client anywhere with the following:", {:to_parent => true})
      domains.each do |domain|
        @window.puts_ex("  ./dnscat2 #{domain}", {:to_parent => true})
      end
      @window.puts_ex("", {:to_parent => true})
    end

    @window.puts_ex("To talk directly to the server without a domain name, run:", {:to_parent => true})
    @window.puts_ex("  ./dnscat2 --dns server=x.x.x.x,port=yyy", {:to_parent => true})
    @window.puts_ex("", {:to_parent => true})
    @window.puts_ex("Of course, you have to figure out <server> yourself! Clients", {:to_parent => true})
    @window.puts_ex("will connect directly on UDP port 53 (by default).", {:to_parent => true})
    @window.puts_ex("", {:to_parent => true})


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

  def DriverDNS.set_passthrough(host, port)
    if(host.nil?)
      @@passthrough = nil
      return
    end

    @@passthrough = {
      :host => host,
      :port => port,
    }
    @shown_pt = false
  end

  def id()
    return @window.id
  end

  def do_passthrough(request)
    if(!@shown_pt)
      puts("TODO: Passthrough")
      @shown_pt = true
    end
#    if(@@passthrough)
#      if(!@shown_pt)
#        window.puts("Unable to handle request: #{transaction.name}")
#        window.puts("Passing upstream to: #{@@passthrough}")
#        window.puts("(This will only be shown once)")
#        @shown_pt = true
#      end
#      transaction.passthrough!(@@passthrough)
#    elsif(!@shown_pt)
#      window.puts("Unable to handle request, returning an error: #{transaction.name}")
#      window.puts("(If you want to pass to upstream DNS servers, use --passthrough")
#      window.puts("or run \"set passthrough=true\")")
#      window.puts("(This will only be shown once)")
#      @shown_pt = true
#
#      transaction.fail!(:NXDomain)
#    end
  end

  def start()
    DNSer.new(@host, @port) do |request, reply|
      begin
        if(request.questions.length < 1)
          raise(DnscatException, "Received a packet with no questions")
        end

        question = request.questions[0]

        # Log what's going on
        window.puts("Received:  #{question.name} (#{question.type_s})")

        # Determine the actual name, without the extra cruft
        name, domain = DriverDNS.figure_out_name(question.name, @domains)
        if(name.nil? || name !~ /^[a-fA-F0-9.]*$/)
          do_passthrough(request)
          next
        end

        # Get rid of periods in the incoming name
        name = name.gsub(/\./, '')
        name = [name].pack("H*")

        type_info = RECORD_TYPES[question.type]
        if(type_info.nil?)
          raise(DnscatException, "Couldn't figure out how to handle the record type! (please report this, it shouldn't happen): " + type)
        end

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
        response = proc.call(name, max_length)

        # Sanity check the response
        if(response.nil?)
          response = '' # TODO(iagox86): When does this happen, and why? Can we handle it better?
        elsif(response.length > max_length)
          raise(DnscatException, "The handler returned too much data! This shouldn't happen, please report. (max = #{max_length}, returned = #{response.length}")
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
        if(response.is_a?(String) && response.length > type_info[:max_length])
          raise(DnscatException, "The handler returned too much data (after encoding)! This shouldn't happen, please report.")
        end

        # Log the response
        window.puts("Sending:  #{response}")

        # Allow multiple response records
        if(response.is_a?(String))
          reply.add_answer(question.answer(60, response))
        else
          response.each do |r|
            reply.add_answer(question.answer(60, r))
          end
        end
      rescue DNSer::DnsException => e
        window.puts("There was a problem parsing the incoming packet!")
        window.puts(e.inspect)
        e.backtrace.each do |bt|
          window.puts(bt)
        end

        reply = question.get_error(DNSer::Packet::RCODE_NAME_ERROR)
      rescue DnscatException => e
        window.puts("Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
        window.puts(e.inspect)
        e.backtrace.each do |bt|
          window.puts(bt)
        end
        reply = question.get_error(DNSer::Packet::RCODE_NAME_ERROR)
      rescue StandardError => e
        window.puts("Error caught:")
        window.puts(e.inspect)
        e.backtrace.each do |bt|
          window.puts(bt)
        end
        reply = question.get_error(DNSer::Packet::RCODE_NAME_ERROR)
      end

      reply
    end
  end

  def stop()
    # TODO
    exit
    @window.close()
  end
end

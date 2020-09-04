##
# dnser.rb
# Created Oct 7, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# I had nothing but trouble using rubydns (which became celluloid-dns, whose
# documentation is just flat out wrong), and I only really need a very small
# subset of DNS functionality, so I decided that I should just wrote my own.
#
# Note: after writing this, I noticed that Resolv::DNS exists in the Ruby
# language, I need to check if I can use that.
#
# There are two methods for using this library: as a client (to make a query)
# or as a server (to listen for queries and respond to them).
#
# To make a query, use DNSer.query:
#
#   DNSer.query("google.com") do |response|
#     ...
#   end
#
# `response` will be of type DNSer::Packet.
#
# To listen for queries, create a new instance of DNSer, which will begin
# listening on a port, but won't actually handle queries yet:
#
#   dnser = DNSer.new("0.0.0.0", 53)
#   dnser.on_request() do |transaction|
#     ...
#   end
#
# `transaction` is of type DNSer::Transaction, and allows you to respond to the
# request either immediately or asynchronously.
#
# DNSer currently supports the following record types: A, NS, CNAME, SOA, MX,
# TXT, and AAAA.
##

require 'ipaddr'
require 'socket'
require 'timeout'

require_relative './vash'

class DNSer
  # Create a custom error message
  class DnsException < StandardError
  end

  class Packet
    attr_accessor :trn_id, :opcode, :flags, :rcode, :questions, :answers

    # Request / response
    QR_QUERY    = 0x0000
    QR_RESPONSE = 0x0001

    QRS = {
      QR_QUERY    => "QUERY",
      QR_RESPONSE => "RESPONSE",
    }

    # Return codes
    RCODE_SUCCESS         = 0x0000
    RCODE_FORMAT_ERROR    = 0x0001
    RCODE_SERVER_FAILURE  = 0x0002 # :servfail
    RCODE_NAME_ERROR      = 0x0003 # :NXDomain
    RCODE_NOT_IMPLEMENTED = 0x0004
    RCODE_REFUSED         = 0x0005

    RCODES = {
      RCODE_SUCCESS         => ":NoError (RCODE_SUCCESS)",
      RCODE_FORMAT_ERROR    => ":FormErr (RCODE_FORMAT_ERROR)",
      RCODE_SERVER_FAILURE  => ":ServFail (RCODE_SERVER_FAILURE)",
      RCODE_NAME_ERROR      => ":NXDomain (RCODE_NAME_ERROR)",
      RCODE_NOT_IMPLEMENTED => ":NotImp (RCODE_NOT_IMPLEMENTED)",
      RCODE_REFUSED         => ":Refused (RCODE_REFUSED)",
    }

    # Opcodes - only QUERY is typically used
    OPCODE_QUERY  = 0x0000
    OPCODE_IQUERY = 0x0800
    OPCODE_STATUS = 0x1000

    OPCODES = {
      OPCODE_QUERY  => "OPCODE_QUERY",
      OPCODE_IQUERY => "OPCODE_IQUERY",
      OPCODE_STATUS => "OPCODE_STATUS",
    }

    # The types that we support
    TYPE_A     = 0x0001
    TYPE_NS    = 0x0002
    TYPE_CNAME = 0x0005
    TYPE_SOA   = 0x0006
    TYPE_MX    = 0x000f
    TYPE_TXT   = 0x0010
    TYPE_AAAA  = 0x001c
    TYPE_ANY   = 0x00FF

    TYPES = {
      TYPE_A     => "A",
      TYPE_NS    => "NS",
      TYPE_CNAME => "CNAME",
      TYPE_SOA   => "SOA",
      TYPE_MX    => "MX",
      TYPE_TXT   => "TXT",
      TYPE_AAAA  => "AAAA",
      TYPE_ANY   => "ANY",
    }

    # The DNS flags
    FLAG_AA = 0x0008 # Authoritative answer
    FLAG_TC = 0x0004 # Truncated
    FLAG_RD = 0x0002 # Recursion desired
    FLAG_RA = 0x0001 # Recursion available

    # This converts a set of flags, as an integer, into a string
    def Packet.FLAGS(flags)
      result = []
      if((flags & FLAG_AA) == FLAG_AA)
        result << "AA"
      end
      if((flags & FLAG_TC) == FLAG_TC)
        result << "TC"
      end
      if((flags & FLAG_RD) == FLAG_RD)
        result << "RD"
      end
      if((flags & FLAG_RA) == FLAG_RA)
        result << "RA"
      end

      return result.join("|")
    end

    # Classes - we only define IN (Internet)
    CLS_IN                = 0x0001 # Internet

    CLSES = {
      CLS_IN => "IN",
    }

    class FormatException < StandardError
    end

    # DNS has some unusual properties that we have to handle, which is why I
    # wrote this class. It handles building / parsing DNS packets and keeping
    # track of where in the packet we currently are. The advantage, besides
    # simpler unpacking, is that encoded names (with pointers to other parts
    # of the packet) can be trivially handled.
    class DnsUnpacker
      attr_accessor :data

      # Create a new instance, initialized with the given data
      def initialize(data)
        @data = data.force_encoding("ASCII-8BIT")
        @offset = 0
      end

#      def remaining()
#        return @data[@offset..-1]
#      end

      # Unpack from the string, exactly like the normal `String#Unpack` method
      # in Ruby, except that an offset into the string is maintained and updated.
      def unpack(format, offset = nil)
        # If there's an offset, unpack starting there
        if(!offset.nil?)
          results = @data[offset..-1].unpack(format)
        else
          results = @data[@offset..-1].unpack(format + "a*")
          remaining = results.pop
          @offset = @data.length - remaining.length
        end

        if(!results.index(nil).nil?)
          raise(DNSer::Packet::FormatException, "DNS packet was truncated (or we messed up parsing it)!")
        end

        return *results
      end

      # This temporarily changes the offset that we're reading from, runs the
      # given block, then changes it back. This is used internally while
      # unpacking names.
      def _move_offset(offset)
        old_offset = @offset
        @offset = offset
        yield
        @offset = old_offset
      end

      # Unpack a name from the packet. Names are special, because they're
      # encoded as:
      # * A series of length-prefixed blocks, each indicating a segment
      # * Blocks with a length the starts with two '1' bits (11xxxxx...), which
      #   contains a pointer to another name elsewhere in the packet
      def unpack_name(depth = 0)
        segments = []

        if(depth > 16)
          raise(DNSer::Packet::FormatException, "It looks like this packet contains recursive pointers!")
        end

        loop do
          # If no offset is given, just eat data from the normal source
          len = unpack("C").pop()

          # Stop at the null terminator
          if(len == 0)
            break
          end
          # Handle "pointer" records by updating the offset
          if((len & 0xc0) == 0xc0)
            # If the first two bits are 1 (ie, 0xC0), the next
            # 10 bits are an offset, so we have to mask out the first two bits
            # with 0x3F (00111111)
            offset = ((len << 8) | unpack("C").pop()) & 0x3FFF

            _move_offset(offset) do
              segments << unpack_name(depth+1).split(/\./)
            end

            break
          end

          # It's normal, just unpack what we need to!
          segments << unpack("a#{len}")
        end

        return segments.join('.')
      end

      def verify_length(len)
        start_length = @offset
        yield
        end_length   = @offset

        if(end_length - start_length != len)
          raise(FormatException, "A resource record's length didn't match its actual length; something is funky")
        end
      end

      # Take a name, as a dotted string ("google.com") and return it as length-
      # prefixed segments ("\x06google\x03com\x00").
      #
      # TODO: Compress the name properly, if we can ("\xc0\x0c")
      def DnsUnpacker.pack_name(name)
        result = ''

        name.split(/\./).each do |segment|
          result += [segment.length(), segment].pack("Ca*")
        end

        result += "\0"
        return result
      end

      # Shows where in the string we're currently editing. Mostly usefulu for
      # debugging.
      def to_s()
        if(@offset == 0)
          return @data.unpack("H*").pop
        else
          return "#{@data[0..@offset-1].unpack("H*")}|#{@data[@offset..-1].unpack("H*")}"
        end
      end
    end

    class A
      attr_accessor :address

      def initialize(address)
        @address = IPAddr.new(address)

        if(!@address.ipv4?())
          raise(FormatException, "IPv4 address required!")
        end
      end

      def A.parse(data)
        address = data.unpack("A4").pop()
        return A.new(IPAddr.ntop(address))
      end

      def serialize()
        return @address.hton()
      end

      def to_s()
        return "#{@address} [A]"
      end
    end

    class NS
      attr_accessor :name

      def initialize(name)
        @name = name
      end

      def NS.parse(data)
        return NS.new(data.unpack_name())
      end

      def serialize()
        return DNSer::Packet::DnsUnpacker.pack_name(@name)
      end

      def to_s()
        return "#{@name} [NS]"
      end
    end

    class CNAME
      attr_accessor :name

      def initialize(name)
        @name = name
      end

      def CNAME.parse(data)
        return CNAME.new(data.unpack_name())
      end

      def serialize()
        return DNSer::Packet::DnsUnpacker.pack_name(@name)
      end

      def to_s()
        return "#{@name} [CNAME]"
      end
    end

    class SOA
      attr_accessor :primary, :responsible, :serial, :refresh, :retry_interval, :expire, :ttl

      def initialize(primary, responsible, serial, refresh, retry_interval, expire, ttl)
        @primary = primary
        @responsible = responsible
        @serial = serial
        @refresh = refresh
        @retry_interval = retry_interval
        @expire = expire
        @ttl = ttl
      end

      def SOA.parse(data)
        primary = data.unpack_name()
        responsible = data.unpack_name()
        serial, refresh, retry_interval, expire, ttl = data.unpack("NNNNN")

        return SOA.new(primary, responsible, serial, refresh, retry_interval, expire, ttl)
      end

      def serialize()
        return [
          DNSer::Packet::DnsUnpacker.pack_name(@primary),
          DNSer::Packet::DnsUnpacker.pack_name(@responsible),
          @serial,
          @refresh,
          @retry_interval,
          @expire,
          @ttl
        ].pack("a*a*NNNNN")
      end

      def to_s()
        return "Primary name server = #{@primary}, responsible authority's mailbox: #{@responsible}, serial number: #{@serial}, refresh interval: #{@refresh}, retry interval: #{@retry_interval}, expire limit: #{@expire}, min_ttl: #{@ttl} [SOA]"
      end
    end

    class MX
      attr_accessor :preference, :name

      def initialize(name, preference = 10)
        if(!name.is_a?(String) || !preference.is_a?(Integer))
          raise ArgumentError("Creating an MX record wrong! Please file a bug!")
        end
        @name = name
        @preference = preference
      end

      def MX.parse(data)
        preference = data.unpack("n").pop()
        name = data.unpack_name()

        return MX.new(name, preference)
      end

      def serialize()
        name = DNSer::Packet::DnsUnpacker.pack_name(@name)
        return [@preference, name].pack("na*")
      end

      def to_s()
        return "#{@preference} #{@name} [MX]"
      end
    end

    class TXT
      attr_accessor :data

      def initialize(data)
        @data = data
      end

      def TXT.parse(data)
        len = data.unpack("C").pop()
        bytes = data.unpack("A#{len}").pop()

        return TXT.new(bytes)
      end

      def serialize()
        return [@data.length, data].pack("Ca*")
      end

      def to_s()
        return "#{@data} [TXT]"
      end
    end

    class AAAA
      attr_accessor :address

      def initialize(address)
        @address = IPAddr.new(address)

        if(!@address.ipv6?())
          raise(FormatException, "IPv6 address required!")
        end
      end

      def AAAA.parse(data)
        address = data.unpack("A16").pop()
        return AAAA.new(IPAddr.ntop(address))
      end

      def serialize()
        return @address.hton()
      end

      def to_s()
        return "#{@address} [A]"
      end
    end

    class RRUnknown
      def initialize(type, data)
        @type = type
        @data = data
      end

      def RRUnknown.parse(type, data, length)
        data = data.unpack("A#{length}").pop()
        return RRUnknown.new(type, data)
      end

      def serialize()
        return @data
      end

      def to_s()
        return "(Unknown record type #{@type}): #{@data.unpack("H*")}"
      end
    end

    # This defines a DNS question. One question is sent in outgoing packets,
    # and one question is also sent in the response - generally, the same as
    # the question that was asked.
    class Question
      attr_reader :name, :type, :cls

      def initialize(name, type = DNSer::Packet::TYPE_ANY, cls = DNSer::Packet::CLS_IN)
        @name  = name
        @type  = type
        @cls  = cls
      end

      def Question.parse(data)
        name = data.unpack_name()
        type, cls = data.unpack("nn")

        return Question.new(name, type, cls)
      end

      def serialize()
        return [DNSer::Packet::DnsUnpacker.pack_name(@name), type, cls].pack("a*nn")
      end

      def type_s()
        return DNSer::Packet::TYPES[@type] || "<unknown>"
      end

      def cls_s()
        return DNSer::Packet::CLSES[@cls] || "<unknown>"
      end

      def to_s()
        return "#{name} [#{type_s()} #{cls_s()}]"
      end

      def answer(ttl, *args)
        case @type
        when DNSer::Packet::TYPE_A
          record = DNSer::Packet::A.new(*args)
        when DNSer::Packet::TYPE_NS
          record = DNSer::Packet::NS.new(*args)
        when DNSer::Packet::TYPE_CNAME
          record = DNSer::Packet::CNAME.new(*args)
        when DNSer::Packet::TYPE_MX
          record = DNSer::Packet::MX.new(*args)
        when DNSer::Packet::TYPE_TXT
          record = DNSer::Packet::TXT.new(*args)
        when DNSer::Packet::TYPE_AAAA
          record = DNSer::Packet::AAAA.new(*args)
        when DNSer::Packet::TYPE_ANY
          raise(DNSer::Packet::FormatException, "We can't automatically create a response for an 'ANY' request :(")
        else
          raise(DNSer::Packet::FormatException, "We don't know how to answer that type of request!")
        end

        return Answer.new(@name, @type, @cls, ttl, record)
      end

      def ==(other)
        if(!other.is_a?(Question))
          return false
        end

        return (@name == other.name) && (@type == other.type) && (@cls == other.cls)
      end
    end

    # A DNS answer. A DNS response packet contains zero or more Answer records
    # (defined by the 'ancount' value in the header). An answer contains the
    # name of the domain from the question, followed by a resource record.
    class Answer
      attr_reader :name, :type, :cls, :ttl, :rr

      def initialize(name, type, cls, ttl, rr)
        @name = name
        @type = type
        @cls  = cls
        @ttl  = ttl
        @rr   = rr

        if(rr.is_a?(String))
          raise(ArgumentError, "'rr' can't be a string!")
        end
      end

      def ==(other)
        if(!other.is_a?(Answer))
          return false
        end

        # Note: we don't check TTL here, and checking RR probably doesn't work (but we don't actually need it)
        return (@name == other.name) && (@type == other.type) && (@cls == other.cls) && (@rr == other.rr)
      end

      def Answer.parse(data)
        name = data.unpack_name()
        type, cls, ttl, rr_length = data.unpack("nnNn")

        rr = nil
        data.verify_length(rr_length) do
          case type
          when TYPE_A
            rr = A.parse(data)
          when TYPE_NS
            rr = NS.parse(data)
          when TYPE_CNAME
            rr = CNAME.parse(data)
          when TYPE_SOA
            rr = SOA.parse(data)
          when TYPE_MX
            rr = MX.parse(data)
          when TYPE_TXT
            rr = TXT.parse(data)
          when TYPE_AAAA
            rr = AAAA.parse(data)
          else
            puts("Warning: Unknown record type: #{type}")
            rr = RRUnknown.parse(type, data, rr_length)
          end
        end

        return Answer.new(name, type, cls, ttl, rr)
      end

      def serialize()
        # Hardcoding 0xc00c is kind of ugly, but it always works
        rr = @rr.serialize()
        return [0xc00c, @type, @cls, @ttl, rr.length(), rr].pack("nnnNna*")
      end

      def type_s()
        return DNSer::Packet::TYPES[@type]
      end

      def cls_s()
        return DNSer::Packet::CLSES[@cls]
      end

      def to_s()
        return "#{@name} [#{type_s()} #{cls_s()}]: #{@rr} [TTL = #{@ttl}]"
      end
    end

    def initialize(trn_id, qr, opcode, flags, rcode)
      @trn_id    = trn_id
      @qr        = qr
      @opcode    = opcode
      @flags     = flags
      @rcode     = rcode
      @questions = []
      @answers   = []
    end

    def add_question(question)
      @questions << question
    end

    def add_answer(answer)
      @answers << answer
    end

    def Packet.parse(data)
      data = DnsUnpacker.new(data)
      trn_id, full_flags, qdcount, ancount, _, _ = data.unpack("nnnnnn")

      qr     = (full_flags >> 15) & 0x0001
      opcode = (full_flags >> 11) & 0x000F
      flags  = (full_flags >> 7)  & 0x000F
      rcode  = (full_flags >> 0)  & 0x000F

      packet = Packet.new(trn_id, qr, opcode, flags, rcode)

      0.upto(qdcount - 1) do
        question = Question.parse(data)
        packet.add_question(question)
      end

      0.upto(ancount - 1) do
        answer = Answer.parse(data)
        packet.add_answer(answer)
      end

      return packet
    end

    def get_error(rcode)
      return Packet.new(@trn_id, DNSer::Packet::QR_RESPONSE, DNSer::Packet::OPCODE_QUERY, DNSer::Packet::FLAG_RD | DNSer::Packet::FLAG_RA, rcode)
    end

    def serialize()
      result = ''

      full_flags = ((@qr     << 15) & 0x8000) |
                   ((@opcode << 11) & 0x7800) |
                   ((@flags  <<  7) & 0x0780) |
                   ((@rcode  <<  0) & 0x000F)

      result += [
                  @trn_id,             # trn_id
                  full_flags,          # qr, opcode, flags, rcode
                  @questions.length(), # qdcount
                  @answers.length(),   # ancount
                  0,                   # nscount (ignored)
                  0                    # arcount (ignored)
                ].pack("nnnnnn")

      questions.each do |q|
        result += q.serialize()
      end

      answers.each do |a|
        result += a.serialize()
      end

      return result
    end

    def to_s(brief = false)
      if(brief)
        question = @questions[0] || '<unknown>'

        # Print error packets more clearly
        if(@rcode != DNSer::Packet::RCODE_SUCCESS)
          return "Request for #{question}: error: #{DNSer::Packet::RCODES[@rcode]}"
        end

        if(@qr == DNSer::Packet::QR_QUERY)
          return "Request for #{question}"
        else
          if(@answers.length == 0)
            return "Response for #{question}: n/a"
          else
            return "Response for #{question}: #{@answers[0]} (and #{@answers.length - 1} others)"
          end
        end
      end

      results = ["DNS #{QRS[@qr] || "unknown"}: id=#{@trn_id}, opcode=#{OPCODES[@opcode]}, flags=#{Packet.FLAGS(@flags)}, rcode=#{RCODES[@rcode] || "unknown"}, qdcount=#{@questions.length}, ancount=#{@answers.length}"]

      @questions.each do |q|
        results << "    Question: #{q}"
      end

      @answers.each do |a|
        results << "    Answer: #{a}"
      end

      return results.join("\n")
    end

    def ==(other)
      if(!other.is_a?(Packet))
        return false
      end

      return (@trn_id == other.trn_id) && (@opcode == other.opcode) && (@flags == other.flags) && (@questions == other.questions) && (@answers == other.answers)
    end
  end

  # When a request comes in, a transaction is created and sent to the callback.
  # The transaction can be used to respond to the request at any point in the
  # future.
  #
  # Any methods with a bang ('!') in front will send the response back to the
  # requester. Only one bang method can be called, any subsequent calls will
  # throw an exception.
  class Transaction
    attr_reader :request, :response, :sent

    def initialize(s, request, host, port, cache = nil)
      @s       = s
      @request = request
      @host    = host
      @port    = port
      @sent    = false
      @cache   = cache

      @response = DNSer::Packet.new(
        @request.trn_id,
        DNSer::Packet::QR_RESPONSE,
        @request.opcode,
        DNSer::Packet::FLAG_RD | DNSer::Packet::FLAG_RA,
        DNSer::Packet::RCODE_SUCCESS
      )

      @response.add_question(@request.questions[0])
    end

    def add_answer(answer)
      raise ArgumentError("Already sent!") if(@sent)

      @response.add_answer(answer)
    end

    def error(rcode)
      raise ArgumentError("Already sent!") if(@sent)

      @response.rcode = rcode
    end

    def error!(rcode)
      raise ArgumentError("Already sent!") if(@sent)

      @response.rcode = rcode
      reply!()
    end

    def passthrough!(pt_host, pt_port, callback = nil)
      raise ArgumentError("Already sent!") if(@sent)

      DNSer.query(@request.questions[0].name, {
          :server  => pt_host,
          :port    => pt_port,
          :type    => @request.questions[0].type,
          :cls     => @request.questions[0].cls,
          :timeout => 3,
        }
      ) do |response|
        # If there was a timeout, handle it
        if(response.nil?)
          response = @response
          response.rcode = DNSer::Packet::RCODE_SERVER_FAILURE
        end

        response.trn_id = @request.trn_id
        @s.send(response.serialize(), 0, @host, @port)

        # Let the callback know if anybody registered one
        if(callback)
          callback.call(response)
        end
      end

      @sent = true
    end

    def reply!(_="")
      raise ArgumentError("Already sent!") if(@sent)

      # Cache it if we have a cache
      if(@cache)
        @cache[@request.trn_id, 3] = {
          :request  => @request,
          :response => @response,
        }
      end

      # Send the response
      @s.send(@response.serialize(), 0, @host, @port)
      @sent = true
    end
  end

  # Create a new DNSer and listen on the given host/port. This will throw an
  # exception if we aren't allowed to bind to the given port.
  def initialize(host, port, cache=false)
    @s = UDPSocket.new()
    @s.bind(host, port)
    @thread = nil

    # Create a cache if the user wanted one
    if(cache)
      @cache = Vash.new()
    end
  end

  # This method returns immediately, but spawns a background thread. The thread
  # will recveive and parse DNS packets, create a transaction, and pass it to
  # the caller's block.
  def on_request(&block)
    @thread = Thread.new() do |t|
      begin
        loop do
          data = @s.recvfrom(65536)
          
          begin   
            # Data is an array where the first element is the actual data, and the second is the host/port
            request = DNSer::Packet.parse(data[0])

            # Create a transaction object, which we can use to respond
            transaction = Transaction.new(@s, request, data[1][3], data[1][1], @cache)

            # If caching is enabled, deal with it
            if(@cache)
              # This is somewhat expensive, but we aren't using the cache for performance
              @cache.cleanup!()

              # See if the transaction is cached
              cached = @cache[request.trn_id]

              # Verify it deeper (for security reasons)
              if(!cached.nil?)
                if(request == cached[:request])
                  puts("CACHE HIT")
                  transaction.reply!(cached[:response])
                end
              end
            end

            if(!transaction.sent)
              begin
                block.call(transaction)
              rescue StandardError => e
                puts("Caught an error: #{e}")
                puts(e.backtrace())
                transaction.reply!(transaction.response_template({:rcode => DNSer::Packet::RCODE_SERVER_FAILURE}))
              end
            end
          rescue StandardError => e
            puts("Caught an error: #{e}")
            puts(e.backtrace())
          end
        end
      ensure
        @s.close
      end
    end
  end

  # Kill the listener
  def stop()
    if(@thread.nil?)
      puts("Tried to stop a listener that wasn't listening!")
      return
    end

    @thread.kill()
    @thread = nil
  end

  # After calling on_request(), this can be called to halt the program's
  # execution until the thread is stopped.
  def wait()
    if(@thread.nil?)
      puts("Tried to wait on a DNSer instance that wasn't listening!")
      return
    end

    @thread.join()
  end

  # Send out a query, asynchronously. This immediately returns, then, when the
  # query is finished, the callback block is called with a DNSer::Packet that
  # represents the response (or nil, if there was a timeout).
  def DNSer.query(hostname, params = {}, &block)
    server   = params[:server]   || "8.8.8.8"
    port     = params[:port]     || 53
    type     = params[:type]     || DNSer::Packet::TYPE_A
    cls      = params[:cls]      || DNSer::Packet::CLS_IN
    timeout  = params[:timeout]  || 3

    packet = DNSer::Packet.new(rand(65535), DNSer::Packet::QR_QUERY, DNSer::Packet::OPCODE_QUERY, DNSer::Packet::FLAG_RD, DNSer::Packet::RCODE_SUCCESS)
    packet.add_question(DNSer::Packet::Question.new(hostname, type, cls))

    s = UDPSocket.new()

    return Thread.new() do
      begin
        s.send(packet.serialize(), 0, server, port)

        timeout(timeout) do
          response = s.recv(65536)
          block.call(DNSer::Packet.parse(response))
        end
      rescue Timeout::Error
        block.call(nil)
      rescue Exception => e
        puts("There was an exception sending a query for #{hostname} to #{server}:#{port}: #{e}")
      ensure
        if(s)
          s.close()
        end
      end
    end
  end
end

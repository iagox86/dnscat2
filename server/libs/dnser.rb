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
##

require 'ipaddr'
require 'socket'
require 'timeout'
require '/usr/local/google/home/rbowes/tools/dnscat2/server/libs/hex.rb'

class DNSer
  class DnsException < StandardError
  end

  class Packet
    attr_accessor :trn_id, :opcode, :flags, :rcode, :questions, :answers

    QR_QUERY    = 0x0000
    QR_RESPONSE = 0x0001

    QRS = {
      QR_QUERY    => "QUERY",
      QR_RESPONSE => "RESPONSE",
    }

    RCODE_SUCCESS         = 0x0000
    RCODE_FORMAT_ERROR    = 0x0001
    RCODE_SERVER_FAILURE  = 0x0002
    RCODE_NAME_ERROR      = 0x0003
    RCODE_NOT_IMPLEMENTED = 0x0004
    RCODE_REFUSED         = 0x0005

    RCODES = {
      RCODE_SUCCESS         => "RCODE_SUCCESS",
      RCODE_FORMAT_ERROR    => "RCODE_FORMAT_ERROR",
      RCODE_SERVER_FAILURE  => "RCODE_SERVER_FAILURE",
      RCODE_NAME_ERROR      => "RCODE_NAME_ERROR",
      RCODE_NOT_IMPLEMENTED => "RCODE_NOT_IMPLEMENTED",
      RCODE_REFUSED         => "RCODE_REFUSED",
    }

    OPCODE_QUERY  = 0x0000
    OPCODE_IQUERY = 0x0800
    OPCODE_STATUS = 0x1000

    OPCODES = {
      OPCODE_QUERY  => "OPCODE_QUERY",
      OPCODE_IQUERY => "OPCODE_IQUERY",
      OPCODE_STATUS => "OPCODE_STATUS",
    }

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

    FLAG_AA = 0x0008 # Authoritative answer
    FLAG_TC = 0x0004 # Truncated
    FLAG_RD = 0x0002 # Recursion desired
    FLAG_RA = 0x0001 # Recursion available

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

    CLS_IN                = 0x0001 # Internet

    CLSES = {
      CLS_IN => "IN",
    }

    class FormatException < StandardError
    end

    class DnsUnpacker
      attr_accessor :data

      def initialize(data)
        @data = data.force_encoding("ASCII-8BIT")
        @offset = 0
      end

      def remaining()
        return @data[@offset..-1]
      end

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

      def move_offset(offset)
        old_offset = @offset
        @offset = offset
        yield
        @offset = old_offset
      end

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

            move_offset(offset) do
              segments << unpack_name().split(/\./)
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
          raise(FormatException, "There was something wrong with a resource record in the packet!")
        end
      end

      # TODO: Compress the name properly, if we acn
      def DnsUnpacker.pack_name(name)
        result = ''

        name.split(/\./).each do |segment|
          result += [segment.length(), segment].pack("Ca*")
        end

        result += "\0"
        return result
      end

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
        if(!name.is_a?(String) || !preference.is_a?(Fixnum))
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
        return DNSer::Packet::TYPES[@type]
      end

      def cls_s()
        return DNSer::Packet::CLSES[@cls]
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
    end

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

    def to_s()
      results = ["DNS #{QRS[@qr] || "unknown"}: id=#{@trn_id}, opcode=#{OPCODES[@opcode]}, flags=#{Packet.FLAGS(@flags)}, rcode=#{RCODES[@rcode] || "unknown"}, qdcount=#{@questions.length}, ancount=#{@answers.length}"]

      @questions.each do |q|
        results << "    Question: #{q}"
      end

      @answers.each do |a|
        results << "    Answer: #{a}"
      end

      return results.join("\n")
    end
  end

  class Transaction
    attr_reader :request, :response, :sent

    def initialize(s, request, host, port)
      @s       = s
      @request = request
      @host    = host
      @port    = port
      @sent    = false

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

      DNSer.query(
        @request.questions[0].name,
        pt_host,
        pt_port,
        @request.questions[0].type,
        @request.questions[0].cls,
        3
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

    def reply!()
      raise ArgumentError("Already sent!") if(@sent)

      @s.send(@response.serialize(), 0, @host, @port)
      @sent = true
    end
  end

  def initialize(host, port)
    @s = UDPSocket.new()
    @s.bind(host, port)
  end

  def on_request()
    @thread = Thread.new() do |t|
      begin
        loop do
          data = @s.recvfrom(65536)
          request = DNSer::Packet.parse(data[0])
          transaction = Transaction.new(@s, request, data[1][3], data[1][1])

          begin
            proc.call(transaction)
          rescue StandardError => e
            puts("Caught an error: #{e}")
            puts(e.backtrace())
            transaction.reply!(transaction.response_template({:rcode => DNSer::Packet::RCODE_SERVER_FAILURE}))
          end
        end
      ensure
        @s.close
      end
    end
  end

  def stop()
    @thread.kill()
  end

  def wait()
    @thread.join()
  end

  def DNSer.query(hostname, server = "8.8.8.8", port = 53, type = DNSer::Packet::TYPE_ANY, cls = DNSer::Packet::CLS_IN, timeout_seconds = 3)
    packet = DNSer::Packet.new(rand(65535), DNSer::Packet::QR_QUERY, DNSer::Packet::OPCODE_QUERY, DNSer::Packet::FLAG_RD, DNSer::Packet::RCODE_SUCCESS)
    packet.add_question(DNSer::Packet::Question.new(hostname, type, cls))

    s = UDPSocket.new()

    Thread.new() do
      begin
        s.send(packet.serialize(), 0, server, port)

        timeout(timeout_seconds) do
          response = s.recv(65536)
          proc.call(DNSer::Packet.parse(response))
        end
      rescue Timeout::Error
        proc.call(nil)
      ensure
        if(s)
          s.close()
        end
      end
    end
  end
end

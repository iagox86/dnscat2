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

class Dnser
  class DnsException < StandardError
  end

  class Packet
    class FormatException < StandardError
    end

    class DnsUnpacker
      def initialize(data)
        @data = data
        @offset = 0
        @saved_offset = nil
      end

      def reset
        @offset = 0
      end

      def save()
        if(@saved_offset.nil?)
          @saved_offset = @offset
        end
      end

      def restore()
        @offset = @saved_offset
        @saved_offset = nil
      end

      def move(i)
        @offset = i
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
          raise(Dnser::Packet::FormatException, "DNS packet was truncated (or we messed up parsing it)!")
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
          raise(Dnser::Packet::FormatException, "It looks like this packet contains recursive pointers!")
        end

        loop do
          # If no offset is given, just eat data from the normal source
          len = unpack("C").pop()

          # Stop at the null terminator
          if(len == 0)
            break
          end

          # Handle "pointer" records by updating the offset
          if(len == 0xc0)
            save()
            offset = unpack("C").pop()

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

    attr_accessor :trn_id, :opcode, :flags, :rcode, :questions, :answers

    OPCODE_QUERY          = 0x0000
    OPCODE_RESPONSE       = 0x0001

    RCODE_SUCCESS         = 0x0000
    RCODE_FORMAT_ERROR    = 0x0001
    RCODE_SERVER_FAILURE  = 0x0002
    RCODE_NAME_ERROR      = 0x0003
    RCODE_NOT_IMPLEMENTED = 0x0004
    RCODE_REFUSED         = 0x0005

    TYPE_A                = 0x0001
    TYPE_NS               = 0x0002
    TYPE_CNAME            = 0x0005
    TYPE_SOA              = 0x0006
    TYPE_MX               = 0x000f
    TYPE_TEXT             = 0x0010
    TYPE_AAAA             = 0x001c
    TYPE_ANY              = 0x00FF

    FLAG_AA               = 0x0008 # Authoritative answer
    FLAG_TC               = 0x0004 # Truncated
    FLAG_RD               = 0x0002 # Recursion desired
    FLAG_RA               = 0x0001 # Recursion available

    CLS_IN                = 0x0001 # Internet

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
        return data.unpack_name()
      end

      def serialize()
        return Dnser::Packet::DnsUnpacker.pack_name(@name)
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
        return data.unpack_name()
      end

      def serialize()
        return Dnser::Packet::DnsUnpacker.pack_name(@name)
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
          Dnser::Packet::DnsUnpacker.pack_name(@primary),
          Dnser::Packet::DnsUnpacker.pack_name(@responsible),
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

      def initialize(preference, name)
        @preference = preference
        @name = name
      end

      def MX.parse(data)
        preference = data.unpack("n").pop()
        name = data.unpack_name()

        return MX.new(preference, name)
      end

      def serialize()
        name = Dnser::Packet::DnsUnpacker.pack_name(@name)
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

    class Question
      attr_reader :name, :type, :cls

      def initialize(name, type = Dnser::Packet::TYPE_ANY, cls = Dnser::Packet::CLS_IN)
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
        return [Dnser::Packet::DnsUnpacker.pack_name(@name), type, cls].pack("a*nn")
      end

      def to_s()
        return "#{name} [#{type} #{cls}]"
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
      end

      def to_s()
        return "#{@name} [#{@type} #{@cls}]: #{@rr} [TTL = #{@ttl}]"
      end

      def Answer.parse(data)
        name = data.unpack_name()
        type, cls, ttl, rr_length = data.unpack("nnNn")

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
        when TYPE_TEXT
          rr = TXT.parse(data)
        when TYPE_AAAA
          rr = AAAA.parse(data)
        else
          puts("Warning: Unknown record type: #{type}")
          data.unpack("A#{rr_length}").pop()
        end

        return Answer.new(name, type, cls, ttl, rr)
      end

      def serialize()
        # Hardcoding 0xc00c is kind of ugly, but it always works
        rr = @rr.serialize()
        return [0xc00c, @type, @cls, @ttl, rr.length(), rr].pack("nnnNna*")
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
      results = ["DNS #{@qr == 0 ? 'request' : 'response'}: id=#{@trn_id}, opcode=#{@opcode}, flags=#{@flags}, rcode=#{@rcode}, qdcount=#{@questions.length}, ancount=#{@answers.length}"]

      @questions.each do |q|
        results << "    Question: #{q}"
      end

      @answers.each do |a|
        results << "    Answer: #{a}"
      end

      return results.join("\n")
    end
  end

  def initialize(host, port, regex = nil)
    @host = host
    @port = port
    @proc = proc
    @thread = Thread.new() do |t|
      puts host
      puts @host
    end
  end

  def stop()
    # TODO: Close the socket
    @thread.kill()
  end

  def Dnser.query(hostname, server = "8.8.8.8", port = 53, type = Dnser::Packet::TYPE_ANY, cls = Dnser::Packet::CLS_IN, timeout_seconds = 3)
    packet = Dnser::Packet.new(rand(65535), 0, 0, Dnser::Packet::FLAG_RD, 0)
    packet.add_question(Dnser::Packet::Question.new(hostname, type, cls))

    Thread.new() do
      begin
        timeout(timeout_seconds) do
          s = UDPSocket.new()
          s.send(packet.serialize(), 0, server, port)
          response = s.recv(1024) # Max length of a DNS packet is 512 bytes, so this should be safe
          s.close()

          proc.call(Dnser::Packet.parse(response))
        end
      rescue Timeout::Error
        proc.call(nil)
      end
    end
  end
end

Thread.abort_on_exception = true

#p = Dnser::Packet.parse("\x48\x05\x01\x20\x00\x01\x00\x00\x00\x00\x00\x01\x06\x67\x6f\x6f\x67\x6c\x65\x03\x63\x6f\x6d\x00\x00\x01\x00\x01\x00\x00\x29\x10\x00\x00\x00\x00\x00\x00\x00")
#puts(p.to_s)
#
#p = Dnser::Packet.parse("\x48\x05\x81\x80\x00\x01\x00\x0f\x00\x00\x00\x00\x06\x67\x6f\x6f\x67\x6c\x65\x03\x63\x6f\x6d\x00\x00\x01\x00\x01\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xdb\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xfb\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xdd\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xea\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xf1\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xd3\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xe2\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xd7\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xe6\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xec\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xde\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xf5\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xed\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xcf\xc0\x0c\x00\x01\x00\x01\x00\x00\x01\x2b\x00\x04\x55\xea\xcc\xf9")
#puts(p.to_s)
#
#p = Dnser::Packet.parse("\x6c\xba\x01\x20\x00\x01\x00\x00\x00\x00\x00\x01\x0d\x73\x6b\x75\x6c\x6c\x73\x65\x63\x75\x72\x69\x74\x79\x03\x6f\x72\x67\x00\x00\xff\x00\x01\x00\x00\x29\x10\x00\x00\x00\x00\x00\x00\x00")
#puts(p.to_s)
#
#p = Dnser::Packet.parse("\x6c\xba\x81\x80\x00\x01\x00\x0c\x00\x00\x00\x00\x0d\x73\x6b\x75\x6c\x6c\x73\x65\x63\x75\x72\x69\x74\x79\x03\x6f\x72\x67\x00\x00\xff\x00\x01\xc0\x0c\x00\x0f\x00\x01\x00\x00\x0e\x0f\x00\x19\x00\x0a\x06\x41\x53\x50\x4d\x58\x32\x0a\x47\x4f\x4f\x47\x4c\x45\x4d\x41\x49\x4c\x03\x63\x6f\x6d\x00\xc0\x0c\x00\x0f\x00\x01\x00\x00\x0e\x0f\x00\x0b\x00\x0a\x06\x41\x53\x50\x4d\x58\x33\xc0\x38\xc0\x0c\x00\x1c\x00\x01\x00\x00\x0e\x0f\x00\x10\x26\x00\x3c\x01\x00\x00\x00\x00\xf0\x3c\x91\xff\xfe\xc8\xb8\x32\xc0\x0c\x00\x0f\x00\x01\x00\x00\x0e\x0f\x00\x13\x00\x01\x05\x41\x53\x50\x4d\x58\x01\x4c\x06\x47\x4f\x4f\x47\x4c\x45\xc0\x43\xc0\x0c\x00\x10\x00\x01\x00\x00\x0e\x0f\x00\x45\x44\x67\x6f\x6f\x67\x6c\x65\x2d\x73\x69\x74\x65\x2d\x76\x65\x72\x69\x66\x69\x63\x61\x74\x69\x6f\x6e\x3d\x75\x49\x63\x42\x46\x76\x4e\x58\x53\x53\x61\x41\x45\x49\x6b\x67\x36\x6b\x5a\x33\x5f\x5a\x4c\x41\x56\x70\x43\x6e\x41\x6d\x49\x33\x50\x49\x75\x49\x7a\x77\x72\x62\x70\x76\x38\xc0\x0c\x00\x02\x00\x01\x00\x00\x0e\x0f\x00\x15\x04\x6e\x73\x31\x39\x0d\x64\x6f\x6d\x61\x69\x6e\x63\x6f\x6e\x74\x72\x6f\x6c\xc0\x43\xc0\x0c\x00\x06\x00\x01\x00\x00\x0e\x0f\x00\x25\xc0\xf7\x03\x64\x6e\x73\x05\x6a\x6f\x6d\x61\x78\x03\x6e\x65\x74\x00\x78\x1b\xb6\xdb\x00\x00\x70\x80\x00\x00\x1c\x20\x00\x09\x3a\x80\x00\x00\x0e\x10\xc0\x0c\x00\x10\x00\x01\x00\x00\x0e\x0f\x00\x0b\x0a\x6f\x68\x20\x68\x61\x69\x20\x4e\x53\x41\xc0\x0c\x00\x02\x00\x01\x00\x00\x0e\x0f\x00\x07\x04\x6e\x73\x32\x30\xc0\xfc\xc0\x0c\x00\x0f\x00\x01\x00\x00\x0e\x0f\x00\x09\x00\x05\x04\x41\x4c\x54\x31\xc0\x89\xc0\x0c\x00\x0f\x00\x01\x00\x00\x0e\x0f\x00\x09\x00\x05\x04\x41\x4c\x54\x32\xc0\x89\xc0\x0c\x00\x01\x00\x01\x00\x00\x0e\x0f\x00\x04\xc0\x9b\x51\x56")
#puts(p.to_s)

Dnser.query('skullsecadsfurity.org', '4.2.2.1') do |response|
  if(response.nil?)
    puts "Timeout!"
    next
  end

  puts response
  exit
end

sleep(5)

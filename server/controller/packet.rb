##
# packet.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# Builds and parses dnscat2 packets.
##

require 'controller/encryptor'
require 'libs/dnscat_exception'
require 'libs/hex'

module PacketHelper
  def at_least?(data, needed)
    return (data.length >= needed)
  end
  def exactly?(data, needed)
    return (data.length == needed)
  end
end

class Packet
  extend PacketHelper

  # Message types
  MESSAGE_TYPE_SYN        = 0x00
  MESSAGE_TYPE_MSG        = 0x01
  MESSAGE_TYPE_FIN        = 0x02
  MESSAGE_TYPE_PING       = 0xFF
  MESSAGE_TYPE_ENC        = 0x03

  OPT_NAME                = 0x0001
  # OPT_TUNNEL              = 0x0002 # Deprecated
  # OPT_DATAGRAM            = 0x0004 # Deprecated
  # OPT_DOWNLOAD            = 0x0008 # Deprecated
  # OPT_CHUNKED_DOWNLOAD    = 0x0010 # Deprecated
  OPT_COMMAND             = 0x0020

  attr_reader :packet_id, :type, :session_id, :body

  class SynBody
    extend PacketHelper

    attr_reader :seq, :options, :name

    def initialize(options, params = {})
      @options = options || raise(DnscatException, "options can't be nil!")
      @seq = params[:seq] || raise(DnscatException, "params[:seq] can't be nil!")

      if((@options & OPT_NAME) == OPT_NAME)
        @name = params[:name] || raise(DnscatException, "params[:name] can't be nil when OPT_NAME is set!")
      else
        @name = "(unnamed)"
      end
    end

    def SynBody.parse(data)
      at_least?(data, 4) || raise(DnscatException, "Packet is too short (SYN)")

      seq, options, data = data.unpack("nna*")

      # Parse the option name, if it has one
      name = nil
      if((options & OPT_NAME) == OPT_NAME)
        if(data.index("\0").nil?)
          raise(DnscatException, "OPT_NAME set, but no null-terminated name given")
        end
        name, data = data.unpack("Z*a*")
      else
        name = "[unnamed]"
      end

      # Verify that that was the entire packet
      if(data.length > 0)
        raise(DnscatException, "Extra data on the end of an SYN packet :: #{data.unpack("H*")}")
      end

      return SynBody.new(options, {
        :seq          => seq,
        :name         => name,
      })
    end

    def to_s()
      return "[[SYN]] :: isn = %04x, options = %04x, name = %s" % [@seq, @options, @name]
    end

    def to_bytes()
      result = [@seq, @options].pack("nn")

      if((@options & OPT_NAME) == OPT_NAME)
        result += [@name].pack("Z*")
      end

      return result
    end
  end

  class MsgBody
    extend PacketHelper

    attr_reader :seq, :ack, :data

    def initialize(options, params = {})
      @options = options
      @seq = params[:seq] || raise(DnscatException, "params[:seq] can't be nil!")
      @ack = params[:ack] || raise(DnscatException, "params[:ack] can't be nil!")
      @data = params[:data] || raise(DnscatException, "params[:data] can't be nil!")
    end

    def MsgBody.parse(options, data)
      at_least?(data, 4) || raise(DnscatException, "Packet is too short (MSG norm)")

      seq, ack = data.unpack("nn")
      data = data[4..-1] # Remove the first four bytes

      return MsgBody.new(options, {
        :seq   => seq,
        :ack   => ack,
        :data  => data,
      })
    end

    def MsgBody.header_size(options)
      return MsgBody.new(options, {
        :seq   => 0,
        :ack   => 0,
        :data  => '',
      }).to_bytes().length()
    end

    def to_s()
      return "[[MSG]] :: seq = %04x, ack = %04x, data = 0x%x bytes" % [@seq, @ack, data.length]
    end

    def to_bytes()
      result = ""
      seq = @seq || 0
      ack = @ack || 0
      result += [seq, ack, @data].pack("nna*")

      return result
    end
  end

  class FinBody
    extend PacketHelper

    attr_reader :reason

    def initialize(options, params = {})
      @options = options
      @reason = params[:reason] || raise(DnscatException, "params[:reason] can't be nil!")
    end

    def FinBody.parse(options, data)
      at_least?(data, 1) || raise(DnscatException, "Packet is too short (FIN)")

      reason = data.unpack("Z*").pop
      data = data[(reason.length+1)..-1]

      if(data.length > 0)
        raise(DnscatException, "Extra data on the end of a FIN packet")
      end

      return FinBody.new(options, {
        :reason => reason,
      })
    end

    def to_s()
      return "[[FIN]] :: %s" % [@reason]
    end

    def to_bytes()
      [@reason].pack("Z*")
    end
  end

  class PingBody
    extend PacketHelper

    attr_reader :data

    def initialize(options, params = {})
      @options = options
      @data = params[:data] || raise(DnscatException, "params[:data] can't be nil!")
    end

    def PingBody.parse(options, data)
      at_least?(data, 3) || raise(DnscatException, "Packet is too short (PING)")

      data = data.unpack("Z*").pop

      return PingBody.new(options, {
        :data => data,
      })
    end

    def to_s()
      return "[[PING]] :: %s" % [@data]
    end

    def to_bytes()
      [@data].pack("Z*")
    end
  end

  class EncBody
    extend PacketHelper

    SUBTYPE_INIT = 0x0000
    SUBTYPE_AUTH = 0x0001
    attr_reader :subtype, :flags
    attr_reader :public_key_x, :public_key_y # SUBTYPE_INIT
    attr_reader :authenticator # SUBTYPE_AUTH

    def initialize(params = {})
      @subtype = params[:subtype] || raise(DnscatException, "params[:subtype] is required!")
      @flags   = params[:flags]   || raise(DnscatException, "params[:flags] is required!")

      if(@subtype == SUBTYPE_INIT)
        @public_key_x = params[:public_key_x] || raise(DnscatException, "params[:public_key_x] is required!")
        @public_key_y = params[:public_key_y] || raise(DnscatException, "params[:public_key_y] is required!")

        if(!@public_key_x.is_a?(Bignum) || !@public_key_y.is_a?(Bignum))
          raise(DnscatException, "Public keys have to be Bignums! (Seen: #{@public_key_x.class} #{@public_key_y.class})")
        end
      elsif(@subtype == SUBTYPE_AUTH)
        @authenticator = params[:authenticator] || raise(DnscatException, "params[:authenticator] is required!")

        if(@authenticator.length != 32)
          raise(DnscatException, "params[:authenticator] was the wrong size!")
        end
      else
        raise(DnscatException, "Unknown subtype: #{@subtype}")
      end
    end

    def EncBody.parse(data)
      at_least?(data, 4) || raise(DnscatException, "ENC packet is too short!")

      subtype, flags, data = data.unpack("nna*")

      params = {
        :subtype => subtype,
        :flags   => flags,
      }

      if(subtype == SUBTYPE_INIT)
        exactly?(data, 64) || raise(DnscatException, "ENC packet is too short!")

        public_key_x, public_key_y, data = data.unpack("a32a32a*")

        params[:public_key_x] = Encryptor.binary_to_bignum(public_key_x)
        params[:public_key_y] = Encryptor.binary_to_bignum(public_key_y)

      elsif(subtype == SUBTYPE_AUTH)
        exactly?(data, 32) || raise(DnscatException, "ENC packet is too short!")

        authenticator, data = data.unpack("a32a*")

        params[:authenticator] = authenticator
      else
        raise(DnscatException, "Unknown subtype: #{subtype}")
      end

      if(data != "")
        raise(DnscatException, "Extra data on the end of an ENC packet")
      end

      return EncBody.new(params)
    end

    def to_s()
      if(@subtype == SUBTYPE_INIT)
        return "[[ENC|INIT]] :: flags = 0x%04x, pubkey = %s,%s" % [@flags, Encryptor.bignum_to_text(@public_key_x), Encryptor.bignum_to_text(@public_key_y)]
      elsif(@subtype == SUBTYPE_AUTH)
        return "[[ENC|AUTH]] :: flags = 0x%04x, authenticator = %s" % [@flags, @authenticator.unpack("H*").pop()]
      else
        raise(DnscatException, "Unknown subtype: #{@subtype}")
      end
    end

    def to_bytes()
      if(@subtype == SUBTYPE_INIT)
        public_key_x = Encryptor.bignum_to_binary(@public_key_x)
        public_key_y = Encryptor.bignum_to_binary(@public_key_y)

        return [@subtype, @flags, public_key_x, public_key_y].pack("nna32a32")
      elsif(@subtype == SUBTYPE_AUTH)
        return [@subtype, @flags, @authenticator].pack("nna32")
      else
        raise(DnscatException, "Unknown subtype: #{@subtype}")
      end
    end
  end

  def ==(other_packet)
    return self.to_bytes() == other_packet.to_bytes()
  end

  # You probably don't ever want to use this, call Packet.parse() or Packet.create_*() instead
  def initialize(packet_id, type, session_id, body)
    @packet_id  = packet_id  || rand(0xFFFF)
    @type       = type       || raise(DnscatException, "type can't be nil!")
    @session_id = session_id || raise(DnscatException, "session_id can't be nil!")
    @body       = body
  end

  def Packet.header_size(options)
    return Packet.new(0, 0, 0, nil).to_bytes().length()
  end

  def Packet.parse_header(data)
    at_least?(data, 5) || raise(DnscatException, "Packet is too short (header)")

    # (uint16_t) packet_id
    # (uint8_t)  message_type
    # (uint16_t) session_id
    packet_id, type, session_id = data.unpack("nCn")
    data = data[5..-1]

    return packet_id, type, session_id, data
  end

  def Packet.peek_session_id(data)
    _, _, session_id, _ = Packet.parse_header(data)

    return session_id
  end

  def Packet.peek_type(data)
    _, type, _, _ = Packet.parse_header(data)

    return type
  end

  def Packet.parse(data, options = nil)
    packet_id, type, session_id, data = Packet.parse_header(data)

    if(type == MESSAGE_TYPE_SYN)
      body = SynBody.parse(data)
    elsif(type == MESSAGE_TYPE_MSG)
      if(options.nil?)
        raise(DnscatException, "Options are required when parsing MSG packets!")
      end
      body = MsgBody.parse(options, data)
    elsif(type == MESSAGE_TYPE_FIN)
      if(options.nil?)
        raise(DnscatException, "Options are required when parsing FIN packets!")
      end
      body = FinBody.parse(options, data)
    elsif(type == MESSAGE_TYPE_PING)
      body = PingBody.parse(nil, data)
    elsif(type == MESSAGE_TYPE_ENC)
      body = EncBody.parse(data)
    else
      raise(DnscatException, "Unknown message type: 0x%x" % type)
    end

    return Packet.new(packet_id, type, session_id, body)
  end

  def Packet.create_syn(options, params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_SYN, params[:session_id], SynBody.new(options, params))
  end

  def Packet.create_msg(options, params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_MSG, params[:session_id], MsgBody.new(options, params))
  end

  def Packet.create_fin(options, params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_FIN, params[:session_id], FinBody.new(options, params))
  end

  def Packet.create_ping(params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_PING, params[:session_id], PingBody.new(nil, params))
  end

  def Packet.create_enc(params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_ENC, params[:session_id], EncBody.new(params))
  end

  def to_s()
    result = "[0x%04x] session = %04x :: %s\n" % [@packet_id, @session_id, @body.to_s]
    result += Hex.to_s(to_bytes(), 2)
    return result
  end

  def to_bytes()
    result = [@packet_id, @type, @session_id].pack("nCn")

    # If we set the body to nil, just return a header (this happens when determining the header length)
    if(!@body.nil?)
      result += @body.to_bytes()
    end

    return result
  end
end


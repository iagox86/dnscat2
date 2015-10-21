##
# packet.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# Builds and parses dnscat2 packets.
##

require 'salsa20'

require 'libs/dnscat_exception'
require 'libs/hex'
require 'libs/sha3'

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
  MESSAGE_TYPE_NEGENC     = 0x03
  MESSAGE_TYPE_AUTH       = 0x04

  OPT_NAME                = 0x0001
  # OPT_TUNNEL              = 0x0002 # Deprecated
  # OPT_DATAGRAM            = 0x0004 # Deprecated
  OPT_DOWNLOAD            = 0x0008
  # OPT_CHUNKED_DOWNLOAD    = 0x0010 # Deprecated for now
  OPT_COMMAND             = 0x0020

  attr_reader :packet_id, :type, :session_id, :body

  class SynBody
    extend PacketHelper

    attr_reader :seq, :options, :name, :download

    def initialize(options, params = {})
      @options = options || raise(DnscatException, "options can't be nil!")
      @seq = params[:seq] || raise(DnscatException, "params[:seq] can't be nil!")

      if((@options & OPT_NAME) == OPT_NAME)
        @name = params[:name] || raise(DnscatException, "params[:name] can't be nil when OPT_NAME is set!")
      else
        @name = "(unnamed)"
      end

      if((@options & OPT_DOWNLOAD) == OPT_DOWNLOAD)
        @download = params[:download] || raise(DnscatException, "params[:download] can't be nil when OPT_DOWNLOAD is set!")
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
        name = data.unpack("Z*").pop
        data = data[(name.length+1)..-1]
      else
        name = "[unnamed]"
      end

      # Parse the download option, if it exists
      download = nil
      if((options & OPT_DOWNLOAD) == OPT_DOWNLOAD)
        if(data.index("\0").nil?)
          raise(DnscatException, "OPT_DOWNLOAD set, but no null-terminated name given")
        end
        download = data.unpack("Z*").pop
        data = data[(download.length+1)..-1]
      end

      # Verify that that was the entire packet
      if(data.length > 0)
        raise(DnscatException, "Extra data on the end of an SYN packet :: #{data.unpack("H*")}")
      end

      return SynBody.new(options, {
        :seq     => seq,
        :name    => name,
        :download => download,
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
      if((@options & OPT_DOWNLOAD) == OPT_DOWNLOAD)
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

  class NegEncBody
    extend PacketHelper

    attr_reader :flags, :public_key_x, :public_key_y

    def initialize(params = {})
      @flags        = params[:flags]        || raise(DnscatException, "params[:flags] is required!")
      @public_key_x = params[:public_key_x] || raise(DnscatException, "params[:public_key_x] is required!")
      @public_key_y = params[:public_key_y] || raise(DnscatException, "params[:public_key_y] is required!")
    end

    def NegEncBody.parse(data)
      exactly?(data, 68) || raise(DnscatException, "Packet is the wrong length (NEGENC)")
      flags, public_key_x, public_key_y, data = data.unpack("Na32a32a*")

      if(data != "")
        raise(DnscatException, "Extra data on the end of a NEGENC packet")
      end

      public_key_x = public_key_x.unpack("H*").pop().to_i(16)
      public_key_y = public_key_y.unpack("H*").pop().to_i(16)

      return NegEncBody.new({:flags => flags, :public_key_x => public_key_x, :public_key_y => public_key_y})
    end

    def to_s()
      return "[[NEGENC]] :: flags = 0x%08x, pubkey = %s %s" % [@flags, @public_key_x.to_s(16), @public_key_y.to_s(16)]
    end

    def to_bytes()
      public_key_x = [@public_key_x.to_s(16)].pack("H*")
      public_key_y = [@public_key_y.to_s(16)].pack("H*")

      return [@flags, public_key_x, public_key_y].pack("Na32a32")
    end
  end

  class AuthBody
    extend PacketHelper

    attr_reader :authenticator

    def initialize(params = {})
      @authenticator = params[:authenticator] || raise(DnscatException, "params[:authenticator] is required!")

      if(@authenticator.length != 32)
        raise(DnscatException, "params[:authenticator] was the wrong size!")
      end
    end

    def AuthBody.parse(data)
      exactly?(data, 32) || raise(DnscatException, "Packet is the wrong length (AUTH)")
      authenticator, data = data.unpack("a32a*")

      if(authenticator.length != 32)
        raise(DnscatException, "AUTH packet was too short")
      end
      if(data != "")
        raise(DnscatException, "Extra data on the end of a AUTH packet")
      end

      return AuthBody.new({:authenticator => authenticator})
    end

    def to_s()
      return "[[AUTH]] :: authenticator = %s" % [@authenticator.unpack("H*").pop()]
    end

    def to_bytes()
      return [authenticator].pack("a32")
    end
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
    elsif(type == MESSAGE_TYPE_NEGENC)
      body = NegEncBody.parse(data)
    elsif(type == MESSAGE_TYPE_AUTH)
      body = AuthBody.parse(data)
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

  def Packet.create_negenc(options, params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_NEGENC, params[:session_id], NegEncBody.new(params))
  end

  def Packet.create_auth(options, params = {})
    return Packet.new(params[:packet_id], MESSAGE_TYPE_AUTH, params[:session_id], AuthBody.new(params))
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

class EncryptedPacket
  attr_reader :nonce, :packet

  def initialize(nonce, packet)
    if(nonce < 0x0000 || nonce > 0xFFFF)
      raise(DnscatException, "Invalid nonce: #{nonce}")
    end
  end

  def EncryptedPacket.parse(data, their_mac_key, their_write_key, options = nil)
    signature, signed_data = data.unpack("a6a*")

    correct_signature = Digest::SHA3.new(256).digest(their_mac_key + signed_data)
    # TODO: I might want to change this exception so we can handle it more gracefully
    if(correct_signature[0,6] != signature)
      raise(DnscatException, "Invalid signature on message!")
    end

    # Read the nonce as a 16-bit integer
    nonce, encrypted_data = data.unpack("na*")

    # Decrypt the data
    decryptor = Salsa20.new(their_write_key, [nonce].pack("q>"))
    data = decryptor.decrypt(encrypted_data)

    return EncryptedPacket.new(nonce, Packet.parse(data, options))
  end

  def to_s()
    return "[Will be encrypted 0x%x] %s" % [@nonce, @packet.to_s]
  end

  def to_bytes(our_mac_key, our_write_key)
    encryptor = Salsa20.new(our_write_key, [@nonce].pack("q>"))
    encrypted_data = encryptor.encrypt(@packet.to_bytes())
    to_sign = [@nonce, encrypted_data].pack("na*")
    signature = Digest::SHA3.new(256).digest(our_mac_key + to_sign)

    return signature[0,6] + to_sign
  end
end


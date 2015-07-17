##
# packet.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# Builds and parses dnscat2 packets.
##

require 'dnscat_exception'

module PacketHelper
  def at_least?(data, needed)
    return (data.length >= needed)
  end
end

class Packet
  extend PacketHelper

  # Message types
  MESSAGE_TYPE_SYN        = 0x00
  MESSAGE_TYPE_MSG        = 0x01
  MESSAGE_TYPE_FIN        = 0x02
  MESSAGE_TYPE_PING       = 0xFF

  OPT_NAME                = 0x0001
  # OPT_TUNNEL              = 0x0002 # Deprecated
  # OPT_DATAGRAM            = 0x0004 # Deprecated
  OPT_DOWNLOAD            = 0x0008
  OPT_CHUNKED_DOWNLOAD    = 0x0010
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

      seq, options = data.unpack("nn")
      data = data[4..-1]

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
      result = "[[SYN]] :: isn = %04x, options = %04x" % [@seq, @options]

      if((options & OPT_DOWNLOAD) == OPT_DOWNLOAD || (options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        result += ", download = %s" % @download
      end

      return result
    end

    def to_bytes()
      return [seq, options].pack("nn")
    end
  end

  class MsgBody
    extend PacketHelper

    attr_reader :chunk, :seq, :ack, :data

    def initialize(options, params = {})
      @options = options
      if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        @chunk = params[:chunk] || raise(DnscatException, "params[:chunk] can't be nil when OPT_CHUNKED_DOWNLOAD is set!")
      else
        @seq = params[:seq] || raise(DnscatException, "params[:seq] can't be nil unless OPT_CHUNKED_DOWNLOAD is set!")
        @ack = params[:ack] || raise(DnscatException, "params[:ack] can't be nil unless OPT_CHUNKED_DOWNLOAD is set!")
      end
      @data = params[:data] || raise(DnscatException, "params[:data] can't be nil!")
    end

    def MsgBody.parse(options, data)
      if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        at_least?(data, 4) || raise(DnscatException, "Packet is too short (MSG d/l)")

        chunk = data.unpack("N").pop
        data = data[4..-1] # Remove the first eight bytes
      else
        at_least?(data, 4) || raise(DnscatException, "Packet is too short (MSG norm)")

        seq, ack = data.unpack("nn")
        data = data[4..-1] # Remove the first four bytes
      end

      return MsgBody.new(options, {
        :chunk => chunk,
        :data  => data,
        :seq   => seq,
        :ack   => ack,
        :data  => data,
      })
    end

    def MsgBody.header_size(options)
      return MsgBody.new(options, {
        :chunk => 0,
        :seq   => 0,
        :ack   => 0,
        :data  => '',
      }).to_bytes().length()
    end

#    def str_format(str)
#      result = ""
#      str.chars.each do |c|
#        if(c.ord < 0x20 || c.ord > 0x7F)
#          result += '\x%02x' % (c.ord)
#        else
#          result += c
#        end
#      end
#
#      return result
#    end

    def to_s()
      data = @data.gsub(/\n/, '\n')
      if((@options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        return "[[MSG]] :: chunk = %d, data = 0x%x bytes" % [@chunk, data.length]
      else
        return "[[MSG]] :: seq = %04x, ack = %04x, data = 0x%x bytes" % [@seq, @ack, data.length]
      end
    end

    def to_bytes()
      result = ""
      if((@options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        chunk = @chunk || 0
        result += [chunk, @data].pack("NA*")
      else
        seq = @seq || 0
        ack = @ack || 0
        result += [seq, ack, @data].pack("nnA*")
      end

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

    attr_reader :reason

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
      return "[[PING]] :: %s" % [@reason]
    end

    def to_bytes()
      [@data].pack("Z*")
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
    else
      raise(DnscatException, "Unknown message type: 0x%x", type)
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

  def to_s()
    return "[0x%04x] session = %04x :: %s" % [@packet_id, @session_id, @body.to_s]
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

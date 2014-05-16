##
# packet.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# Builds and parses dnscat2 packets.
##

require 'dnscat_exception'

class Packet
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

  attr_reader :data, :packet_id, :type, :session_id, :options, :seq, :ack
  attr_reader :name     # For SYN
  attr_reader :download # For downloads (in SYN)
  attr_reader :chunk    # For chunked downloads (in MSG)
  attr_reader :reason   # For FIN
  attr_reader :data     # For PING

  def at_least?(data, needed)
    return (data.length >= needed)
  end

  def parse_header(data)
    at_least?(data, 5) || raise(DnscatException, "Packet is too short (header)")

    # (uint16_t) packet_id
    # (uint8_t)  message_type
    # (uint16_t) session_id
    @packet_id, @type, @session_id = data.unpack("nCn")

    return data[5..-1]
  end

  def parse_syn(data)
    at_least?(data, 4) || raise(DnscatException, "Packet is too short (SYN)")
    @seq, @options = data.unpack("nn")
    data = data[4..-1]

    # Parse the option name, if it has one
    @name = nil
    if((@options & OPT_NAME) == OPT_NAME)
      if(data.index("\0").nil?)
        raise(DnscatException, "OPT_NAME set, but no null-terminated name given")
      end
      @name = data.unpack("Z*").pop
      data = data[(@name.length+1)..-1]
    else
      @name = "[unnamed]"
    end

    # Parse the download option, if it exists
    @download = nil
    if((@options & OPT_DOWNLOAD) == OPT_DOWNLOAD)
      if(data.index("\0").nil?)
        raise(DnscatException, "OPT_DOWNLOAD set, but no null-terminated name given")
      end
      @download = data.unpack("Z*").pop
      data = data[(@download.length+1)..-1]
    end

    # Verify that that was the entire packet
    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of an SYN packet :: #{data.unpack("H*")}")
    end
  end

  def parse_msg(data, options)
    if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
      at_least?(data, 4) || raise(DnscatException, "Packet is too short (MSG d/l)")

      @chunk = data.unpack("N").pop
      @data = data[4..-1] # Remove the first eight bytes
    else
      at_least?(data, 4) || raise(DnscatException, "Packet is too short (MSG norm)")

      @seq, @ack = data.unpack("nn")
      @data = data[4..-1] # Remove the first four bytes
    end
  end

  def parse_fin(data, options)
    at_least?(data, 1) || raise(DnscatException, "Packet is too short (FIN)")

    @reason = data.unpack("Z*").pop
    data = data[(@reason.length+1)..-1]

    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of a FIN packet")
    end
  end

  def parse_ping(data)
    at_least?(data, 1) || raise(DnscatException, "Packet is too short (PING)")

    @data = data.unpack("Z*").pop
    data = data[(@data.length+1)..-1]

    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of a PING packet")
    end
  end

  def initialize(data)
    # Parse the hader
    parse_header(data)
  end

  def parse_body(data, options)
    # Parse the hader
    data = parse_header(data)

    # Parse the message differently depending on what type it is
    if(@type == MESSAGE_TYPE_SYN)
      parse_syn(data)
    elsif(@type == MESSAGE_TYPE_MSG)
      parse_msg(data, options)
    elsif(@type == MESSAGE_TYPE_FIN)
      parse_fin(data, options)
    elsif(@type == MESSAGE_TYPE_PING)
      parse_ping(data)
    else
      raise(DnscatException, "Unknown message type: #{@type}")
    end
  end

  def Packet.parse_header(data)
    return Packet.new(data)
  end

  def Packet.create_header(type, session_id, packet_id = nil)
    packet_id = packet_id || rand(0xFFFF)
    return [packet_id, type, session_id].pack("nCn")
  end

  def Packet.create_syn(session_id, seq, options = 0, packet_id = nil)
    return create_header(MESSAGE_TYPE_SYN, session_id, packet_id) + [seq, options].pack("nn")
  end

  def Packet.syn_header_size()
    return create_syn(0, 0, 0, nil, 0).length
  end

  def Packet.create_msg(session_id, msg, options, option_data = {}, packet_id = nil)
    result = create_header(MESSAGE_TYPE_MSG, session_id, packet_id)
    if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
      chunk = option_data['chunk'] || 0
      result += [chunk, msg].pack("NA*")
    else
      seq = option_data['seq'] || 0
      ack = option_data['ack'] || 0
      result += [seq, ack, msg].pack("nnA*")
    end

    return result
  end

  def Packet.msg_header_size(options)
    return create_msg(0, '', options, {'seq'=>0, 'ack'=>0, 'chunk'=>0}, 0).length
  end

  def Packet.create_fin(session_id, reason, options, packet_id = nil)
    return create_header(MESSAGE_TYPE_FIN, session_id, packet_id) + [reason].pack("Z*")
  end

  def Packet.fin_header_size()
    return create_fin(0, "", 0).length
  end

  def Packet.create_ping(data, packet_id = nil)
    return create_header(MESSAGE_TYPE_PING, 0, packet_id) + [data].pack("Z*")
  end

  def to_s()
    result = nil
    if(@type == MESSAGE_TYPE_SYN)
      result = "[[SYN]] :: [0x%04x] session = %04x, seq = %04x, options = %04x" % [@packet_id, @session_id, @seq, @options]

      if((options & OPT_DOWNLOAD) == OPT_DOWNLOAD || (options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        result += ", download = %s" % @download
      end
    elsif(@type == MESSAGE_TYPE_MSG)
      data = @data.gsub(/\n/, '\n')
      if((@options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        result = "[[MSG]] :: [0x%04x] session = %04x, chunk = %d, data = \"%s\"" % [@packet_id, @session_id, @chunk, data]
      else
        result = "[[MSG]] :: [0x%04x] session = %04x, seq = %04x, ack = %04x, data = \"%s\"" % [@packet_id, @session_id, @seq, @ack, data]
      end
    elsif(@type == MESSAGE_TYPE_FIN)
      result = "[[FIN]] :: [0x%04x] session = %04x :: %s" % [@packet_id, @session_id, @reason]
    elsif(@type == MESSAGE_TYPE_PING)
      result = "[[PING]] :: [0x%04x] data = %s" % [@packet_id, @data]
    else
      raise DnscatException("Unknown packet type")
    end

    return result
  end
end

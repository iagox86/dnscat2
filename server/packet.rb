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

  OPT_NAME                = 0x01
  # OPT_TUNNEL              = 0x02 # Deprecated
  # OPT_DATAGRAM            = 0x04 # Deprecated
  OPT_DOWNLOAD            = 0x08
  OPT_CHUNKED_DOWNLOAD    = 0x10

  attr_reader :data, :type, :session_id, :options, :seq, :ack
  attr_reader :name
  attr_reader :download

  def at_least?(data, needed)
    return (data.length >= needed)
  end

  def parse_header(data)
    at_least?(data, 3) || raise(DnscatException, "Packet is too short (header)")

    # (uint8_t) message_type
    # (uint16_t) session_id
    @type, @session_id = data.unpack("Cn")

    return data[3..-1]
  end

  def parse_syn(data)
    puts("Data: #{data.unpack("H*")} (#{data.length})")

    at_least?(data, 4) || raise(DnscatException, "Packet is too short (syn)")
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
    if((@options & OPT_DOWNLOAD) == OPT_DOWNLOAD || (@options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
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
    if((@options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
      at_least?(data, 8) || raise(DnscatException, "Packet is too short (msg d/l)")

      @seq, @ack, @chunk = data.unpack("nnN")
      @data = data[8..-1] # Remove the first eight bytes
    else
      at_least?(data, 4) || raise(DnscatException, "Packet is too short (msg)")

      @seq, @ack = data.unpack("nn")
      @data = data[4..-1] # Remove the first four bytes
    end
  end

  def parse_fin(data, options)
    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of a FIN packet")
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
    else
      raise(DnscatException, "Unknown message type: #{@type}")
    end
  end

  def Packet.parse_header(data)
    return Packet.new(data)
  end

  def Packet.create_header(type, session_id)
    return [type, session_id].pack("Cn")
  end

  def Packet.create_syn(session_id, seq, options = 0)
    return create_header(MESSAGE_TYPE_SYN, session_id) + [seq, options].pack("nn")
  end

  def Packet.syn_header_size()
    return create_syn(0, 0, 0, nil).length
  end

  def Packet.create_msg(session_id, seq, ack, msg, options, option_data = {})
    if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
      chunk_size = option_data['chunk_size'] || 0
      return create_header(MESSAGE_TYPE_MSG, session_id) + [seq, ack, chunk_size, msg].pack("nnNA*")
    else
      return create_header(MESSAGE_TYPE_MSG, session_id) + [seq, ack, msg].pack("nnA*")
    end
  end

  def Packet.msg_header_size(options)
    return create_msg(0, 0, 0, "", options).length
  end

  def Packet.create_fin(session_id, options)
    return create_header(MESSAGE_TYPE_FIN, session_id)
  end

  def Packet.fin_header_size()
    return create_fin(0, 0).length
  end

  def to_s()
    result = nil
    if(@type == MESSAGE_TYPE_SYN)
      result = "[[SYN]] :: session = %04x, seq = %04x, options = %04x" % [@session_id, @seq, @options]

      if((options & OPT_DOWNLOAD) == OPT_DOWNLOAD || (options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        result += ", download = %s" % @download
      end
    elsif(@type == MESSAGE_TYPE_MSG)
      data = @data.gsub(/\n/, '\n')
      result = "[[MSG]] :: session = %04x, seq = %04x, ack = %04x, data = \"%s\"" % [@session_id, @seq, @ack, data]
      if((options & OPT_CHUNKED_DOWNLOAD) == OPT_CHUNKED_DOWNLOAD)
        result += ", chunk number/size: %d" % @chunk
      end
    elsif(@type == MESSAGE_TYPE_FIN)
      result = "[[FIN]] :: session = %04x" % [@session_id]
    end

    return result
  end
end

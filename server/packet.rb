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
  MESSAGE_TYPE_STRAIGHTUP = 0xFF

  OPT_NAME                = 0x01
  OPT_TUNNEL              = 0x02

  attr_reader :data, :type, :packet_id, :session_id, :options, :seq, :ack
  attr_reader :name
  attr_reader :tunnel_host, :tunnel_port

  def at_least?(data, needed)
    if(data.length < needed)
      raise(DnscatException, "Packet is too short")
    end
  end

  def parse_header(data)
    at_least?(data, 3)

    # (uint8_t) message_type
    # (uint16_t) packet_id
    # (uint16_t) session_id
    @type, @packet_id, @session_id = data.unpack("Cnn")

    return data[5..-1]
  end

  def parse_syn(data)
    at_least?(data, 4)
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

    @tunnel_host = nil
    @tunnel_port = nil
    if((@options & OPT_TUNNEL) == OPT_TUNNEL)
      @tunnel_host, @tunnel = data.unpack("Z*n")
    end

    # Verify that that was the entire packet
    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of an SYN packet")
    end
  end

  def parse_msg(data)
    @seq, @ack = data.unpack("nn")
    @data = data[4..-1] # Remove the first four bytes
  end

  def parse_fin(data)
    if(data.length > 0)
      raise(DnscatException, "Extra data on the end of a FIN packet")
    end
  end

  def parse_straightup(data)
    raise(Exception, "Not implemented yet")
  end

  def initialize(data)
    # Parse the hader
    data = parse_header(data)

    # Parse the message differently depending on what type it is
    if(@type == MESSAGE_TYPE_SYN)
      parse_syn(data)
    elsif(@type == MESSAGE_TYPE_MSG)
      parse_msg(data)
    elsif(@type == MESSAGE_TYPE_FIN)
      parse_fin(data)
    elsif(@type == MESSAGE_TYPE_STRAIGHTUP) # TODO
      parse_straightup(data)
    else
      raise(DnscatException, "Unknown message type: #{@type}")
    end
  end

  def Packet.parse(data)
    return Packet.new(data)
  end

  def Packet.create_header(type, packet_id, session_id)
    return [type, packet_id, session_id].pack("Cnn")
  end

  def Packet.create_syn(packet_id, session_id, seq, options = nil)
    options = options.nil? ? 0 : options
    return create_header(MESSAGE_TYPE_SYN, packet_id, session_id) + [seq, options].pack("nn")
  end

  def Packet.syn_header_size()
    return create_syn(0, 0, 0, nil).length
  end

  def Packet.create_msg(packet_id, session_id, seq, ack, msg)
    return create_header(MESSAGE_TYPE_MSG, packet_id, session_id) + [seq, ack, msg].pack("nnA*")
  end

  def Packet.msg_header_size()
    return create_msg(0, 0, 0, 0, "").length
  end

  def Packet.create_fin(packet_id, session_id)
    return create_header(MESSAGE_TYPE_FIN, packet_id, session_id)
  end

  def Packet.fin_header_size()
    return create_fin(0, 0).length
  end

  def to_s()
    if(@type == MESSAGE_TYPE_SYN)
      return "[[SYN]] :: packet_id = %04x, session = %04x, seq = %04x, options = %04x" % [@packet_id, @session_id, @seq, @options]
    elsif(@type == MESSAGE_TYPE_MSG)
      data = @data.gsub(/\n/, '\n')
      return "[[MSG]] :: packet_id = %04x, session = %04x, seq = %04x, ack = %04x, data = \"%s\"" % [@packet_id, @session_id, @seq, @ack, data]
    elsif(@type == MESSAGE_TYPE_FIN)
      return "[[FIN]] :: packet_id = %04x, session = %04x" % [@packet_id, @session_id]
    end
  end
end

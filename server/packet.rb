class Packet
  # Message types
  MESSAGE_TYPE_SYN        = 0x00
  MESSAGE_TYPE_MSG        = 0x01
  MESSAGE_TYPE_FIN        = 0x02
  MESSAGE_TYPE_STRAIGHTUP = 0xFF

  attr_reader :data, :type, :session_id, :options, :seq, :ack

  def at_least?(data, needed)
    if(data.length < needed)
      raise(IOError, "Packet is too short")
    end
  end

  def parse_header(data)
    at_least?(data, 3)

    # (uint8_t) message_type
    # (uint16_t) session_id
    @type, @session_id = data.unpack("Cn")

    return data[3..-1]
  end

  def parse_syn(data)
    at_least?(data, 4)
    @options, @seq = data.unpack("nn")
    data = data[4..-1]

    # Verify that that was the entire packet
    if(data.length > 0)
      raise(IOError, "Extra data on the end of an SYN packet")
    end
  end

  def parse_msg(data)
    @seq, @ack = data.unpack("nn")
    @data = data[4..-1] # Remove the first four bytes
  end

  def parse_fin(data)
    if(data.length > 0)
      raise(IOError, "Extra data on the end of a FIN packet")
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
      raise(IOError, "Unknown message type: #{parsed[:type]}")
    end
  end

  def Packet.parse(data)
    return Packet.new(data)
  end

  def Packet.create_header(type, session_id)
    return [type, session_id].pack("Cn")
  end

  def Packet.create_syn(session_id, seq, options = nil)
    options = options.nil? ? 0 : options
    return create_header(MESSAGE_TYPE_SYN, session_id) + [seq, options].pack("nn")
  end

  def Packet.syn_header_size()
    return create_syn(0, 0, nil).length
  end

  def Packet.create_msg(session_id, seq, ack, msg)
    return create_header(MESSAGE_TYPE_MSG, session_id) + [seq, ack, msg].pack("nnA*")
  end

  def Packet.msg_header_size()
    return create_msg(0, 0, 0, "").length
  end

  def Packet.create_fin(session_id)
    return create_header(MESSAGE_TYPE_FIN, session_id)
  end

  def Packet.fin_header_size()
    return create_fin(0).length
  end
end

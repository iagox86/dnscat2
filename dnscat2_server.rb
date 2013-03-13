require 'socket'

require 'session'

# Message types
MESSAGE_TYPE_SYN        = 0x00
MESSAGE_TYPE_MSG        = 0x01
MESSAGE_TYPE_FIN        = 0x02
MESSAGE_TYPE_STRAIGHTUP = 0xFF

def get_socket_string(s)
  return "#{s.peeraddr[3]}:#{s.peeraddr[1]} (#{s.peeraddr[2]})"
end

def receive_packet(s)
  length = s.read(2)

  if(length.nil?)
    raise(IOError, "Connection closed while reading length")
  end

  length = length.unpack("n").first
  if(length <= 0 || length > 65535)
    raise(IOError, "Invalid length value received")
  end

  data = s.read(length)
  if(data.length != length)
    raise(IOError, "Connection closed while reading data")
  end

  puts("Received #{data.length} bytes!")

  return data
end

def get_session(id)
end

def parse_packet(raw)
  parsed = {}

  # Get the session_id value
  parsed[:type], parsed[:session_id] = raw.unpack("Cn")
  raw = raw[3..-1] # Remove the first three bytes

  # Parse the message differently depending on what type it is
  if(parsed[:type] == MESSAGE_TYPE_SYN)
    puts("Received a SYN")

    parsed[:options], parsed[:seq] = raw.unpack("nn")
    raw = raw[4..-1] # Remove the first four bytes

    # Verify that that was the entire packet
    if(raw.length > 0)
      raise(IOError, "Extra data on the end of an SYN packet")
    end

  elsif(parsed[:type] == MESSAGE_TYPE_MSG)
    puts("Received a MSG")

    parsed[:seq], parsed[:ack] = raw.unpack("nn")
    raw = raw[4..-1] # Remove the first four bytes

  elsif(parsed[:type] == MESSAGE_TYPE_FIN)
    puts("Received a FIN")

    if(raw.length > 0)
      raise(IOError, "Extra data on the end of a FIN packet")
    end

  elsif(parsed[:type] == MESSAGE_TYPE_STRAIGHTUP) # TODO
    puts("Received a STRAIGHTUP")

    raise(Exception, "Not implemented yet")
  else
    raise(Exception, "Unknown message type: #{parsed[:type]}")
  end

  parsed[:data] = raw

  return parsed
end

def handle_syn(packet, session)
  if(!session.is_syn_valid())
    raise(IOError, "SYN is invalid in this state")
  end

  # TODO: Validate packet
  # TODO: Set initial seq
  # TODO: Send them my initial seq
end

def handle_msg(packet, session)
  if(!session.is_msg_valid())
    raise(IOError, "MSG is invalid in this state")
  end

  # TODO: Validate the seq
  # TODO: Update their seq
  # TODO: Display the message, if any
  # TODO: ACK the message
end

def handle_fin(packet, session)
  if(!session.is_fin_valid())
    raise(IOError, "FIN is invalid in this state")
  end

  session.destroy()
end

def go_tcp(s)
  session_id = nil # Save this so we can recover, possibly
  begin
    loop do
      raw_packet = receive_packet(s)
      packet = parse_packet(raw_packet)
      session = Session.find(packet[:session_id])

      puts(packet.inspect)

      if(packet[:type] == MESSAGE_TYPE_SYN)
        handle_syn(packet, session)
      elsif(packet[:type] == MESSAGE_TYPE_MSG)
        handle_msg(packet, session)
      elsif(packet[:type] == MESSAGE_TYPE_FIN)
        handle_fin(packet, session)
      else
        raise(IOError, "Unknown packet type: #{packet[:type]}")
      end

      puts("Done!")
      exit
    end
  rescue IOError => e
    puts("Exception: #{e}")

    if(!session_id.nil?)
      Session.destroy(session_id)
    end

    puts(e.inspect)
    puts(e.backtrace)
  end

  s.close()
end

server = TCPServer.new(2000)
loop do
  Thread.start(server.accept) do |s|
    begin
      puts("Received a connection from #{get_socket_string(s)}")
      go_tcp(s)
    rescue Exception => e
      puts(e.inspect)
      puts(e.backtrace)
    end
  end
end

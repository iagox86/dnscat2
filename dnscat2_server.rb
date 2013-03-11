require 'socket'

require 'socket'

# Message types
MESSAGE_TYPE_SYN        = 0x00
MESSAGE_TYPE_MSG        = 0x01
MESSAGE_TYPE_FIN        = 0x02
MESSAGE_TYPE_STRAIGHTUP = 0xFF

# Session states [TODO: Do I actually need these?]
SESSION_STATE_NEW      = 0x00
SESSION_STATE_ACTIVE   = 0x01
SESSION_STATE_FINISHED = 0x02

@@sessions = {}

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
  # Get or create the session
  session = @@sessions[id]

  if(session.nil?)
    session = {}
    session[:state] = SESSION_STATE_NEW
    @@sessions[id] = session
  end

  return session
end

def parse_packet(raw)
  parsed = {}

  if(raw.length < 4)
    raise(IOError, "Packet is too short")
  end

  # Get the session_id value
  parsed[:message_type], parsed[:session_id] = raw.unpack("Cn")
  raw = raw[3..-1] # Remove the first three bytes

  # Parse the message differently depending on what type it is
  if(parsed[:message_type] == MESSAGE_TYPE_SYN)
    puts("Received a SYN")

    parsed[:options], parsed[:seq] = raw.unpack("nn")
    raw = raw[4..-1] # Remove the first four bytes

    # Verify that that was the entire packet
    if(raw.length > 0)
      raise(IOError, "Extra data on the end of an SYN packet")
    end

  elsif(parsed[:message_type] == MESSAGE_TYPE_MSG)
    puts("Received a MSG")

    parsed[:seq], parsed[:ack] = raw.unpack("nn")
    raw = raw[4..-1] # Remove the first four bytes

  elsif(parsed[:message_type] == MESSAGE_TYPE_FIN)
    puts("Received a FIN")

    if(raw.length > 0)
      raise(IOError, "Extra data on the end of a FIN packet")
    end

  elsif(parsed[:message_type] == MESSAGE_TYPE_STRAIGHTUP) # TODO
    puts("Received a STRAIGHTUP")

    raise(Exception, "Not implemented yet")
  else
    raise(Exception, "Unknown message type: #{parsed[:message_type]}")
  end

  parsed[:data] = raw

  return parsed
end

def verify_seq(session, packet)
  if(packet[:is_stream] && packet[:seq] != session[:seq])
    puts("Unexpected sequence number: got #{packet[:seq]}, expected #{session[:seq]}")
    return false
  end

  return true
end

def update_seq(session, packet)
  session[:seq] += packet[:data].length
end

def destroy_session(id)
  # TODO
end

def go_tcp(s)
  session_id = nil # Save this so we can recover, possibly
  begin
    loop do
      raw_packet = receive_packet(s)
      packet = parse_packet(raw_packet)
      session_id = packet[:session_id]
      session = get_session(packet[:id])

      if(packet[:type] == MESSAGE_TYPE_SYN)
        # handle_syn() # TODO
      elsif(packet[:type] == MESSAGE_TYPE_MSG)
        # handle_msg() # TODO
      elsif(packet[:type] == MESSAGE_TYPE_FIN)
        # handle_fin() # TODO
      end
    end

  rescue IOError => e
    puts("Exception: #{e}")
    if(!session_id.nil?)
      puts("Destroying session #{session_id}...")
      destroy_session(session_id)
    end
  end

  s.close()
end

server = TCPServer.new(2000)
loop do
  Thread.start(server.accept) do |s|
    begin
      go_tcp(s)
    rescue Exception => e
      puts("Exception: #{e}")
    end
  end
end

require 'socket'

require 'socket'

# Flags
FLAG_STREAM = 1

@@sessions = {}

def receive_packet(s)
  length = s.read(2).unpack("n").first
  if(length <= 0 || length > 65535)
    raise(IOError, "Invalid length value received")
  end

  data = s.read(length)
  if(data.length != length)
    raise(IOError, "Connection closed")
  end

  puts("Received #{data.length} bytes!")

  return data
end

def parse_packet(raw)
  parsed = {}

  if(raw.length < 4)
    raise(IOError, "Packet is too short")
  end

  parsed[:flags], parsed[:session_id] = raw.unpack("nn")
  raw = raw[4..-1] # Remove the first four characters

  if((parsed[:flags] & FLAG_STREAM) != 0)
    if(raw.length < 4)
      raise(IOError, "Packet is too short")
    end

    parsed[:stream] = true
    parsed[:seq], parsed[:ack] = raw.unpack("nn")
    raw = raw[4..-1]
  end

  parsed[:data] = raw

  return parsed
end

def verify_seq(packet)

end

def go_tcp(s)
  puts("go()...")
  begin
    raw_packet = receive_packet(s)
    packet = parse_packet(raw_packet)

    # Get or create the session
    session = @@sessions[packet[:session_id]] || {}
    @@sessions[packet[:session_id]] = session

    if(packet[:stream])
      verify_seq(packet)
    end

    puts(packet.inspect)
  rescue IOError => e
    puts("Exception: #{e}")
  end

  s.close
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
